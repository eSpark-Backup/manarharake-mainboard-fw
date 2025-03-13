#ifndef MODE2_BLUETOOTH_MOUSE_H
#define MODE2_BLUETOOTH_MOUSE_H

#include <Arduino.h>
#include <BleMouse.h>

extern BleMouse bleMouse;
extern bool isBleMouseStarted;  // Flag to track BLE Mouse state

void handleMode2();
void performMouseAction(int gesture);
void stopBleMouse();  // <-- Add this missing declaration

#endif
