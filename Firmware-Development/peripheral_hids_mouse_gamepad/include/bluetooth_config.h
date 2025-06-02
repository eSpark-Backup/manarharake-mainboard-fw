#ifndef BLUETOOTH_CONFIG_H
#define BLUETOOTH_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <bluetooth/services/hids.h>


#define MOUSE_DEVICE_NAME     "Nordic_HIDS_mouse"
#define MOUSE_DEVICE_NAME_LEN (sizeof(MOUSE_DEVICE_NAME) - 1)

#define GAMEPAD_DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define GAMEPAD_DEVICE_NAME_LEN (sizeof(MOUSE_DEVICE_NAME) - 1)

#define BASE_USB_HID_SPEC_VERSION   0x0101

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

extern struct bt_conn_auth_cb conn_auth_callbacks;
extern struct bt_conn_auth_info_cb conn_auth_info_callbacks;

typedef struct {
    struct bt_data *ad_ptr;
    size_t ad_len;
    struct bt_data *sd_ptr;
    size_t sd_len;
}ble_config_t;

struct pairing_data_mitm {
	struct bt_conn *conn;
	unsigned int passkey;
};

struct conn_mode {
    struct bt_conn *conn;
    bool in_boot_mode;
};

extern struct conn_mode conn_mode[CONFIG_BT_HIDS_MAX_CLIENT_COUNT];

extern bool piaring_success;

// extern struct conn_mode {
// 	struct bt_conn *conn;
// 	bool in_boot_mode;
// } conn_mode[CONFIG_BT_HIDS_MAX_CLIENT_COUNT];

extern struct k_msgq mitm_queue;

#ifdef __cplusplus
extern "C" {
#endif

void initialize_bluetooth(void* hids_obj_ptr);
void advertising_start(void);
void configure_buttons(void (*func_ptr)(int32_t button_state, uint32_t has_changed));
void delete_bonded_devices(void);
void hids_pm_evt_handler(enum bt_hids_pm_evt evt,
    struct bt_conn *conn);
void ble_config_set_ad_sd_data(ble_config_t sd_ad_data);
bool ble_config_ble_paired(void);


#ifdef __cplusplus
}
#endif

#endif //BLUETOOTH_CONFIG_H