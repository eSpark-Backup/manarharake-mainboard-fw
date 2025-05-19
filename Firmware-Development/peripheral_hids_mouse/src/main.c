/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
// #include <zephyr/sys/printk.h>
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
#include <zephyr/logging/log.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

/* Uart node */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define MSG_SIZE 32
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);
static const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

void print_uart(char *buf);
void serial_cb(const struct device *dev, void *user_data);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define DEVICE_GAMEPAD  "Nordic_HIDS_gamepad"
#define DEVICE_GAMEPAD_NAME_LEN (sizeof(DEVICE_GAMEPAD) - 1)

#define BASE_USB_HID_SPEC_VERSION   0x0101

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

/* Key used to move cursor left */
#define KEY_LEFT_MASK   DK_BTN1_MSK
/* Key used to move cursor up */
#define KEY_UP_MASK     DK_BTN2_MSK
/* Key used to move cursor right */
#define KEY_RIGHT_MASK  DK_BTN3_MSK
/* Key used to move cursor down */
#define KEY_DOWN_MASK   DK_BTN4_MSK

/* Key used to accept or reject passkey value */
#define KEY_PAIRING_ACCEPT DK_BTN1_MSK
#define KEY_PAIRING_REJECT DK_BTN2_MSK

/* HIDS instance. */
BT_HIDS_DEF(hids_obj,
	    INPUT_REP_BUTTONS_LEN,
	    INPUT_REP_MOVEMENT_LEN,
	    INPUT_REP_MEDIA_PLAYER_LEN);

#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

static struct k_work hids_work;
static struct k_work hids_work_button;
struct mouse_pos {
	int16_t x_val;
	int16_t y_val;
};

struct mouse_button {
	uint8_t button_mask;  // 🖱️ New field for mouse buttons
};

/* Mouse movement queue. */
K_MSGQ_DEFINE(hids_movement_queue,
	      sizeof(struct mouse_pos),
	      HIDS_QUEUE_SIZE,
	      4);

K_MSGQ_DEFINE(hids_button_queue,
			sizeof(struct mouse_button),
			HIDS_QUEUE_SIZE,
			4);

#if CONFIG_BT_DIRECTED_ADVERTISING
/* Bonded address queue. */
K_MSGQ_DEFINE(bonds_queue,
	      sizeof(bt_addr_le_t),
	      CONFIG_BT_MAX_PAIRED,
	      4);
#endif

const struct bt_data *ad;
const struct bt_data *sd;
size_t ad_len;
size_t sd_len;

static const struct bt_data ad_mouse[] = {
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
					  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data ad_gamepad[] = {
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      (964 >> 0) & 0xff,
		      (964 >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
					  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd_mouse[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd_gamepad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_GAMEPAD, DEVICE_GAMEPAD_NAME_LEN),
};

static struct conn_mode {
	struct bt_conn *conn;
	bool in_boot_mode;
} conn_mode[CONFIG_BT_HIDS_MAX_CLIENT_COUNT];

static volatile bool is_adv_running;

static struct k_work adv_work;

static struct k_work pairing_work;
struct pairing_data_mitm {
	struct bt_conn *conn;
	unsigned int passkey;
};

K_MSGQ_DEFINE(mitm_queue,
	      sizeof(struct pairing_data_mitm),
	      CONFIG_BT_HIDS_MAX_CLIENT_COUNT,
	      4);

#define LOG_MODULE_NAME peripheral_hid_mouse
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

static volatile bool client_connected = false;
static volatile bool pairing_request = false;
static volatile bool paired_success = false;

typedef enum {
    MODE_UNKNOWN = 0,
    MODE_1,
    MODE_2,
    MODE_3,
    MODE_4,
} manar_mode_t;

typedef enum {
    GEST_UNKNOWN = 0,
    GEST_1,
    GEST_2,
    GEST_3,
    GEST_4,
    GEST_5,
    GEST_6,
} manar_gesture_t;

typedef void (*mode_handler_t)(void);

typedef struct {
    const char *cmd;
    manar_mode_t mode;
    mode_handler_t handler;
} mode_map_t;

static manar_mode_t current_mode = MODE_UNKNOWN;
static manar_mode_t last_mode = MODE_UNKNOWN;
static manar_gesture_t current_gesture = GEST_UNKNOWN;

#if CONFIG_BT_DIRECTED_ADVERTISING
static void bond_find(const struct bt_bond_info *info, void *user_data)
{
	int err;

	/* Filter already connected peers. */
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn) {
			const bt_addr_le_t *dst =
				bt_conn_get_dst(conn_mode[i].conn);

			if (!bt_addr_le_cmp(&info->addr, dst)) {
				return;
			}
		}
	}

	err = k_msgq_put(&bonds_queue, (void *) &info->addr, K_NO_WAIT);
	if (err) {
		printk("No space in the queue for the bond.\n");
	}
}
#endif

