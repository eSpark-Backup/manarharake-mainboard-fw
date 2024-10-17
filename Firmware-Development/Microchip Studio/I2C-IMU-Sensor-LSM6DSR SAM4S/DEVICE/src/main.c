#include <asf.h>
#include "conf_usb.h"
#include "ui.h"
#include "uart.h"
#include <string.h>
#include "lsm6dsr_reg.h"
#include "twi.h"

static volatile bool main_b_cdc_enable = false;
void println(const char* str);
void receivetransmit(void);

#define LSM6DSR_ACC_SENSITIVITY_FS_2G   0.061f
#define LSM6DSR_ACC_SENSITIVITY_FS_4G   0.122f
#define LSM6DSR_ACC_SENSITIVITY_FS_8G   0.244f
#define LSM6DSR_ACC_SENSITIVITY_FS_16G  0.488f

#define LSM6DSR_GYRO_SENSITIVITY_FS_125DPS    4.370f
#define LSM6DSR_GYRO_SENSITIVITY_FS_250DPS    8.750f
#define LSM6DSR_GYRO_SENSITIVITY_FS_500DPS   17.500f
#define LSM6DSR_GYRO_SENSITIVITY_FS_1000DPS  35.000f
#define LSM6DSR_GYRO_SENSITIVITY_FS_2000DPS  70.000f
#define LSM6DSR_GYRO_SENSITIVITY_FS_4000DPS 140.000f

typedef struct
{
	lsm6dsr_odr_xl_t acc_odr;
	lsm6dsr_odr_g_t gyro_odr;
	
	uint8_t acc_is_enabled;
	uint8_t gyro_is_enabled;	
}LSM6DSR;

typedef enum
{
	LSM6DSR_OK = 0,
	LSM6DSR_ERROR = 1
} LSM6DSRStatusTypeDef;

/**** TWI Parameters ****/
#define TWI_SPEED			400000
#define BOARD_ID_TWI_IMU    ID_TWI0
#define BOARD_BASE_TWI_IMU  TWI0
uint8_t data_received;
uint8_t data_transmitted;
/**** TWI Parameters ****/

/** LSM6DSR Registers **/
#define IMU_BUS_ADDR	0x6B // 0x6B (based on SDO/SA0 pin)
#define IMU_WHO_AM_I	0x0F
#define STATUS_REG		0x1E
lsm6dsr_ctx_t reg_ctx;
/** LSM6DSR Registers **/

/** TWI & LSM6DSR Functions **/
int32_t LSM6DSR_io_read(void *handle, uint8_t ReadAddr, uint8_t *pBuffer, uint16_t nBytesToRead);
int32_t LSM6DSR_io_write(void *handle, uint8_t WriteAddr, uint8_t *pBuffer, uint16_t nBytesToWrite);

void twiBegin(uint8_t devAddress);
void imuSetCallback(void);
uint8_t imuBegin(LSM6DSR* LSM);
void imuReadID(uint8_t *Id);

uint8_t imuEnable_G(LSM6DSR* LSM);
uint8_t Get_G_Axes(int32_t *AngularRate);
uint8_t Get_G_Sensitivity(float *Sensitivity);

uint8_t imuEnable_X(LSM6DSR* LSM);
uint8_t Get_X_Sensitivity(float *Sensitivity);
uint8_t Get_X_Axes(int32_t *Acceleration);
/** TWI & LSM6DSR Functions **/

