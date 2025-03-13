#include "Mode3_BluetoothGamepad.h"
// #include <BLEDevice.h>
#include "gesture_reader.h"
#include "esp_mac.h"
#include "esp_err.h"
BleGamepad bleGamepad("Mode 3, Gecko Gamepad2", "Adstrum", 100);;  // Initialize BLE Gamepad
bool isBleGamepadStarted = false;  // Track if BLE Gamepad is running

void handleMode3() {
    // Only start BLE Gamepad if it hasn't been started already
    if (!isBleGamepadStarted) {
        Serial.println("Starting BLE Gamepad...");
        // System 1 mac
        // uint8_t new_mac[8] = {0x24, 0x0A, 0xC4, 0x78, 0x56, 0x34};
        // System 2 mac
        uint8_t new_mac[8] = {0xBC,0x92,0x6B,0x1F,0x34,0xAA};
        
        esp_base_mac_addr_set(new_mac);
        bleGamepad.begin();
        isBleGamepadStarted = true;  // Mark BLE Gamepad as started
    }

    int gesture = getGesture();
    performGamepadAction(gesture);
    
    // Wait for Serial input (gesture number)
    // if (Serial.available() > 0) {
    //     gesture = Serial.parseInt(); // Read the input number from Serial Monitor

    //     // Check if the gesture is valid (between 1 and 6)
    //     if (gesture >= 1 && gesture <= 6) {
    //         performGamepadAction(gesture);
    //     } else {
    //         Serial.println("Invalid input. Please input a number between 1 and 6.");
    //     }
    // }
}

// Function to safely stop BLE Gamepad
void stopBleGamepad() {
    if (isBleGamepadStarted) {
        Serial.println("Stopping BLE Gamepad...");

        // Stop advertising
        BLEDevice::getAdvertising()->stop();

        // Disconnect any connected clients
        // BLEServer* pServer = BLEDevice::getServer();
        // if (pServer) {
        //     pServer->disconnect(0); // Assuming single connection
        // }

        // Deinitialize BLE
        BLEDevice::deinit();
        bleGamepad.end();

        isBleGamepadStarted = false;
        Serial.println("BLE Gamepad stopped.");
    }
}


// Perform Gamepad actions based on input
void performGamepadAction(int gesture) {
    switch (gesture) {
        case 1:
            Serial.println("DPAD LEFT");
            // bleGamepad.pressSpecialButton(DPAD_LEFT);
            // bleGamepad.press(BUTTON_15);
            bleGamepad.setHat1(HAT_LEFT);
            // delay(500);
            // bleGamepad.releaseSpecialButton(DPAD_LEFT);
            // bleGamepad.pressSpecialButton(START_BUTTON);
            break;
        case 2:
            Serial.println("DPAD RIGHT");
            // bleGamepad.pressSpecialButton(DPAD_RIGHT);
            // bleGamepad.press(BUTTON_16);
            bleGamepad.setHat1(HAT_RIGHT);
            // delay(500);
            // bleGamepad.releaseSpecialButton(DPAD_RIGHT);
            break;
        case 3:
            Serial.println("Cross/ Interact");
            bleGamepad.press(BUTTON_1);
            break;
        case 4:
            Serial.println("Secondary Action");
            bleGamepad.pressSpecialButton(HOME_BUTTON);
            delay(50);
            bleGamepad.releaseSpecialButton(HOME_BUTTON);
            break;
        case 5:
            Serial.println("Shoulder Button");
            bleGamepad.pressSpecialButton(HOME_BUTTON);
            delay(50);
            bleGamepad.releaseSpecialButton(HOME_BUTTON);
            break;
        case 6:
            Serial.println("Trigger Button");
            bleGamepad.setRightTrigger(0xFFFF);

            break;
        default:
            // bleGamepad.release(BUTTON_15);
            // bleGamepad.release(BUTTON_16);
            bleGamepad.setHat1(HAT_CENTERED);

            bleGamepad.release(BUTTON_1);
            Serial.println("Invalid input.");
            break;
    }
}
