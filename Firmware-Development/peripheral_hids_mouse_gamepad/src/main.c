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

/* Number of pixels by which the cursor is moved when a button is pushed. */
#define MOVEMENT_SPEED              5
/* Number of input reports in this application. */
#define INPUT_REPORT_COUNT          3
/* Length of Mouse Input Report containing button data. */
#define INPUT_REP_BUTTONS_LEN       3
/* Length of Mouse Input Report containing movement data. */
#define INPUT_REP_MOVEMENT_LEN      3
/* Length of Mouse Input Report containing media player data. */
#define INPUT_REP_MEDIA_PLAYER_LEN  1
/* Index of Mouse Input Report containing button data. */
#define INPUT_REP_BUTTONS_INDEX     0
/* Index of Mouse Input Report containing movement data. */
#define INPUT_REP_MOVEMENT_INDEX    1
/* Index of Mouse Input Report containing media player data. */
#define INPUT_REP_MPLAYER_INDEX     2
/* Id of reference to Mouse Input Report containing button data. */
#define INPUT_REP_REF_BUTTONS_ID    1
/* Id of reference to Mouse Input Report containing movement data. */
#define INPUT_REP_REF_MOVEMENT_ID   2
/* Id of reference to Mouse Input Report containing media player data. */
#define INPUT_REP_REF_MPLAYER_ID    3

/* HIDs queue size. */
#define HIDS_QUEUE_SIZE 10

/* HIDS instance. */
BT_HIDS_DEF(hids_obj,
	    INPUT_REP_BUTTONS_LEN,
	    INPUT_REP_MOVEMENT_LEN,
	    INPUT_REP_MEDIA_PLAYER_LEN);

static struct k_work hids_work;
struct mouse_pos {
	int16_t x_val;
	int16_t y_val;
};

// #define CHK(X) ({ int err = X; if (err != 0) { LOG_ERR("%s returned %d (%s:%d)", #X, err, __FILE__, __LINE__); } err == 0; })
#define REPORT_ID 3
#define REPORT_ID_IDX 0
#define REPORT_LEN 10

struct __attribute__((packed)) report_t {
    uint8_t dpad;
    uint8_t capture : 1;
    uint8_t assistant : 1;
    uint8_t l2 : 1;
    uint8_t r2 : 1;
    uint8_t stadia : 1;
    uint8_t menu : 1;
    uint8_t options : 1;
    uint8_t r3 : 1;
    uint8_t l3 : 1;
    uint8_t r1 : 1;
    uint8_t l1 : 1;
    uint8_t y : 1;
    uint8_t x : 1;
    uint8_t b : 1;
    uint8_t a : 1;
    uint8_t padding : 1;
    uint8_t lx;
    uint8_t ly;
    uint8_t rx;
    uint8_t ry;
    uint8_t l2_axis;
    uint8_t r2_axis;
    uint8_t extra_buttons;
};

static struct report_t report;
static struct report_t prev_report;

static const uint8_t dpad_lut[] = { 0x0F, 0x06, 0x02, 0x0F, 0x00, 0x07, 0x01, 0x00, 0x04, 0x05, 0x03, 0x04, 0x0F, 0x06, 0x02, 0x0F };

/* Mouse movement queue. */
K_MSGQ_DEFINE(hids_queue,
	      sizeof(struct mouse_pos),
	      HIDS_QUEUE_SIZE,
	      4);


