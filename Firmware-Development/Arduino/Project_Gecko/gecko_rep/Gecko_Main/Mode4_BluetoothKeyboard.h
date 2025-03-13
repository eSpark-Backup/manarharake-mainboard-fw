#ifndef MODE4_BLUETOOTH_KEYBOARD_H
#define MODE4_BLUETOOTH_KEYBOARD_H

#include <Arduino.h>
#include <BleKeyboard.h>

extern BleKeyboard bleKeyboard;
extern bool isBleKeyboardStarted;  // Flag to track BLE Mouse state

void handleMode4();
void performKeyboardAction(int gesture);
void stopBleKeyboard();  // <-- Add this missing declaration

#endif
