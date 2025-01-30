/*
 * usb_initializer.c
 *
 * Created: 12/12/2024 4:46:25 PM
 *  Author: classicComputer
 */ 
#include <asf.h>
#include "usb_initializer.h"
#include "conf_usb.h"
#include "ui.h"
#include "uart.h"
#include <stdarg.h>
#include <string.h>

static volatile bool main_b_cdc_enable = false;

void usb_initializer_init()
{
	ui_init();
	ui_powerdown();

	// Start USB stack to authorize VBus monitoring
	udc_start();
}

void println(const char* str)
{
	udi_cdc_write_buf(str,strlen(str));
	char ch = '\r';
	udi_cdc_putc(ch);
	ch = '\n';
	udi_cdc_putc(ch);
}

void custom_printf(const char *format, ...)
{
	char buffer[256]; // Temporary buffer for formatted string
	va_list args;

	// Initialize the variadic arguments list
	va_start(args, format);

	// Format the string into the buffer
	vsnprintf(buffer, sizeof(buffer), format, args);

	// End the variadic arguments list
	va_end(args);

	// Write the formatted string to the CDC buffer
	udi_cdc_write_buf(buffer, strlen(buffer));

	// Write the newline characters
	char ch = '\r';
	udi_cdc_putc(ch);
	ch = '\n';
	udi_cdc_putc(ch);
}

void cdc_received(uint8_t port)
{
	char value;	value = udi_cdc_getc();	println("Character Received : ");	println(&value);
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