static void hid_init(void)
{
	int err;
	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *hids_inp_rep;
	static const uint8_t mouse_movement_mask[DIV_ROUND_UP(INPUT_REP_MOVEMENT_LEN, 8)] = {0};

	static uint8_t const report_map[] = {
		0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
		0x09, 0x05,        // Usage (Game Pad)
		0xA1, 0x01,        // Collection (Application)
		0x85, 0x03,        //   Report ID (3)
		0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
		0x75, 0x04,        //   Report Size (4)
		0x95, 0x01,        //   Report Count (1)
		0x25, 0x07,        //   Logical Maximum (7)
		0x46, 0x3B, 0x01,  //   Physical Maximum (315)
		0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
		0x09, 0x39,        //   Usage (Hat switch)
		0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
		0x45, 0x00,        //   Physical Maximum (0)
		0x65, 0x00,        //   Unit (None)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x04,        //   Report Count (4)
		0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x09,        //   Usage Page (Button)
		0x15, 0x00,        //   Logical Minimum (0)
		0x25, 0x01,        //   Logical Maximum (1)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x0F,        //   Report Count (15)
		0x09, 0x12,        //   Usage (0x12)
		0x09, 0x11,        //   Usage (0x11)
		0x09, 0x14,        //   Usage (0x14)
		0x09, 0x13,        //   Usage (0x13)
		0x09, 0x0D,        //   Usage (0x0D)
		0x09, 0x0C,        //   Usage (0x0C)
		0x09, 0x0B,        //   Usage (0x0B)
		0x09, 0x0F,        //   Usage (0x0F)
		0x09, 0x0E,        //   Usage (0x0E)
		0x09, 0x08,        //   Usage (0x08)
		0x09, 0x07,        //   Usage (0x07)
		0x09, 0x05,        //   Usage (0x05)
		0x09, 0x04,        //   Usage (0x04)
		0x09, 0x02,        //   Usage (0x02)
		0x09, 0x01,        //   Usage (0x01)
		0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x01,        //   Report Count (1)
		0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
		0x15, 0x01,        //   Logical Minimum (1)
		0x26, 0xFF, 0x00,  //   Logical Maximum (255)
		0x09, 0x01,        //   Usage (Pointer)
		0xA1, 0x00,        //   Collection (Physical)
		0x09, 0x30,        //     Usage (X)
		0x09, 0x31,        //     Usage (Y)
		0x75, 0x08,        //     Report Size (8)
		0x95, 0x02,        //     Report Count (2)
		0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,              //   End Collection
		0x09, 0x01,        //   Usage (Pointer)
		0xA1, 0x00,        //   Collection (Physical)
		0x09, 0x32,        //     Usage (Z)
		0x09, 0x35,        //     Usage (Rz)
		0x75, 0x08,        //     Report Size (8)
		0x95, 0x02,        //     Report Count (2)
		0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,              //   End Collection
		0x05, 0x02,        //   Usage Page (Sim Ctrls)
		0x75, 0x08,        //   Report Size (8)
		0x95, 0x02,        //   Report Count (2)
		0x15, 0x00,        //   Logical Minimum (0)
		0x26, 0xFF, 0x00,  //   Logical Maximum (255)
		0x09, 0xC5,        //   Usage (Brake)
		0x09, 0xC4,        //   Usage (Accelerator)
		0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x0C,        //   Usage Page (Consumer)
		0x15, 0x00,        //   Logical Minimum (0)
		0x25, 0x01,        //   Logical Maximum (1)
		0x09, 0xE9,        //   Usage (Volume Increment)
		0x09, 0xEA,        //   Usage (Volume Decrement)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x02,        //   Report Count (2)
		0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x09, 0xCD,        //   Usage (Play/Pause)
		0x95, 0x01,        //   Report Count (1)
		0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x05,        //   Report Count (5)
		0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
	
		/*
			0x85, 0x05,                    //   Report ID (5)
			0x06, 0x0F, 0x00,              //   Usage Page (PID Page)
			0x09, 0x97,                    //   Usage (0x97)
			0x75, 0x10,                    //   Report Size (16)
			0x95, 0x02,                    //   Report Count (2)
			0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65535)
			0x91, 0x02,                    //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		*/
	
		0xC0,  // End Collection
	};

	hids_init_param.rep_map.data = report_map;
	hids_init_param.rep_map.size = sizeof(report_map);

	hids_init_param.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_param.info.b_country_code = 0x00;
	hids_init_param.info.flags = (BT_HIDS_REMOTE_WAKE |
				      BT_HIDS_NORMALLY_CONNECTABLE);

	// hids_inp_rep = &hids_init_param.inp_rep_group_init.reports[0];
	// hids_inp_rep->size = INPUT_REP_BUTTONS_LEN;
	// hids_inp_rep->id = INPUT_REP_REF_BUTTONS_ID;
	// hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep = &hids_init_param.inp_rep_group_init.reports[0];
    hids_inp_rep->size = sizeof(struct report_t);
    hids_inp_rep->id = REPORT_ID;
    hids_init_param.inp_rep_group_init.cnt++;

	// hids_inp_rep++;
	// hids_inp_rep->size = INPUT_REP_MOVEMENT_LEN;
	// hids_inp_rep->id = INPUT_REP_REF_MOVEMENT_ID;
	// hids_inp_rep->rep_mask = mouse_movement_mask;
	// hids_init_param.inp_rep_group_init.cnt++;

	// hids_inp_rep++;
	// hids_inp_rep->size = INPUT_REP_MEDIA_PLAYER_LEN;
	// hids_inp_rep->id = INPUT_REP_REF_MPLAYER_ID;
	// hids_init_param.inp_rep_group_init.cnt++;

	// hids_init_param.is_mouse = true;
	// hids_init_param.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj, &hids_init_param);
	__ASSERT(err == 0, "HIDS initialization failed\n");
}

