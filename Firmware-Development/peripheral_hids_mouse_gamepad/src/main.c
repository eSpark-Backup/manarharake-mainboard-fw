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

#include <zephyr/bluetooth/controller.h>

#include "bluetooth_config.h"
#include "ble_gamepad.h"
#include "ble_mouse.h"

typedef enum {
	MANAR_MODE_MOUSE,
	MANAR_MODE_GAMEPAD,
	MANAR_MODE_PALM_PCB,
	MANAR_MODE_VR,
}manar_mode_t;

static manar_mode_t manar_mode __attribute__((section(".non_init")));

void manar_mode_next()
{
	manar_mode++;
	if(manar_mode > MANAR_MODE_GAMEPAD)
	{
		manar_mode = MANAR_MODE_MOUSE;
	}
}

uint32_t millis(void) {
    return k_uptime_get_32();  // Returns milliseconds since boot
}

static void set_bt_addr(const char *addr_str, const char *type_str)
{
	int err;
	bt_addr_le_t addr;

	err = bt_addr_le_from_str(addr_str, type_str, &addr);
	if (err) {
		printk("Invalid BT address (err %d)\n", err);
	}

	if (addr.type == BT_ADDR_LE_PUBLIC) {
		bt_ctlr_set_public_addr(addr.a.val);
		return;
	} else {
		err = bt_id_create(&addr, NULL);
		if (err < 0) {
			printk("Creating new ID failed (err %d)\n", err);
		}
	}
}

int main(void)
{
	printk("Application Started\n");

	printk("current Manar mode = %d\n", manar_mode);
	if(manar_mode > MANAR_MODE_VR)
	{
		manar_mode = MANAR_MODE_MOUSE;
	}
	// k_sleep(K_SECONDS(2));
	// manar_mode_next();
	// printk("updated Manar mode = %d\n", manar_mode);
	// k_sleep(K_SECONDS(2));
	// NVIC_SystemReset();

	/*currently working on mousepad so using this.*/
	// manar_mode = MANAR_MODE_GAMEPAD;
	// manar_mode = MANAR_MODE_MOUSE;

	// bt_set_id_addr()


	switch(manar_mode) {
		case MANAR_MODE_PALM_PCB:
			printk("MANAR_MODE_PALM_PCB STARTING\n");
			break;
		
		case MANAR_MODE_MOUSE:
			printk("MANAR_MODE_MOUSE STARTING\n");
			// delete_bonded_devices();
			set_bt_addr("A4:5E:60:3B:7D:21", "random");
			ble_config_set_ad_sd_data(ble_mouse_get_ad_sd_data());
			initialize_bluetooth(ble_mouse_return_hids_object());
			ble_mouse_hid_init();		
			configure_buttons(ble_mouse_button_changed);		
			printk("MANAR_MODE_MOUSE INITIALIZED\n");
			break;
			
		case MANAR_MODE_GAMEPAD:
			printk("MANAR_MODE_GAMEPAD STARTING\n");
			// delete_bonded_devices();
			set_bt_addr("BC:92:6B:1F:34:AA", "random");
			ble_config_set_ad_sd_data(ble_gamepad_get_ad_sd_data());
			initialize_bluetooth(ble_gamepad_return_hids_object());
			ble_gamepad_report_init();
			ble_gamepad_hid_init();		
			configure_buttons(ble_gamepad_button_changed);		
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
			if(!conn_mode[0].conn)
			{
				printk("Connection not established or device not paired!\n");
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
				k_sleep(K_SECONDS(1));
				// bas_notify();
				ble_mouse_handle_movement(-10, 0);
				k_sleep(K_SECONDS(1));
				break;
				
			case MANAR_MODE_GAMEPAD:
				printk("MANAR_MODE_GAMEPAD\n");
				ble_gamepad_handle_buttons(0);
				k_sleep(K_SECONDS(1));
				// bas_notify();
				ble_gamepad_handle_buttons(1);
				k_sleep(K_SECONDS(1));
				break;
				
			case MANAR_MODE_VR:		
				printk("MANAR_MODE_VR\n");
				k_sleep(K_SECONDS(1));
				break;

			default:
				printk("Invalid manar mode!\n");
				k_sleep(K_SECONDS(5));
				break;
		}
	}
}