int main(void)
{
	irq_initialize_vectors();
	cpu_irq_enable();

	// Initialize the sleep manager
	sleepmgr_init();

#if !SAM0
	sysclk_init();
	board_init();
#else
	system_init();
#endif
	ui_init();
	ui_powerdown();

	// Start USB stack to authorize VBus monitoring
	udc_start();
	delay_ms(5000);
	
	twiBegin( (uint8_t)IMU_BUS_ADDR );
	imuSetCallback();
	imuReadID(&data_received);
	
	LSM6DSR sensor;
	imuBegin(&sensor);
	imuEnable_X(&sensor);	
	imuEnable_G(&sensor);
	
	char val[20];	
	int32_t accelerometer[3];
	int32_t gyroscope[3];
	
	
	while (true) 
	{
		
		Get_X_Axes(accelerometer);
		println("Accelerometer");
		sprintf(val, "%d | %d | %d", accelerometer[0], accelerometer[1], accelerometer[2]);
		println(val);

		Get_G_Axes(gyroscope);
		println("Gyroscope");
		sprintf(val, "%d | %d | %d", gyroscope[0], gyroscope[1], gyroscope[2]);
		println(val);
		delay_ms(1000);
		
	}
}

uint8_t imuEnable_X(LSM6DSR* LSM)
{
	/* Check if the component is already enabled */
	if (LSM->acc_is_enabled == 1U)
	{
		return LSM6DSR_OK;
	}

	/* Output data rate selection. */
	if (lsm6dsr_xl_data_rate_set(&reg_ctx, LSM->acc_odr) != LSM6DSR_OK)
	{
		return LSM6DSR_ERROR;
	}

	LSM->acc_is_enabled = 1;

	return LSM6DSR_OK;
}

uint8_t imuEnable_G(LSM6DSR* LSM)
{
	/* Check if the component is already enabled */
	if (LSM->gyro_is_enabled == 1U)
	{
		println("gyro is already enabled!");
		return LSM6DSR_OK;
	}

	/* Output data rate selection. */
	if (lsm6dsr_gy_data_rate_set(&reg_ctx, LSM->gyro_odr) != LSM6DSR_OK)
	{
		println("gyro data-rate not set!");
		return LSM6DSR_ERROR;
	}

	LSM->gyro_is_enabled = 1;

	return LSM6DSR_OK;
}

uint8_t Get_G_Axes(int32_t *AngularRate)
{
	axis3bit16_t data_raw;
	float sensitivity;

	/* Read raw data values. */
	if (lsm6dsr_angular_rate_raw_get(&reg_ctx, data_raw.u8bit) != LSM6DSR_OK)
	{
		return LSM6DSR_ERROR;
	}

	/* Get LSM6DSR actual sensitivity. */
	if (Get_G_Sensitivity(&sensitivity) != LSM6DSR_OK)
	{
		return LSM6DSR_ERROR;
	}

	/* Calculate the data. */
	AngularRate[0] = (int32_t)((float)((float)data_raw.i16bit[0] * sensitivity));
	AngularRate[1] = (int32_t)((float)((float)data_raw.i16bit[1] * sensitivity));
	AngularRate[2] = (int32_t)((float)((float)data_raw.i16bit[2] * sensitivity));

	return LSM6DSR_OK;
}

uint8_t Get_G_Sensitivity(float *Sensitivity)
{
	LSM6DSRStatusTypeDef ret = LSM6DSR_OK;
	lsm6dsr_fs_g_t full_scale;

	/* Read actual full scale selection from sensor. */
	if (lsm6dsr_gy_full_scale_get(&reg_ctx, &full_scale) != LSM6DSR_OK)
	{
		return LSM6DSR_ERROR;
	}

	/* Store the sensitivity based on actual full scale. */
	switch (full_scale)
	{
		case LSM6DSR_125dps:
		*Sensitivity = LSM6DSR_GYRO_SENSITIVITY_FS_125DPS;
		break;

		case LSM6DSR_250dps:
		*Sensitivity = LSM6DSR_GYRO_SENSITIVITY_FS_250DPS;
		break;

		case LSM6DSR_500dps:
		*Sensitivity = LSM6DSR_GYRO_SENSITIVITY_FS_500DPS;
		break;

		case LSM6DSR_1000dps:
		*Sensitivity = LSM6DSR_GYRO_SENSITIVITY_FS_1000DPS;
		break;

		case LSM6DSR_2000dps:
		*Sensitivity = LSM6DSR_GYRO_SENSITIVITY_FS_2000DPS;
		break;

		case LSM6DSR_4000dps:
		*Sensitivity = LSM6DSR_GYRO_SENSITIVITY_FS_4000DPS;
		break;

		default:
		ret = LSM6DSR_ERROR;
		break;
	}

	return ret;
}

