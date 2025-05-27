#ifndef BLE_GAMEPAD_H
#define BLE_GAMEPAD_H

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

void ble_gamepad_button_changed(uint32_t button_state, uint32_t has_changed);
void* ble_gamepad_return_hids_object();
void ble_gamepad_report_init();
void ble_gamepad_handle_buttons(int val);
void ble_gamepad_hid_init(void);
void ble_gamepad_hid_unint(void);

#endif //BLE_GAMEPAD_H