static void change_adv_data(manar_mode_t mode) {
	
	/* Select ad/sd based on current mode */
	switch (current_mode) {
	case MODE_2:
		ad = ad_mouse;
		sd = sd_mouse;
		ad_len = ARRAY_SIZE(ad_mouse);
		sd_len = ARRAY_SIZE(sd_mouse);
		break;
	
	case MODE_3:
		ad = ad_gamepad;
		sd = sd_gamepad;
		ad_len = ARRAY_SIZE(ad_gamepad);
		sd_len = ARRAY_SIZE(sd_gamepad);
		break;
	}
	
}

static void advertising_continue(void)
{
	struct bt_le_adv_param adv_param;

#if CONFIG_BT_DIRECTED_ADVERTISING
	bt_addr_le_t addr;

	if (!k_msgq_get(&bonds_queue, &addr, K_NO_WAIT)) {
		char addr_buf[BT_ADDR_LE_STR_LEN];
		int err;

		if (is_adv_running) {
			err = bt_le_adv_stop();
			if (err) {
				printk("Advertising failed to stop (err %d)\n", err);
				return;
			}
			is_adv_running = false;
		}

		adv_param = *BT_LE_ADV_CONN_DIR(&addr);
		adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;

		err = bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);

		if (err) {
			printk("Directed advertising failed to start (err %d)\n", err);
			return;
		}

		bt_addr_le_to_str(&addr, addr_buf, BT_ADDR_LE_STR_LEN);
		printk("Direct advertising to %s started\n", addr_buf);
	} else
#endif
	{
		int err;

		// if (is_adv_running) {
		// 	return;
		// }
		if (is_adv_running) {
			err = bt_le_adv_stop();
			if (err) {
				printk("Advertising failed to stop (err %d)\n", err);
				return;
			}
			is_adv_running = false;
		}

		adv_param = *BT_LE_ADV_CONN;
		adv_param.options |= BT_LE_ADV_OPT_ONE_TIME;
		err = bt_le_adv_start(&adv_param, ad, ad_len,
				  sd, sd_len);
		if (err) {
			printk("Advertising failed to start (err %d)\n", err);
			return;
		}

		printk("Regular advertising started\n");
	}

	is_adv_running = true;
}

static void advertising_start(void)
{
#if CONFIG_BT_DIRECTED_ADVERTISING
	k_msgq_purge(&bonds_queue);
	bt_foreach_bond(BT_ID_DEFAULT, bond_find, NULL);
#endif

	k_work_submit(&adv_work);
}

static void advertising_process(struct k_work *work)
{
	advertising_continue();
}

static void pairing_process(struct k_work *work)
{
	int err;
	struct pairing_data_mitm pairing_data;

	char addr[BT_ADDR_LE_STR_LEN];

	err = k_msgq_peek(&mitm_queue, &pairing_data);
	if (err) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(pairing_data.conn),
			  addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, pairing_data.passkey);

	if (IS_ENABLED(CONFIG_SOC_SERIES_NRF54HX) || IS_ENABLED(CONFIG_SOC_SERIES_NRF54LX)) {
		printk("Press Button 0 to confirm, Button 1 to reject.\n");
	} else {
		printk("Press Button 1 to confirm, Button 2 to reject.\n");
	}
}


static void insert_conn_object(struct bt_conn *conn)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			conn_mode[i].conn = conn;
			conn_mode[i].in_boot_mode = false;

			return;
		}
	}

	printk("Connection object could not be inserted %p\n", conn);
}


static bool is_conn_slot_free(void)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			return true;
		}
	}

	return false;
}