uint8_t Get_X_Axes(int32_t *Acceleration)
{
	axis3bit16_t data_raw;
	float sensitivity = 0.0f;

	/* Read raw data values. */
	if (lsm6dsr_acceleration_raw_get(&reg_ctx, data_raw.u8bit) != LSM6DSR_OK)
	{
		return LSM6DSR_ERROR;
	}

	/* Get LSM6DSR actual sensitivity. */
	if (Get_X_Sensitivity(&sensitivity) != LSM6DSR_OK)
	{
		return LSM6DSR_ERROR;
	}
	/* Calculate the data. */
	Acceleration[0] = (int32_t)((float)((float)data_raw.i16bit[0] * sensitivity));
	Acceleration[1] = (int32_t)((float)((float)data_raw.i16bit[1] * sensitivity));
	Acceleration[2] = (int32_t)((float)((float)data_raw.i16bit[2] * sensitivity));
	
	return LSM6DSR_OK;
}

uint8_t Get_X_Sensitivity(float *Sensitivity)
{
	LSM6DSRStatusTypeDef ret = LSM6DSR_OK;
	lsm6dsr_fs_xl_t full_scale;

	/* Read actual full scale selection from sensor. */
	if (lsm6dsr_xl_full_scale_get(&reg_ctx, &full_scale) != LSM6DSR_OK)
	{
		return LSM6DSR_ERROR;
	}

	/* Store the Sensitivity based on actual full scale. */
	switch (full_scale)
	{
		case LSM6DSR_2g:
		*Sensitivity = LSM6DSR_ACC_SENSITIVITY_FS_2G;
		break;

		case LSM6DSR_4g:
		*Sensitivity = LSM6DSR_ACC_SENSITIVITY_FS_4G;
		break;

		case LSM6DSR_8g:
		*Sensitivity = LSM6DSR_ACC_SENSITIVITY_FS_8G;
		break;

		case LSM6DSR_16g:
		*Sensitivity = LSM6DSR_ACC_SENSITIVITY_FS_16G;
		break;

		default:
		ret = LSM6DSR_ERROR;
		break;
	}

	return ret;
}

/* ******* TWI Begin ******** 
	Enables the I2C peripheral.
	
	@para 
		devAddress : The bus address of the I2C Device
		
*/	

void twiBegin(uint8_t devAddress)
{
	pmc_enable_periph_clk(ID_PIOA);
	pio_set_peripheral(PIOA, PIO_PERIPH_A, PIO_PA3);
	pio_set_peripheral(PIOA, PIO_PERIPH_A, PIO_PA4);
	
	twi_options_t opt;
	/* Enable the peripheral clock for TWI */
	pmc_enable_periph_clk(BOARD_ID_TWI_IMU);
	
	/* Configure the options of TWI driver */
	opt.master_clk	= sysclk_get_peripheral_hz();
	opt.chip		= devAddress;
	opt.speed		= TWI_SPEED;
	opt.smbus		= 0;
	
	twi_master_init(BOARD_BASE_TWI_IMU, &opt);
}

void imuSetCallback(void)
{
	reg_ctx.write_reg = LSM6DSR_io_write;
	reg_ctx.read_reg = LSM6DSR_io_read;
	reg_ctx.handle	=	(void *)NULL;	
}

void imuReadID(uint8_t *Id)
{
	if (lsm6dsr_device_id_get(&reg_ctx, Id) == LSM6DSR_OK)
	{
		println("Reading WHO_AM_I Register");
		println("packet_recieved : ");
		char temp[10];
		sprintf(temp, "%d", *Id);
		println(temp);	
	}
}


