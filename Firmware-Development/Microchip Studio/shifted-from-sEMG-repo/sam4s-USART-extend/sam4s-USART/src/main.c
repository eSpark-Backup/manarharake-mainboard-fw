/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# "Insert system clock initialization code here" comment
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */
#include <asf.h>
#include "string.h"
#include "lsm6dsr_reg.h"

#include "lsm6dsr.h"
#include "adc_serivce.h"
#include "usart_service.h"

#define JSON_PACKET_TEMPLATE "{\"ADC\":{\"Value_1\":%d,\"Value_2\":%d,\"Value_3\":%d,\"Value_4\":%d,\"Value_5\":%d,\"Value_6\":%d,\"Value_7\":%d,\"Value_8\":%d},\"Gyro\":{\"X\":%d,\"Y\":%d,\"Z\":%d},\"Acc\":{\"X\":%d,\"Y\":%d,\"Z\":%d}}\r\n"
#define JSON_PACKET_TEMPLATE_NO_IMU "{\"ADC\":{\"Value_1\":%d,\"Value_2\":%d,\"Value_3\":%d,\"Value_4\":%d,\"Value_5\":%d,\"Value_6\":%d,\"Value_7\":%d,\"Value_8\":%d},\"Gyro\":{\"X\":0,\"Y\":0,\"Z\":0},\"Acc\":{\"X\":0,\"Y\":0,\"Z\":0}}\r\n"
#define ARDUINO_PACKER_TEMPLATE "%d, %d, %d, %d, %d, %d, %d, %d\r\n"

/* UART Data buffer */
char buf[300];

void println(const char* str)
{
	udi_cdc_write_buf(str,strlen(str));
	char ch = '\r';
	udi_cdc_putc(ch);
	ch = '\n';
	udi_cdc_putc(ch);
}

void send_struct(const void* data, size_t size)
{
	udi_cdc_write_buf(data, size);
	char ch = '\r';
	udi_cdc_putc(ch);
	ch = '\n';
	udi_cdc_putc(ch);
}

void cdc_received()
{
	char valueBuffer;
	valueBuffer = udi_cdc_getc();
	//echo back
	udi_cdc_putc(valueBuffer);
}

int main (void)
{
	/* Insert system clock initialization code here (sysclk_init()). */
	sysclk_init();
	
	/* Initialize board */
	board_init();

	/* start USB stack */
	udc_start();
	
	/* Insert application code here */	
	imuData_t imuData;
	
	/* Initialize ADC */
	initializeAdc();
	
	/* Initialize Usart */
	initializeUart();	
		
	/* Initialize IMU */
	initializeImu();
	delay_ms(3000);
	const char *greetingMsgBuf = "Firmware Starting";
	println(greetingMsgBuf);
	
	uint32_t timeout_value = wdt_get_timeout_value(3000 * 1000, BOARD_FREQ_SLCK_XTAL);
	
	
	/* Initialize WDT with the given parameters. */
	wdt_init(WDT, WDT_MR_WDRPROC | WDT_MR_WDRSTEN | WDT_MR_WDIDLEHLT, timeout_value, timeout_value);
	
	adc_start(ADC);

	struct {
		uint16_t us_value[NUM_CHANNELS];
	}adc_values_s;
		
	while(true)
	{
		// delay_ms(10);

		//getImuData(&imuData);
		
		
		/* Check if ADC sample is done. */
		if (g_adc_sample_data.us_done == ADC_DONE_MASK) {						
			/* Fill the buffer to be sent via UART */
			//sprintf(buf, JSON_PACKET_TEMPLATE,
			//g_adc_sample_data.us_value[0], g_adc_sample_data.us_value[1], g_adc_sample_data.us_value[2], g_adc_sample_data.us_value[3],
			//g_adc_sample_data.us_value[4], g_adc_sample_data.us_value[5], g_adc_sample_data.us_value[6], g_adc_sample_data.us_value[7],
			// (int)imuData.gyroscope[0], (int)imuData.gyroscope[1], (int)imuData.gyroscope[2], (int)imuData.accelerometer[0], (int)imuData.accelerometer[1], (int)imuData.accelerometer[2]);
			 //g_adc_sample_data.us_done = 0;

			
			//sprintf(buf, ARDUINO_PACKER_TEMPLATE,
			//g_adc_sample_data.us_value[0], g_adc_sample_data.us_value[1], g_adc_sample_data.us_value[2], g_adc_sample_data.us_value[3],
			//g_adc_sample_data.us_value[4], g_adc_sample_data.us_value[5], g_adc_sample_data.us_value[6], g_adc_sample_data.us_value[7]);
			//g_adc_sample_data.us_done = 0;
			
			/* Send Packet to UART */
			// usartWritePacket((uint8_t*)buf);
			//buf[299] = 0;
			//println(buf);
			// memcpy(&adc_values_s, (&g_adc_sample_data + (sizeof(uint8_t) * NUM_CHANNELS)), sizeof(adc_values_s));
			send_struct((void*)&g_adc_sample_data + sizeof(g_adc_sample_data.uc_ch_num), sizeof(g_adc_sample_data.us_value));
			g_adc_sample_data.us_done = 0;
						
		}
		
		wdt_restart(WDT);
	}
}
