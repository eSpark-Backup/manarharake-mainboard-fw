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

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/bluetooth/services/bas.h>
#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/services/dis.h>
#include <dk_buttons_and_leds.h>

#include "bluetooth_config.h"
#include "ble_gamepad.h"
#include "ble_mouse.h"

/* HIDS instance. */
// BT_HIDS_DEF(hids_obj,
// 	    INPUT_REP_BUTTONS_LEN,
// 	    INPUT_REP_MOVEMENT_LEN,
// 	    INPUT_REP_MEDIA_PLAYER_LEN);

// #define CHK(X) ({ int err = X; if (err != 0) { LOG_ERR("%s returned %d (%s:%d)", #X, err, __FILE__, __LINE__); } err == 0; })

typedef enum {
	MANAR_MODE_PALM_PCB,
	MANAR_MODE_MOUSE,
	MANAR_MODE_GAMEPAD,
	MANAR_MODE_VR,
}manar_mode_t;

static manar_mode_t manar_mode __attribute__((section(".non_init")));

struct mouse_pos {
	int16_t x_val;
	int16_t y_val;
};

// static void mouse_movement_send(int16_t x_delta, int16_t y_delta)
// {
// 	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {

// 		if (!conn_mode[i].conn) {
// 			continue;
// 		}

// 		if (conn_mode[i].in_boot_mode) {
// 			x_delta = MAX(MIN(x_delta, SCHAR_MAX), SCHAR_MIN);
// 			y_delta = MAX(MIN(y_delta, SCHAR_MAX), SCHAR_MIN);

// 			bt_hids_boot_mouse_inp_rep_send(&hids_obj,
// 							     conn_mode[i].conn,
// 							     NULL,
// 							     (int8_t) x_delta,
// 							     (int8_t) y_delta,
// 							     NULL);
// 		} else {
// 			uint8_t x_buff[2];
// 			uint8_t y_buff[2];
// 			uint8_t buffer[INPUT_REP_MOVEMENT_LEN];

// 			int16_t x = MAX(MIN(x_delta, 0x07ff), -0x07ff);
// 			int16_t y = MAX(MIN(y_delta, 0x07ff), -0x07ff);

// 			/* Convert to little-endian. */
// 			sys_put_le16(x, x_buff);
// 			sys_put_le16(y, y_buff);

// 			/* Encode report. */
// 			BUILD_ASSERT(sizeof(buffer) == 3,
// 					 "Only 2 axis, 12-bit each, are supported");

// 			buffer[0] = x_buff[0];
// 			buffer[1] = (y_buff[0] << 4) | (x_buff[1] & 0x0f);
// 			buffer[2] = (y_buff[1] << 4) | (y_buff[0] >> 4);


// 			bt_hids_inp_rep_send(&hids_obj, conn_mode[i].conn,
// 						  INPUT_REP_MOVEMENT_INDEX,
// 						  buffer, sizeof(buffer), NULL);
// 		}
// 	}
// }

// static void mouse_handler(struct k_work *work)
// {
// 	struct mouse_pos pos;

// 	while (!k_msgq_get(&hids_queue, &pos, K_NO_WAIT)) {
// 		mouse_movement_send(pos.x_val, pos.y_val);
// 	}
// }

void manar_mode_next()
{
	manar_mode++;
	if(manar_mode > MANAR_MODE_VR)
	{
		manar_mode = MANAR_MODE_PALM_PCB;
	}
}

int main(void)
{
	printk("Application Started\n");

	printk("current Manar mode = %d\n", manar_mode);
	k_sleep(K_SECONDS(20));
	manar_mode_next();
	printk("updated Manar mode = %d\n", manar_mode);
	k_sleep(K_SECONDS(2));
	// NVIC_SystemReset();

	//currently working on mousepad so using this.
	// manar_mode = MANAR_MODE_GAMEPAD;
	manar_mode = MANAR_MODE_MOUSE;

	// /* DIS initialized at system boot with SYS_INIT macro. */
	
	// // k_work_init(&hids_work, mouse_handler);
	// initialize_bluetooth(ble_gamepad_return_hids_object());
	
	// ble_gamepad_report_init();
	// ble_gamepad_hid_init();

	// configure_buttons(ble_gamepad_button_changed);

	// delete_bonded_devices();

	// // deinit after 30 iteration of gamepad on/off
	// printk("Deleting bonded devices!\n");
	// // delete_bonded_devices();
	// ble_gamepad_hid_unint();
	// struct bt_hids* hids_obj_base_ptr = ble_gamepad_return_hids_object();
	// hids_obj_base_ptr->gp.attr_array_size = CONFIG_BT_HIDS_ATTR_MAX;
	
	// //wait 30 seconds before next init
	// printk("Waiting 30s before init!\n");
	// k_sleep(K_SECONDS(30));
	
	// //initialzie again the hid object
	// printk("hid reinit!\n");
	// ble_gamepad_hid_init();
	
	// printk("Waiting 30s before loop\n");
	// k_sleep(K_SECONDS(30));

	switch(manar_mode) {
		case MANAR_MODE_PALM_PCB:
			break;
		
		case MANAR_MODE_MOUSE:
			printk("MANAR_MODE_MOUSE STARTING\n");
			ble_config_set_ad_sd_data(ble_mouse_get_ad_sd_data());
			initialize_bluetooth(ble_mouse_return_hids_object());
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
		printk("looping mode %d!\n", manar_mode);

		switch(manar_mode) {
			case MANAR_MODE_PALM_PCB:
				break;
			
			case MANAR_MODE_MOUSE:
				printk("MANAR_MODE_MOUSE\n");
				if(conn_mode[0].conn && piaring_success)
				{
					ble_mouse_handle_movement(10, 0);
				}
				else
				{
					printk("No connection established!");
				}
				k_sleep(K_SECONDS(1));
				// bas_notify();
				// ble_mouse_handle_movement(-10, 0);
				// k_sleep(K_SECONDS(1));
				break;
			
			case MANAR_MODE_GAMEPAD:
				printk("MANAR_MODE_GAMEPAD\n");
				ble_gamepad_handle_buttons(0);
				k_sleep(K_SECONDS(1));
				bas_notify();
				ble_gamepad_handle_buttons(1);
				k_sleep(K_SECONDS(1));
				break;
			
			default:
				printk("Invalid manar mode!\n");
				break;
		}

	}
}
