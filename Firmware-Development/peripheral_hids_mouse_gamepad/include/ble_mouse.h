#ifndef BLE_MOUSE_H
#define BLE_MOUSE_H

#include "bluetooth_config.h"

void ble_mouse_button_changed(uint32_t button_state, uint32_t has_changed);
void* ble_mouse_return_hids_object();
void ble_mouse_handle_movement(int dx, int dy);
void ble_mouse_handle_button(uint32_t btn_mask);
void ble_mouse_hid_init(void);
void ble_mouse_hid_unint(void);
ble_config_t ble_mouse_get_ad_sd_data();

#endif //BLE_MOUSE_H