static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	is_adv_running = false;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		if (err == BT_HCI_ERR_ADV_TIMEOUT) {
			printk("Direct advertising to %s timed out\n", addr);
			k_work_submit(&adv_work);
		} else {
			printk("Failed to connect to %s 0x%02x %s\n", addr, err,
			       bt_hci_err_to_str(err));
		}
		return;
	}

	printk("Connected %s\n", addr);

	err = bt_hids_connected(&hids_obj, conn);

	if (err) {
		printk("Failed to notify HID service about connection\n");
		return;
	}

	insert_conn_object(conn);

	if (is_conn_slot_free()) {
		advertising_start();
	}
}


static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

	err = bt_hids_disconnected(&hids_obj, conn);

	if (err) {
		printk("Failed to notify HID service about disconnection\n");
	}

	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn == conn) {
			conn_mode[i].conn = NULL;
			break;
		}
	}

	advertising_start();
}


#ifdef CONFIG_BT_HIDS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d %s\n", addr, level, err,
		       bt_security_err_to_str(err));
	}
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
#ifdef CONFIG_BT_HIDS_SECURITY_ENABLED
	.security_changed = security_changed,
#endif
};


static void hids_pm_evt_handler(enum bt_hids_pm_evt evt,
				struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	size_t i;

	for (i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn == conn) {
			break;
		}
	}

	if (i >= CONFIG_BT_HIDS_MAX_CLIENT_COUNT) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	switch (evt) {
	case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
		printk("Boot mode entered %s\n", addr);
		conn_mode[i].in_boot_mode = true;
		break;

	case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
		printk("Report mode entered %s\n", addr);
		conn_mode[i].in_boot_mode = false;
		break;

	default:
		break;
	}
}

static void hid_deinit(void)
{
	int err = 0;
	printk("%s called!\n", __FUNCTION__);
	err = bt_hids_uninit(&hids_obj);	
	if(err != 0) {
		printk("unable to uninit hid");
	}
}

static void hid_init(void)
{
	int err;
	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *hids_inp_rep;
	static const uint8_t mouse_movement_mask[DIV_ROUND_UP(INPUT_REP_MOVEMENT_LEN, 8)] = {0};

	static const uint8_t report_map_gamepad[] = {
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

	err = bt_hids_init(&hids_obj, &hids_init_param);
	__ASSERT(err == 0, "HIDS initialization failed\n");
}


static void mouse_button_send(uint8_t button_mask)
{
    for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
        if (!conn_mode[i].conn) continue;

        if (conn_mode[i].in_boot_mode) {
            // Boot mode: simpler API
            bt_hids_boot_mouse_inp_rep_send(&hids_obj,
                                            conn_mode[i].conn,
                                            button_mask,
                                            0, 0,
                                            NULL);
            k_msleep(10);
            bt_hids_boot_mouse_inp_rep_send(&hids_obj,
                                            conn_mode[i].conn,
                                            0, 0, 0,
                                            NULL);
        } else {
            // Report mode: use INPUT_REP_BUTTONS_INDEX
            uint8_t report[INPUT_REP_BUTTONS_LEN] = {0};

            report[0] = button_mask;  // Buttons (Left, Right, Middle)
            report[1] = 0x00;         // Padding or reserved
            report[2] = 0x00;         // Padding or reserved

            bt_hids_inp_rep_send(&hids_obj,
                                 conn_mode[i].conn,
                                 INPUT_REP_BUTTONS_INDEX,
                                 report,
                                 INPUT_REP_BUTTONS_LEN,
                                 NULL);

            k_msleep(10);  // simulate press duration

            report[0] = 0x00;  // release buttons
            bt_hids_inp_rep_send(&hids_obj,
                                 conn_mode[i].conn,
                                 INPUT_REP_BUTTONS_INDEX,
                                 report,
                                 INPUT_REP_BUTTONS_LEN,
                                 NULL);
        }
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


static void mouse_movement_handler(struct k_work *work)
{
	struct mouse_pos pos;

	while (!k_msgq_get(&hids_movement_queue, &pos, K_NO_WAIT)) {
		mouse_movement_send(pos.x_val, pos.y_val);
	}
}

static void mouse_button_handler(struct k_work *work)
{
	struct mouse_button sbutton_mask;

	while (!k_msgq_get(&hids_button_queue, &sbutton_mask, K_NO_WAIT)) {
		mouse_button_send(sbutton_mask.button_mask);
	}
}

#if defined(CONFIG_BT_HIDS_SECURITY_ENABLED)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
}


static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	int err;

	struct pairing_data_mitm pairing_data;

	pairing_data.conn    = bt_conn_ref(conn);
	pairing_data.passkey = passkey;

	err = k_msgq_put(&mitm_queue, &pairing_data, K_NO_WAIT);
	if (err) {
		printk("Pairing queue is full. Purge previous data.\n");
	}

	/* In the case of multiple pairing requests, trigger
	 * pairing confirmation which needed user interaction only
	 * once to avoid display information about all devices at
	 * the same time. Passkey confirmation for next devices will
	 * be proccess from queue after handling the earlier ones.
	 */
	if (k_msgq_num_used_get(&mitm_queue) == 1) {
		k_work_submit(&pairing_work);
	}
	pairing_request = true;

}