int32_t LSM6DSR_io_read(void *handle, uint8_t ReadAddr, uint8_t *pBuffer, uint16_t nBytesToRead)
{
	twi_packet_t packet_received = {
		.chip		  = IMU_BUS_ADDR,
		.addr[0]      = ReadAddr,      // TWI slave memory address data MSB
		.addr_length  = 1,     // TWI slave memory address data sizes
		.buffer       = (void *)pBuffer,        // transfer data destination buffer
		.length       = nBytesToRead   // transfer data size (bytes)
	};
	return  ( twi_master_read(BOARD_BASE_TWI_IMU, &packet_received) );
}

int32_t LSM6DSR_io_write(void *handle, uint8_t WriteAddr, uint8_t *pBuffer, uint16_t nBytesToWrite)
{
	twi_packet_t packet_transmit = {
		.chip		  = IMU_BUS_ADDR,
		.addr[0]      = WriteAddr,      // TWI slave memory address data MSB
		.addr_length  = 1,     // TWI slave memory address data sizes
		.buffer       = (void *)pBuffer,        // transfer data destination buffer
		.length       = nBytesToWrite   // transfer data size (bytes)
	};
	return ( twi_master_write(BOARD_BASE_TWI_IMU, &packet_transmit) );
}

uint8_t imuBegin(LSM6DSR* LSM)
{
	 /* Disable I3C */
  if (lsm6dsr_i3c_disable_set(&reg_ctx, LSM6DSR_I3C_DISABLE) != LSM6DSR_OK)
  {
    return LSM6DSR_ERROR;
  }

  /* Enable register address automatically incremented during a multiple byte
  access with a serial interface. */
  if (lsm6dsr_auto_increment_set(&reg_ctx, PROPERTY_ENABLE) != LSM6DSR_OK)
  {
    return LSM6DSR_ERROR;
  }

  /* Enable BDU */
  if (lsm6dsr_block_data_update_set(&reg_ctx, PROPERTY_ENABLE) != LSM6DSR_OK)
  {
    return LSM6DSR_ERROR;
  }

  /* FIFO mode selection */
  if (lsm6dsr_fifo_mode_set(&reg_ctx, LSM6DSR_BYPASS_MODE) != LSM6DSR_OK)
  {
    return LSM6DSR_ERROR;
  }

  /* Select default output data rate. */
  LSM->acc_odr = LSM6DSR_XL_ODR_104Hz;

  /* Output data rate selection - power down. */
  if (lsm6dsr_xl_data_rate_set(&reg_ctx, LSM6DSR_XL_ODR_OFF) != LSM6DSR_OK)
  {
    return LSM6DSR_ERROR;
  }

  /* Full scale selection. */
  if (lsm6dsr_xl_full_scale_set(&reg_ctx, LSM6DSR_2g) != LSM6DSR_OK)
  {
    return LSM6DSR_ERROR;
  }

  /* Select default output data rate. */
  LSM->gyro_odr = LSM6DSR_GY_ODR_104Hz;

  /* Output data rate selection - power down. */
  if (lsm6dsr_gy_data_rate_set(&reg_ctx, LSM6DSR_GY_ODR_OFF) != LSM6DSR_OK)
  {
    return LSM6DSR_ERROR;
  }

  /* Full scale selection. */
  if (lsm6dsr_gy_full_scale_set(&reg_ctx, LSM6DSR_2000dps) != LSM6DSR_OK)
  {
    return LSM6DSR_ERROR;
  }
  
  LSM->acc_is_enabled = 0;
  LSM->gyro_is_enabled = 0;

  return LSM6DSR_OK;
}

void println(const char* str)
{
	udi_cdc_write_buf(str,strlen(str));
	char ch = '\r';
	udi_cdc_putc(ch);
	ch = '\n';
	udi_cdc_putc(ch);
}

void cdc_received(uint8_t port)
{
	char value;
	value = udi_cdc_getc();
	println("Character Received : ");
	println(&value);
}

void main_suspend_action(void)
{
	ui_powerdown();
}

void main_resume_action(void)
{
	ui_wakeup();
}

