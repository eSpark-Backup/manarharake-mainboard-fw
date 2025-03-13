#ifndef MODE3_BLUETOOTH_GAMEPAD_H
#define MODE3_BLUETOOTH_GAMEPAD_H

#include <Arduino.h>
#include <BleGamepad.h>

extern BleGamepad bleGamepad;
extern bool isBleGamepadStarted;  // Flag to track BLE Mouse state

void handleMode3();
void performGamepadAction(int gesture);
void stopBleGamepad();  // <-- Add this missing declaration

#endif
