#include "Mode4_BluetoothKeyboard.h"
#include "gesture_reader.h"
#include "esp_mac.h"
#include "esp_err.h"
BleKeyboard bleKeyboard("Mode 4, Gecko Mobile2", "Adstrum", 100);;  // Initialize BLE Keyboard
bool isBleKeyboardStarted = false;  // Track if BLE Keyboard is running

void handleMode4() {
    // Only start BLE Keyboard if it hasn't been started already
    if (!isBleKeyboardStarted) {
        Serial.println("Starting BLE Keyboard...");
        // System 1
        // uint8_t new_mac[8] = {0x9A,0x3F,0xB2,0x8D,0x65,0xE1};
        // System 2
        uint8_t new_mac[8] = {0x34,0xE4,0x83,0xF7,0x67,0x5E};
        esp_base_mac_addr_set(new_mac);
        bleKeyboard.begin();
        isBleKeyboardStarted = true;  // Mark BLE Keyboard as started
    }

    int gesture = getGesture();
    performKeyboardAction(gesture);
    
    // Wait for Serial input (gesture number)
    // if (Serial.available() > 0) {
    //     gesture = Serial.parseInt(); // Read the input number from Serial Monitor

    //     // Check if the gesture is valid (between 1 and 6)
    //     if (gesture >= 1 && gesture <= 6) {
    //         performKeyboardAction(gesture);
    //     } else {
    //         Serial.println("Invalid input. Please input a number between 1 and 6.");
    //     }
    // }
}

// Function to safely stop BLE Keyboard
void stopBleKeyboard() {
    if (isBleKeyboardStarted) {
        Serial.println("Stopping BLE Keyboard...");

        // Stop advertising
        // BLEDevice::getAdvertising()->stop();

        // Disconnect any connected clients
        // BLEServer* pServer = BLEDevice::getServer();
        // if (pServer) {
        //     pServer->disconnect(0); // Assuming single connection
        // }

        // Deinitialize BLE
        // BLEDevice::deinit();
        bleKeyboard.end();

        isBleKeyboardStarted = false;
        Serial.println("BLE Keyboard stopped.");
    }
}


// Perform Keyboard actions based on input
void performKeyboardAction(int gesture) {
    switch (gesture) {
        case 1:
            Serial.println("KEY_MEDIA_VOLUME_DOWN");
            bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
            break;
        case 2:
            Serial.println("KEY_MEDIA_VOLUME_UP");
            bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
            break;
        case 3:
            Serial.println("KEY_MEDIA_CALCULATOR");
            bleKeyboard.write(KEY_MEDIA_CALCULATOR);
            break;
        case 4:
            Serial.println("Secondary Action");
            // bleKeyboard.write(KEY_RETURN);
            break;
        case 5:
            Serial.println("Shoulder Button");
            // bleKeyboard.write(KEY_RETURN);
            break;
        case 6:
            Serial.println("Trigger Button");
            // bleKeyboard.write(KEY_RETURN);
            break;
        default:
            Serial.println("Invalid input.");
            break;
    }
}