static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}


static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	pairing_request = false;
	paired_success = true;

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}


static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	struct pairing_data_mitm pairing_data;

	if (k_msgq_peek(&mitm_queue, &pairing_data) != 0) {
		return;
	}

	if (pairing_data.conn == conn) {
		bt_conn_unref(pairing_data.conn);
		k_msgq_get(&mitm_queue, &pairing_data, K_NO_WAIT);
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d %s\n", addr, reason,
	       bt_security_err_to_str(reason));
	
	pairing_request = false;
	paired_success = false;
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif /* defined(CONFIG_BT_HIDS_SECURITY_ENABLED) */


static void num_comp_reply(bool accept)
{
	struct pairing_data_mitm pairing_data;
	struct bt_conn *conn;

	if (k_msgq_get(&mitm_queue, &pairing_data, K_NO_WAIT) != 0) {
		return;
	}

	conn = pairing_data.conn;

	if (accept) {
		bt_conn_auth_passkey_confirm(conn);
		printk("Numeric Match, conn %p\n", conn);
	} else {
		bt_conn_auth_cancel(conn);
		printk("Numeric Reject, conn %p\n", conn);
	}

	bt_conn_unref(pairing_data.conn);

	if (k_msgq_num_used_get(&mitm_queue)) {
		k_work_submit(&pairing_work);
	}
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

		err = k_msgq_put(&hids_movement_queue, &pos, K_NO_WAIT);
		if (err) {
			printk("No space in the queue for button pressed\n");
			return;
		}
		if (k_msgq_num_used_get(&hids_movement_queue) == 1) {
			k_work_submit(&hids_work);
		}
	}
}


void configure_buttons(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Cannot init buttons (err: %d)\n", err);
	}
}


static void bas_notify(void)
{
	uint8_t battery_level = bt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_bas_set_battery_level(battery_level);
}

void uart_init() {
	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}

	printk("UART device found!");

	/* configure interrupt and callback to receive data */
	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}
	uart_irq_rx_enable(uart_dev);

	print_uart("Hello! I'm your echo bot.\r\n");
	print_uart("Tell me something and press enter:\r\n");
}

