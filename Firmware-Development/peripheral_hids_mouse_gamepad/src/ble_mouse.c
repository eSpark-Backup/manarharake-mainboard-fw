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

#include "ble_mouse.h"

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

struct mouse_pos {
	int16_t x_val;
	int16_t y_val;
};

struct mouse_button {
	uint8_t button_mask;  // 🖱️ New field for mouse buttons
};

BT_HIDS_DEF(hids_obj_mouse,
    INPUT_REP_BUTTONS_LEN,
    INPUT_REP_MOVEMENT_LEN,
    INPUT_REP_MEDIA_PLAYER_LEN);

static struct k_work hids_work;
static struct k_work hids_work_button;

/* Mouse movement queue. */
K_MSGQ_DEFINE(hids_movement_queue,
    sizeof(struct mouse_pos),
    HIDS_QUEUE_SIZE,
    4);

K_MSGQ_DEFINE(hids_button_queue,
      sizeof(struct mouse_button),
      HIDS_QUEUE_SIZE,
      4);

static const struct bt_data ad_mouse[] = {
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
                (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
                (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                        BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd_mouse[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, MOUSE_DEVICE_NAME, MOUSE_DEVICE_NAME_LEN),
};

ble_config_t ble_mouse_get_ad_sd_data()
{
    ble_config_t mouse_ad_sd;
    mouse_ad_sd.ad_ptr = ad_mouse;
    mouse_ad_sd.sd_ptr = sd_mouse;
    mouse_ad_sd.ad_len = ARRAY_SIZE(ad_mouse);
    mouse_ad_sd.sd_len = ARRAY_SIZE(sd_mouse);

    return mouse_ad_sd;
}

void* ble_mouse_return_hids_object()
{
    return &hids_obj_mouse;
}

void ble_mouse_hid_init()
{
    int err;
	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *hids_inp_rep;
	static const uint8_t mouse_movement_mask[DIV_ROUND_UP(INPUT_REP_MOVEMENT_LEN, 8)] = {0};

	static const uint8_t report_map_mouse[] = {
		0x05, 0x01,     /* Usage Page (Generic Desktop) */
		0x09, 0x02,     /* Usage (Mouse) */

		0xA1, 0x01,     /* Collection (Application) */

		/* Report ID 1: Mouse buttons + scroll/pan */
		0x85, 0x01,       /* Report Id 1 */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x95, 0x05,       /* Report Count (3) */
		0x75, 0x01,       /* Report Size (1) */
		0x05, 0x09,       /* Usage Page (Buttons) */
		0x19, 0x01,       /* Usage Minimum (01) */
		0x29, 0x05,       /* Usage Maximum (05) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x01,       /* Logical Maximum (1) */
		0x81, 0x02,       /* Input (Data, Variable, Absolute) */
		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x03,       /* Report Size (3) */
		0x81, 0x01,       /* Input (Constant) for padding */
		0x75, 0x08,       /* Report Size (8) */
		0x95, 0x01,       /* Report Count (1) */
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x38,       /* Usage (Wheel) */
		0x15, 0x81,       /* Logical Minimum (-127) */
		0x25, 0x7F,       /* Logical Maximum (127) */
		0x81, 0x06,       /* Input (Data, Variable, Relative) */
		0x05, 0x0C,       /* Usage Page (Consumer) */
		0x0A, 0x38, 0x02, /* Usage (AC Pan) */
		0x95, 0x01,       /* Report Count (1) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0xC0,             /* End Collection (Physical) */

		/* Report ID 2: Mouse motion */
		0x85, 0x02,       /* Report Id 2 */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x75, 0x0C,       /* Report Size (12) */
		0x95, 0x02,       /* Report Count (2) */
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x30,       /* Usage (X) */
		0x09, 0x31,       /* Usage (Y) */
		0x16, 0x01, 0xF8, /* Logical maximum (2047) */
		0x26, 0xFF, 0x07, /* Logical minimum (-2047) */
		0x81, 0x06,       /* Input (Data, Variable, Relative) */
		0xC0,             /* End Collection (Physical) */
		0xC0,             /* End Collection (Application) */

		/* Report ID 3: Advanced buttons */
		0x05, 0x0C,       /* Usage Page (Consumer) */
		0x09, 0x01,       /* Usage (Consumer Control) */
		0xA1, 0x01,       /* Collection (Application) */
		0x85, 0x03,       /* Report Id (3) */
		0x15, 0x00,       /* Logical minimum (0) */
		0x25, 0x01,       /* Logical maximum (1) */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x01,       /* Report Count (1) */

		0x09, 0xCD,       /* Usage (Play/Pause) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x83, 0x01, /* Usage (Consumer Control Configuration) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB5,       /* Usage (Scan Next Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB6,       /* Usage (Scan Previous Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */

		0x09, 0xEA,       /* Usage (Volume Down) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xE9,       /* Usage (Volume Up) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x25, 0x02, /* Usage (AC Forward) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x24, 0x02, /* Usage (AC Back) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0xC0              /* End Collection */
	};

	hids_init_param.rep_map.data = report_map_mouse;
	hids_init_param.rep_map.size = sizeof(report_map_mouse);

	hids_init_param.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_param.info.b_country_code = 0x00;
	hids_init_param.info.flags = (BT_HIDS_REMOTE_WAKE |
				      BT_HIDS_NORMALLY_CONNECTABLE);

	hids_inp_rep = &hids_init_param.inp_rep_group_init.reports[0];
	hids_inp_rep->size = INPUT_REP_BUTTONS_LEN;
	hids_inp_rep->id = INPUT_REP_REF_BUTTONS_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MOVEMENT_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MOVEMENT_ID;
	hids_inp_rep->rep_mask = mouse_movement_mask;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MEDIA_PLAYER_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MPLAYER_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_init_param.is_mouse = true;
	hids_init_param.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj_mouse, &hids_init_param);
	__ASSERT(err == 0, "HIDS initialization failed\n");
}

static void mouse_movement_handler(struct k_work *work)
{
	struct mouse_pos pos;

	while (!k_msgq_get(&hids_movement_queue, &pos, K_NO_WAIT)) {
		ble_mouse_handle_movement(pos.x_val, pos.y_val);
	}
}

static void mouse_button_handler(struct k_work *work)
{
	struct mouse_button sbutton_mask;

	while (!k_msgq_get(&hids_button_queue, &sbutton_mask, K_NO_WAIT)) {
		ble_mouse_handle_button(sbutton_mask.button_mask);
	}
}
// k_work_init(&hids_work, mouse_movement_handler);
// k_work_init(&hids_work_button, mouse_button_handler);

static K_WORK_DEFINE(hids_work, mouse_movement_handler);
static K_WORK_DEFINE(hids_work_button, mouse_button_handler);


void ble_mouse_handle_movement(int dx, int dy) {
    int err;
    struct mouse_pos pos;

    memset(&pos, 0, sizeof(struct mouse_pos));
    pos.x_val = dx;
    pos.y_val = dy;

    err = k_msgq_put(&hids_movement_queue, &pos, K_NO_WAIT);
    if (err) {
        printk("No space in the queue for button pressed\n");
        return;
    }

    if (k_msgq_num_used_get(&hids_movement_queue) == 1) {
        k_work_submit(&hids_work);
    }
}

void ble_mouse_handle_button(uint32_t btn_mask) {
	int err;
    struct mouse_button pvbutton_mask;

    memset(&pvbutton_mask, 0, sizeof(struct mouse_button));
	pvbutton_mask.button_mask = btn_mask;

    err = k_msgq_put(&hids_button_queue, &pvbutton_mask, K_NO_WAIT);
    if (err) {
        printk("No space in the queue for button pressed\n");
        return;
    }

    if (k_msgq_num_used_get(&hids_button_queue) == 1) {
        k_work_submit(&hids_work_button);
    }
}

void ble_mouse_button_changed(uint32_t button_state, uint32_t has_changed)
{
	bool data_to_send = false;
	// struct mouse_pos pos;
	uint32_t buttons = button_state & has_changed;

	// memset(&pos, 0, sizeof(struct mouse_pos));

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

	// if (buttons & KEY_LEFT_MASK) {
	// 	pos.x_val -= MOVEMENT_SPEED;
	// 	printk("%s(): left\n", __func__);
	// 	data_to_send = true;
	// }
	// if (buttons & KEY_UP_MASK) {
	// 	pos.y_val -= MOVEMENT_SPEED;
	// 	printk("%s(): up\n", __func__);
	// 	data_to_send = true;
	// }
	// if (buttons & KEY_RIGHT_MASK) {
	// 	pos.x_val += MOVEMENT_SPEED;
	// 	printk("%s(): right\n", __func__);
	// 	data_to_send = true;
	// }
	// if (buttons & KEY_DOWN_MASK) {
	// 	pos.y_val += MOVEMENT_SPEED;
	// 	printk("%s(): down\n", __func__);
	// 	data_to_send = true;
	// }

	// if (data_to_send) {
	// 	int err;

	// 	err = k_msgq_put(&hids_queue, &pos, K_NO_WAIT);
	// 	if (err) {
	// 		printk("No space in the queue for button pressed\n");
	// 		return;
	// 	}
	// 	if (k_msgq_num_used_get(&hids_queue) == 1) {
	// 		k_work_submit(&hids_work);
	// 	}
	// }
}

void ble_mouse_hid_unint(void)
{
	bt_hids_uninit(&hids_obj_mouse);
}