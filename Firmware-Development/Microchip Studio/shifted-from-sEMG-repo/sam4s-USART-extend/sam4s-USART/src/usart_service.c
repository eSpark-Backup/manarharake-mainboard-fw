/*
 * usart_service.c
 *
 * Created: 4/17/2024 2:30:12 PM
 *  Author: classicComputer
 */ 
#include "usart_service.h"
#include "string.h"

/* UART config */
static usart_serial_options_t usart_options = {
	.baudrate = CONF_UART_BAUDRATE,
	.charlength = CONF_UART_CHAR_LENGTH,
	.paritytype = CONF_UART_PARITY,
	.stopbits = CONF_UART_STOP_BITS
};

void initializeUart(void)
{
	usart_serial_init(CONF_UART, &usart_options);
}

void usartWritePacket(uint8_t* bufPtr)
{
	usart_serial_write_packet(CONF_UART, (uint8_t*)bufPtr, strlen((const char*)bufPtr));	
}