void handle_mouse_movement(int dx, int dy) {
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

void handle_mouse_button(uint32_t btn_mask) {
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

void init_mode(manar_mode_t mode) {
	switch (mode)
	{
	case MODE_1:

		break;
	
	case MODE_2:
		hid_init();
		break;
	default:
		break;
	}
}

void deinit_last_mode(manar_mode_t mode) {
	switch (mode)
	{
	case MODE_1:

		break;
	
	case MODE_2:
		hid_deinit();
		break;
	default:
		break;
	}
}


void handle_mode_palm_pcb(void) {
    printk("%s", __FUNCTION__);
	last_mode = current_mode;
	deinit_last_mode(last_mode);
	current_mode = MODE_1;
}

void handle_mode_mouse(void) {
    printk("%s", __FUNCTION__);
	last_mode = current_mode;
	deinit_last_mode(last_mode);
	current_mode = MODE_2;
	/* DIS initialized at system boot with SYS_INIT macro. */
	hid_init();
	change_adv_data(current_mode);
	k_work_submit(&adv_work);
}

void handle_mode_gamepad(void) {
	printk("%s", __FUNCTION__);
	last_mode = current_mode;
	deinit_last_mode(last_mode);
	current_mode = MODE_3;
	change_adv_data(current_mode);
	k_work_submit(&adv_work);
}

void handle_mode_vr(void) {
	printk("%s", __FUNCTION__);
	last_mode = current_mode;
	deinit_last_mode(last_mode);
	current_mode = MODE_4;
}

void handle_gest_1(void) {
    printk("%s", __FUNCTION__);
	switch (current_mode) {
        case MODE_1:
            printk("Gesture 1 triggered in MODE_1\n");
            break;
			
		case MODE_2:
            printk("Gesture 1 triggered in MODE_2\n");
			// handle_mouse_movement(+MOVEMENT_SPEED, 0);
			handle_mouse_button(MOUSE_BTN_LEFT);
            break;

        case MODE_3:
            printk("Gesture 1 triggered in MODE_3\n");
            break;

        case MODE_4:
            printk("Gesture 1 triggered in MODE_4\n");
            break;

        default:
            printk("Gesture 1 triggered in unknown mode\n");
            break;
    }

}

void handle_gest_2(void) {
    printk("%s", __FUNCTION__);
	switch (current_mode) {
        case MODE_1:
            printk("Gesture 2 triggered in MODE_1\n");
            break;
			
			case MODE_2:
            printk("Gesture 2 triggered in MODE_2\n");
			// handle_mouse_movement(-MOVEMENT_SPEED, 0);
			handle_mouse_button(MOUSE_BTN_RIGHT);
            break;

        case MODE_3:
            printk("Gesture 2 triggered in MODE_3\n");
            break;

        case MODE_4:
            printk("Gesture 2 triggered in MODE_4\n");
            break;

        default:
            printk("Gesture 2 triggered in unknown mode\n");
            break;
    }
}

void handle_gest_3(void) {
    printk("%s", __FUNCTION__);
    switch (current_mode) {
        case MODE_1:
            printk("Gesture 3 triggered in MODE_1\n");
            break;
			
        case MODE_2:
            printk("Gesture 3 triggered in MODE_2\n");
			handle_mouse_movement(0, +MOVEMENT_SPEED);
            break;

        case MODE_3:
            printk("Gesture 3 triggered in MODE_3\n");
            break;

        case MODE_4:
            printk("Gesture 3 triggered in MODE_4\n");
            break;

        default:
            printk("Gesture 3 triggered in unknown mode\n");
            break;
    }
}

void handle_gest_4(void) {
    printk("%s", __FUNCTION__);
	switch (current_mode) {
        case MODE_1:
            printk("Gesture 4 triggered in MODE_1\n");
			break;

        case MODE_2:
            printk("Gesture 4 triggered in MODE_2\n");
			handle_mouse_movement(0, -MOVEMENT_SPEED);
            break;

        case MODE_3:
            printk("Gesture 4 triggered in MODE_3\n");
            break;

        case MODE_4:
            printk("Gesture 4 triggered in MODE_4\n");
            break;

        default:
            printk("Gesture 4 triggered in unknown mode\n");
            break;
    }
}

void handle_gest_5(void) {
    printk("%s", __FUNCTION__);
	switch (current_mode) {
        case MODE_1:
            printk("Gesture 5 triggered in MODE_1\n");
            break;

        case MODE_2:
            printk("Gesture 5 triggered in MODE_2\n");
            break;

        case MODE_3:
            printk("Gesture 5 triggered in MODE_3\n");
            break;

        case MODE_4:
            printk("Gesture 5 triggered in MODE_4\n");
            break;

        default:
            printk("Gesture 5 triggered in unknown mode\n");
            break;
    }
}

void handle_gest_6(void) {
    printk("%s", __FUNCTION__);
	switch (current_mode) {
        case MODE_1:
            printk("Gesture 6 triggered in MODE_1\n");
            break;

        case MODE_2:
            printk("Gesture 6 triggered in MODE_2\n");
            break;

        case MODE_3:
            printk("Gesture 6 triggered in MODE_3\n");
            break;

        case MODE_4:
            printk("Gesture 6 triggered in MODE_4\n");
            break;

        default:
            printk("Gesture 6 triggered in unknown mode\n");
            break;
    }
}

const mode_map_t command_table[] = {
    {"M1", MODE_1, handle_mode_palm_pcb},
    {"M2", MODE_2, handle_mode_mouse},
    {"M3", MODE_3, handle_mode_gamepad},
    {"M4", MODE_4, handle_mode_vr},
    {NULL, MODE_UNKNOWN, NULL},
};

typedef struct {
    const char *cmd;
    manar_gesture_t gesture;
    mode_handler_t handler;
} gesture_map_t;

const gesture_map_t gesture_table[] = {
    {"1", GEST_1, handle_gest_1},
    {"2", GEST_2, handle_gest_2},
    {"3", GEST_3, handle_gest_3},
    {"4", GEST_4, handle_gest_4},
    {"5", GEST_5, NULL},
    {"6", GEST_6, NULL},
    {NULL, GEST_UNKNOWN, NULL},
};

manar_mode_t get_mode_from_string(const char *cmd) {
    for (int i = 0; command_table[i].cmd != NULL; i++) {
        if (strcmp(cmd, command_table[i].cmd) == 0) {
            return command_table[i].mode;
        }
    }
    return MODE_UNKNOWN;
}

manar_gesture_t get_gesture_from_string(const char *cmd) {
    for (int i = 0; gesture_table[i].cmd != NULL; i++) {
        if (strcmp(cmd, gesture_table[i].cmd) == 0) {
            return gesture_table[i].gesture;
        }
    }
    return GEST_UNKNOWN;
}

void process_uart_input(const char *input) {
    // Try to match as command
    for (int i = 0; command_table[i].cmd != NULL; i++) {
        if (strcmp(input, command_table[i].cmd) == 0) {
            if (command_table[i].handler) {
                command_table[i].handler();
            } else {
                printk("No handler for command: %s\n", input);
            }
            return;
        }
    }

    // Try to match as gesture
    for (int i = 0; gesture_table[i].cmd != NULL; i++) {
        if (strcmp(input, gesture_table[i].cmd) == 0) {
			current_gesture = gesture_table[i].gesture;
            if (gesture_table[i].handler) {
                gesture_table[i].handler();
            } else {
                printk("No handler for gesture: %s\n", input);
            }
            return;
        }
    }

    // Not found in either table
    printk("Unknown input: %s\n", input);
}

int main(void)
{
	int err;

	printk("Starting Bluetooth Peripheral HIDS mouse example\n");

	if (IS_ENABLED(CONFIG_BT_HIDS_SECURITY_ENABLED)) {
		err = bt_conn_auth_cb_register(&conn_auth_callbacks);
		if (err) {
			printk("Failed to register authorization callbacks.\n");
			return 0;
		}

		err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
		if (err) {
			printk("Failed to register authorization info callbacks.\n");
			return 0;
		}
	}

	/* UART init */
	uart_init();
	printk("uart_init called!\n");

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	k_work_init(&hids_work, mouse_movement_handler);
	k_work_init(&hids_work_button, mouse_button_handler);
	
	k_work_init(&adv_work, advertising_process);

	if (IS_ENABLED(CONFIG_BT_HIDS_SECURITY_ENABLED)) {
		k_work_init(&pairing_work, pairing_process);
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	// advertising_start();

	// configure_buttons();

	char tx_buf[MSG_SIZE];

	while (1) {
		while (k_msgq_get(&uart_msgq, &tx_buf, K_SECONDS(5)) == 0) {
			print_uart("Echo: ");
			print_uart(tx_buf);
			print_uart("\r\n");

			printk("Echo: ");
			printk(tx_buf);
			printk("\r\n");

			process_uart_input(tx_buf);

		}
		/* Battery level simulation */
		bas_notify();
		printk("Running\n");
		if(pairing_request)
		{
			k_sleep(K_MSEC(5000));
			num_comp_reply(true);
			pairing_request = false;
		}
	}
}

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if(c == '\n') {
			//do nothing
			continue;
		}
		if ((c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

/*
 * Print a null-terminated string character by character to the UART interface
 */
void print_uart(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}