static void report_sent_cb(struct bt_conn* conn, void* user_data) {
    // LOG_DBG("");
}

static void hids_work_fn(struct k_work* work) {
    uint8_t report[REPORT_LEN];

    while (!k_msgq_get(&hids_queue, report, K_NO_WAIT)) {
		for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
			if (conn_mode[i].conn != NULL) {
				// k_work_reschedule(&sleep_work, CONNECTED_SLEEP_TIMEOUT);
				printk("Sending report...");
				bt_hids_inp_rep_send(&hids_obj, conn_mode[i].conn, REPORT_ID_IDX, report, REPORT_LEN, report_sent_cb);
			}	
		}
    }
}
static K_WORK_DEFINE(hids_work, hids_work_fn);

static void handle_buttons(int val) {
    report.menu = val;
    report.options = val;
    report.stadia = val;
    report.capture = val;
    report.l3 = val;
    report.r3 = val;
    report.x = val;
    report.y = val;
    report.r1 = val;
    report.l1 = val;
    report.a = val;
    report.b = val;
    report.r2 = val;
    report.r2_axis = report.r2 * 255;
    report.l2 = val;
    report.l2_axis = report.l2 * 255;

    // int dpad = gpio_pin_get_dt(&buttons[6]) | (gpio_pin_get_dt(&buttons[8]) << 1) | (gpio_pin_get_dt(&buttons[9]) << 2) | (gpio_pin_get_dt(&buttons[7]) << 3);
    int dpad = val | (0 << 1) | (0 << 2) | (0 << 3);

    report.dpad = dpad_lut[dpad];

    if (memcmp(&prev_report, &report, sizeof(report))) {
        k_msgq_put(&hids_queue, &report, K_NO_WAIT);
        k_work_submit(&hids_work);
        memcpy(&prev_report, &report, sizeof(report));
    }
}