void main_sof_action(void)
{
	if (!main_b_cdc_enable)
		return;
	ui_process(udd_get_frame_number());
}

#ifdef USB_DEVICE_LPM_SUPPORT
void main_suspend_lpm_action(void)
{
	ui_powerdown();
}

void main_remotewakeup_lpm_disable(void)
{
	ui_wakeup_disable();
}

void main_remotewakeup_lpm_enable(void)
{
	ui_wakeup_enable();
}
#endif

bool main_cdc_enable(uint8_t port)
{
	main_b_cdc_enable = true;
	// Open communication
	uart_open(port);
	return true;
}

void main_cdc_disable(uint8_t port)
{
	main_b_cdc_enable = false;
	// Close communication
	uart_close(port);
}

void main_cdc_set_dtr(uint8_t port, bool b_enable)
{
	if (b_enable) {
		// Host terminal has open COM
		ui_com_open(port);
	}else{
		// Host terminal has close COM
		ui_com_close(port);
	}
}

/**
 * \mainpage ASF USB Device CDC
 *
 * \section intro Introduction
 * This example shows how to implement a USB Device CDC
 * on Atmel MCU with USB module.
 * The application note AVR4907 http://ww1.microchip.com/downloads/en/appnotes/doc8447.pdf
 * provides more information about this implementation.
 *
 * \section desc Description of the Communication Device Class (CDC)
 * The Communication Device Class (CDC) is a general-purpose way to enable all
 * types of communications on the Universal Serial Bus (USB).
 * This class makes it possible to connect communication devices such as
 * digital telephones or analog modems, as well as networking devices
 * like ADSL or Cable modems.
 * While a CDC device enables the implementation of quite complex devices,
 * it can also be used as a very simple method for communication on the USB.
 * For example, a CDC device can appear as a virtual COM port, which greatly
 * simplifies application development on the host side.
 *
 * \section startup Startup
 * The example is a bridge between a USART from the main MCU
 * and the USB CDC interface.
 *
 * In this example, we will use a PC as a USB host:
 * it connects to the USB and to the USART board connector.
 * - Connect the USART peripheral to the USART interface of the board.
 * - Connect the application to a USB host (e.g. a PC)
 *   with a mini-B (embedded side) to A (PC host side) cable.
 * The application will behave as a virtual COM (see Windows Device Manager).
 * - Open a HyperTerminal on both COM ports (RS232 and Virtual COM)
 * - Select the same configuration for both COM ports up to 115200 baud.
 * - Type a character in one HyperTerminal and it will echo in the other.
 *
 * \note
 * On the first connection of the board on the PC,
 * the operating system will detect a new peripheral:
 * - This will open a new hardware installation window.
 * - Choose "No, not this time" to connect to Windows Update for this installation
 * - click "Next"
 * - When requested by Windows for a driver INF file, select the
 *   atmel_devices_cdc.inf file in the directory indicated in the Atmel Studio
 *   "Solution Explorer" window.
 * - click "Next"
 *
 * \copydoc UI
 *
 * \section example About example
 *
 * The example uses the following module groups:
 * - Basic modules:
 *   Startup, board, clock, interrupt, power management
 * - USB Device stack and CDC modules:
 *   <br>services/usb/
 *   <br>services/usb/udc/
 *   <br>services/usb/class/cdc/
 * - Specific implementation:
 *    - main.c,
 *      <br>initializes clock
 *      <br>initializes interrupt
 *      <br>manages UI
 *      <br>
 *    - uart_xmega.c,
 *      <br>implementation of RS232 bridge for XMEGA parts
 *    - uart_uc3.c,
 *      <br>implementation of RS232 bridge for UC3 parts
 *    - uart_sam.c,
 *      <br>implementation of RS232 bridge for SAM parts
 *    - specific implementation for each target "./examples/product_board/":
 *       - conf_foo.h   configuration of each module
 *       - ui.c        implement of user's interface (leds,buttons...)
 */
