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

/* UART Data buffer */
char buf[300];

int main (void)
{
	/* Insert system clock initialization code here (sysclk_init()). */
	sysclk_init();
	
	/* Initialize board */
	board_init();
	
	/* Insert application code here */	
	imuData_t imuData;
	
	/* Initialize ADC */
	initializeAdc();
	
	/* Initialize Usart */
	initializeUart();	
		
	/* Initialize IMU */
	initializeImu();
			
	while(true)
	{
		delay_ms(1000);

		getImuData(&imuData);
		
		adc_start(ADC);
		
		/* Check if ADC sample is done. */
		if (g_adc_sample_data.us_done == ADC_DONE_MASK) {
			
			/* Fill the buffer to be sent via UART */
			sprintf(buf, JSON_PACKET_TEMPLATE,
			g_adc_sample_data.us_value[0], g_adc_sample_data.us_value[1], g_adc_sample_data.us_value[2], g_adc_sample_data.us_value[3],
			g_adc_sample_data.us_value[4], g_adc_sample_data.us_value[5], g_adc_sample_data.us_value[6], g_adc_sample_data.us_value[7],
			(int)imuData.gyroscope[0], (int)imuData.gyroscope[1], (int)imuData.gyroscope[2], (int)imuData.accelerometer[0], (int)imuData.accelerometer[1], (int)imuData.accelerometer[2]);
			g_adc_sample_data.us_done = 0;
		}
		
		/* Send Packet to UART */
		 usartWritePacket((uint8_t*)buf);
	}
}
