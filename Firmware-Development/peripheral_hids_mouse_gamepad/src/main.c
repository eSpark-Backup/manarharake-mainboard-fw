/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>
#include <assert.h>

#include "bluetooth_config.h"
#include "ble_gamepad.h"
#include "ble_mouse.h"

typedef enum {
	MANAR_MODE_PALM_PCB,
	MANAR_MODE_MOUSE,
	MANAR_MODE_GAMEPAD,
	MANAR_MODE_VR,
}manar_mode_t;

static manar_mode_t manar_mode __attribute__((section(".non_init")));

void manar_mode_next()
{
	manar_mode++;
	if(manar_mode > MANAR_MODE_VR)
	{
		manar_mode = MANAR_MODE_PALM_PCB;
	}
}

uint32_t millis(void) {
    return k_uptime_get_32();  // Returns milliseconds since boot
}

int main(void)
{
	printk("Application Started\n");

	printk("current Manar mode = %d\n", manar_mode);
	// k_sleep(K_SECONDS(2));
	// manar_mode_next();
	// printk("updated Manar mode = %d\n", manar_mode);
	// k_sleep(K_SECONDS(2));
	// NVIC_SystemReset();

	/*currently working on mousepad so using this.*/
	// manar_mode = MANAR_MODE_GAMEPAD;
	// manar_mode = MANAR_MODE_MOUSE;

	switch(manar_mode) {
		case MANAR_MODE_PALM_PCB:
			printk("MANAR_MODE_PALM_PCB STARTING\n");
			break;
		
		case MANAR_MODE_MOUSE:
			printk("MANAR_MODE_MOUSE STARTING\n");
			ble_config_set_ad_sd_data(ble_mouse_get_ad_sd_data());
			initialize_bluetooth(ble_mouse_return_hids_object());
			delete_bonded_devices();
			ble_mouse_hid_init();		
			configure_buttons(ble_mouse_button_changed);		
			delete_bonded_devices();
			printk("MANAR_MODE_MOUSE INITIALIZED\n");
			break;
			
		case MANAR_MODE_GAMEPAD:
			printk("MANAR_MODE_GAMEPAD STARTING\n");
			ble_config_set_ad_sd_data(ble_gamepad_get_ad_sd_data());
			initialize_bluetooth(ble_gamepad_return_hids_object());
			ble_gamepad_report_init();
			delete_bonded_devices();
			ble_gamepad_hid_init();		
			configure_buttons(ble_gamepad_button_changed);		
			delete_bonded_devices();
			printk("MANAR_MODE_GAMEPAD INTITIALIZED\n");
			break;
		
		default:
			printk("Invalid manar mode!\n");
			break;
	}

	while (1) {

		/* switch from current mode to next mode every 2 minutes */
		if(millis() > 1 * 60 * 1000)
		{
			manar_mode_next();
			printk("Switching to mode %d in 5 seconds", manar_mode);
			k_sleep(K_SECONDS(5));
			NVIC_SystemReset();
		}

		printk("looping mode %d!\n", manar_mode);
		if(manar_mode == MANAR_MODE_GAMEPAD || manar_mode == MANAR_MODE_MOUSE)
		{
			if(!conn_mode[0].conn && !piaring_success)
			{
				printk("Connection not established or pairing failed!\n");
				k_sleep(K_SECONDS(1));
				continue;
			}
		}
		else
		{
			printk("handling non ble mode\n");
		}

		switch(manar_mode) {
			case MANAR_MODE_PALM_PCB:
				printk("MANAR_MODE_PALM_PCB\n");
				k_sleep(K_SECONDS(1));

				break;
			
			case MANAR_MODE_MOUSE:
				printk("MANAR_MODE_MOUSE\n");
				ble_mouse_handle_movement(5, 0);
				printk("No connection established!");
				k_sleep(K_SECONDS(1));
				bas_notify();
				ble_mouse_handle_movement(-10, 0);
				k_sleep(K_SECONDS(1));
				break;
				
			case MANAR_MODE_GAMEPAD:
				printk("MANAR_MODE_GAMEPAD\n");
				ble_gamepad_handle_buttons(0);
				k_sleep(K_SECONDS(1));
				bas_notify();
				ble_gamepad_handle_buttons(1);
				k_sleep(K_SECONDS(1));
				break;
				
			case MANAR_MODE_VR:		
				printk("MANAR_MODE_VR\n");
				k_sleep(K_SECOND(1));
				break;

			default:
				printk("Invalid manar mode!\n");
				k_sleep(K_SECONDS(5));
				break;
		}
	}
}
