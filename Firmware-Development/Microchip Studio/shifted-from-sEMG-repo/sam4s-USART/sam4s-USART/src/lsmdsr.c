/*
 * lsmdsr.c
 *
 * Created: 4/17/2024 11:19:47 AM
 *  Author: classicComputer
 */ 

#include "lsm6dsr.h"

#include "pmc.h"
#include "pio.h"
#include "sysclk.h"

/* Function Prototypes */
static uint8_t Get_G_Sensitivity(float *Sensitivity);
static uint8_t Get_X_Sensitivity(float *Sensitivity);

static void twiBegin(uint8_t devAddress);
static void imuSetCallback(void);
static uint8_t imuBegin(LSM6DSR* LSM);
static uint8_t imuEnable_X(LSM6DSR* LSM);
static uint8_t imuEnable_G(LSM6DSR* LSM);
static void imuReadID(uint8_t *Id);
static uint8_t Get_G_Axes(int32_t *AngularRate);
static uint8_t Get_X_Axes(int32_t *Acceleration);


/************************************************************************/
/*							IMU and TWI Function						*/
/************************************************************************/
static uint8_t imuEnable_X(LSM6DSR* LSM)
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

static uint8_t imuEnable_G(LSM6DSR* LSM)
{
	/* Check if the component is already enabled */
	if (LSM->gyro_is_enabled == 1U)
	{
		//println("gyro is already enabled!");
		return LSM6DSR_OK;
	}

	/* Output data rate selection. */
	if (lsm6dsr_gy_data_rate_set(&reg_ctx, LSM->gyro_odr) != LSM6DSR_OK)
	{
		//println("gyro data-rate not set!");
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

static uint8_t Get_G_Sensitivity(float *Sensitivity)
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

static uint8_t Get_X_Axes(int32_t *Acceleration)
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

static uint8_t Get_X_Sensitivity(float *Sensitivity)
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

static void twiBegin(uint8_t devAddress)
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

static void imuSetCallback(void)
{
	reg_ctx.write_reg = LSM6DSR_io_write;
	reg_ctx.read_reg = LSM6DSR_io_read;
	reg_ctx.handle	=	(void *)NULL;	
}

static void imuReadID(uint8_t *Id)
{
	if (lsm6dsr_device_id_get(&reg_ctx, Id) == LSM6DSR_OK)
	{
		char temp[10];
		sprintf(temp, "%d", *Id);
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

static uint8_t imuBegin(LSM6DSR* LSM)
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

/* Public Functions */
void initializeImu(void)
{
	twiBegin( (uint8_t)IMU_BUS_ADDR );
	imuSetCallback();
	imuReadID(&data_received);
		
	LSM6DSR sensor = {0};
	imuBegin(&sensor);
	imuEnable_X(&sensor);
	imuEnable_G(&sensor);
}

void getImuData(imuData_t* imuData)
{
	Get_X_Axes(imuData->accelerometer);
	Get_G_Axes(imuData->gyroscope);
}