#include "Mode2_BluetoothMouse.h"
#include <BLEDevice.h>
#include "gesture_reader.h"
#include "esp_mac.h"
#include "esp_err.h"
BleMouse bleMouse("Mode 2, Gecko Mouse2", "Adstrum", 100);  // Initialize BLE Mouse
bool isBleMouseStarted = false;  // Track if BLE Mouse is running

void handleMode2() {
    // Only start BLE Mouse if it hasn't been started already
    if (!isBleMouseStarted) {
        Serial.println("Starting BLE Mouse...");
        // System 1 mac
        // uint8_t new_mac[8] = {0x24, 0x0A, 0xC4, 0xAB, 0xCD, 0xEF};
        // System 2 mac
        uint8_t new_mac[8] = {0xA4,0x5E,0x60,0x3B,0x7D,0x21};
        esp_base_mac_addr_set(new_mac);
        bleMouse.begin();
        isBleMouseStarted = true;  // Mark BLE Mouse as started
    }

    int gesture = getGesture();
    performMouseAction(gesture);
    
    // Wait for Serial input (gesture number)
    // if (Serial.available() > 0) {
    //     gesture = Serial.parseInt(); // Read the input number from Serial Monitor

    //     // Check if the gesture is valid (between 1 and 6)
    //     if (gesture >= 1 && gesture <= 6) {
    //         performMouseAction(gesture);
    //     } else {
    //         Serial.println("Invalid input. Please input a number between 1 and 6.");
    //     }
    // }
}

// Function to safely stop BLE Mouse
void stopBleMouse() {
    if (isBleMouseStarted) {
        Serial.println("Stopping BLE Mouse...");

        // Stop advertising
        BLEDevice::getAdvertising()->stop();

        // Disconnect any connected clients
        // BLEServer* pServer = BLEDevice::getServer();
        // if (pServer) {
        //     pServer->disconnect(0); // Assuming single connection
        // }

        // Deinitialize BLE
        BLEDevice::deinit();
        bleMouse.end();

        isBleMouseStarted = false;
        Serial.println("BLE Mouse stopped.");
    }
}


// Perform mouse actions based on input
void performMouseAction(int gesture) {
    switch (gesture) {
        case 4:
            Serial.println("Left Click");
            bleMouse.click(MOUSE_LEFT);
            break;
        case 3:
            Serial.println("Right Click");
            bleMouse.click(MOUSE_RIGHT);
            break;
        case 5:
            Serial.println("Double Click");
            bleMouse.click(MOUSE_LEFT);
            delay(100);
            bleMouse.click(MOUSE_LEFT);
            break;
        case 1:
            Serial.println("Scroll Up");
            bleMouse.move(0, 0, 1);  // Scroll up
            break;
        case 2:
            Serial.println("Scroll Down");
            bleMouse.move(0, 0, -1);  // Scroll down
            break;
        case 6:
            Serial.println("Middle Click");
            bleMouse.click(MOUSE_MIDDLE);
            break;
        default:
            Serial.println("Invalid input.");
            break;
    }
}