static void mouse_movement_send(int16_t x_delta, int16_t y_delta)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {

		if (!conn_mode[i].conn) {
			continue;
		}

		if (conn_mode[i].in_boot_mode) {
			x_delta = MAX(MIN(x_delta, SCHAR_MAX), SCHAR_MIN);
			y_delta = MAX(MIN(y_delta, SCHAR_MAX), SCHAR_MIN);

			bt_hids_boot_mouse_inp_rep_send(&hids_obj,
							     conn_mode[i].conn,
							     NULL,
							     (int8_t) x_delta,
							     (int8_t) y_delta,
							     NULL);
		} else {
			uint8_t x_buff[2];
			uint8_t y_buff[2];
			uint8_t buffer[INPUT_REP_MOVEMENT_LEN];

			int16_t x = MAX(MIN(x_delta, 0x07ff), -0x07ff);
			int16_t y = MAX(MIN(y_delta, 0x07ff), -0x07ff);

			/* Convert to little-endian. */
			sys_put_le16(x, x_buff);
			sys_put_le16(y, y_buff);

			/* Encode report. */
			BUILD_ASSERT(sizeof(buffer) == 3,
					 "Only 2 axis, 12-bit each, are supported");

			buffer[0] = x_buff[0];
			buffer[1] = (y_buff[0] << 4) | (x_buff[1] & 0x0f);
			buffer[2] = (y_buff[1] << 4) | (y_buff[0] >> 4);


			bt_hids_inp_rep_send(&hids_obj, conn_mode[i].conn,
						  INPUT_REP_MOVEMENT_INDEX,
						  buffer, sizeof(buffer), NULL);
		}
	}
}

static void mouse_handler(struct k_work *work)
{
	struct mouse_pos pos;

	while (!k_msgq_get(&hids_queue, &pos, K_NO_WAIT)) {
		mouse_movement_send(pos.x_val, pos.y_val);
	}
}

static void report_init() {
    memset(&report, 0, sizeof(report));
    report.dpad = 8;
    report.lx = 0x80;
    report.ly = 0x80;
    report.rx = 0x80;
    report.ry = 0x80;
    memcpy(&prev_report, &report, sizeof(report));
}

void button_changed(uint32_t button_state, uint32_t has_changed)
{
	bool data_to_send = false;
	struct mouse_pos pos;
	uint32_t buttons = button_state & has_changed;

	memset(&pos, 0, sizeof(struct mouse_pos));

	if (IS_ENABLED(CONFIG_BT_HIDS_SECURITY_ENABLED)) {
		if (k_msgq_num_used_get(&mitm_queue)) {
			if (buttons & KEY_PAIRING_ACCEPT) {
				num_comp_reply(true);

				return;
			}

			if (buttons & KEY_PAIRING_REJECT) {
				num_comp_reply(false);

				return;
			}
		}
	}

	if (buttons & KEY_LEFT_MASK) {
		pos.x_val -= MOVEMENT_SPEED;
		printk("%s(): left\n", __func__);
		data_to_send = true;
	}
	if (buttons & KEY_UP_MASK) {
		pos.y_val -= MOVEMENT_SPEED;
		printk("%s(): up\n", __func__);
		data_to_send = true;
	}
	if (buttons & KEY_RIGHT_MASK) {
		pos.x_val += MOVEMENT_SPEED;
		printk("%s(): right\n", __func__);
		data_to_send = true;
	}
	if (buttons & KEY_DOWN_MASK) {
		pos.y_val += MOVEMENT_SPEED;
		printk("%s(): down\n", __func__);
		data_to_send = true;
	}

	if (data_to_send) {
		int err;

		err = k_msgq_put(&hids_queue, &pos, K_NO_WAIT);
		if (err) {
			printk("No space in the queue for button pressed\n");
			return;
		}
		if (k_msgq_num_used_get(&hids_queue) == 1) {
			k_work_submit(&hids_work);
		}
	}
}

int main(void)
{
	/* DIS initialized at system boot with SYS_INIT macro. */
	report_init();
	hid_init();

	// k_work_init(&hids_work, mouse_handler);
	initialize_bluetooth(&hids_obj);

	configure_buttons(button_changed);

	while (1) {
		k_sleep(K_SECONDS(1));
		handle_buttons(0);
		bas_notify();
		k_sleep(K_SECONDS(1));
		handle_buttons(1);
	}
}
