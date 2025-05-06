#include "Mode1_UART.h"
#include "gesture_reader.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_mac.h"
#include "esp_err.h"
#include <EEPROM.h>

HardwareSerial mySerial(1);  // Use Serial1 for UART communication

bool uartServiceStarted = false;

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define JSON_PACKET_TEMPLATE "{\"ADC\":{\"Value_1\":%d,\"Value_2\":%d,\"Value_3\":%d,\"Value_4\":%d,\"Value_5\":%d,\"Value_6\":%d,\"Value_7\":%d,\"Value_8\":%d},\"Gyro\":{\"X\":%d,\"Y\":%d,\"Z\":%d},\"Acc\":{\"X\":%d,\"Y\":%d,\"Z\":%d}}\r\n"
char jsonString[256];

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
      }

      Serial.println();
      Serial.println("*********");
    }
  }
};

void handleMode1() {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n'); // Read the full line
      input.trim(); // Remove any leading or trailing whitespace

      if (input.startsWith("T1 ")) {
          threshold1 = input.substring(3).toInt();
          // EEPROM.put(TH_1_ADD, threshold1);
          // EEPROM.commit();  // Save changes to flash
          write16BitValue(TH_1_ADD, threshold1);
          Serial.print("Threshold1 set to: ");
          Serial.println(threshold1);
        } else if (input.startsWith("T2 ")) {
          threshold2 = input.substring(3).toInt();
          // EEPROM.put(TH_2_ADD, threshold2);
          // EEPROM.commit();  // Save changes to flash
          write16BitValue(TH_2_ADD, threshold2);
          Serial.print("Threshold2 set to: ");
          Serial.println(threshold2);
      } else {
          Serial.println("Invalid command! Use 'T1 <value>' or 'T2 <value>'.");
      }
    }
    int gestre = getGesture();
    static uint32_t last_command_time = 0;
    // if (millis() - last_command_time >= 1000) { 
        sendCommand(gestre);
      //   last_command_time = millis(); 
      // }
      
    // static int lastGesture = 0;
    // if(lastGesture != gestre)
    // {
    //   sendCommand(gestre);
    //   lastGesture = gestre;
    // }

    if(!uartServiceStarted)
    {
      // System 1 mac
      // uint8_t custom_mac[6] = { 0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56 };
      
      // System 2 mac
      uint8_t custom_mac[6] = { 0x00,0x1A,0x2B,0x3C,0x4D,0x5E };

      esp_err_t err = esp_base_mac_addr_set(custom_mac);
      if (err == ESP_OK) {
          Serial.println("Base MAC address set successfully\n");
      } else {
          Serial.print("Failed to set base MAC address, error: ");
          Serial.println(err);
      }
      BLEDevice::init("Mode 1, Gecko2");
  
      BLEDevice::setMTU(517);
    
      // Create the BLE Server
      pServer = BLEDevice::createServer();
      pServer->setCallbacks(new MyServerCallbacks());
    
      // Create the BLE Service
      BLEService *pService = pServer->createService(SERVICE_UUID);
    
      // Create a BLE Characteristic
      pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    
      pTxCharacteristic->addDescriptor(new BLE2902());
    
      BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    
      pRxCharacteristic->setCallbacks(new MyCallbacks());
    
      // Start the service
      pService->start();
    
      // Start advertising
      pServer->getAdvertising()->start();
      Serial.println("Waiting a client connection to notify...");
      uartServiceStarted = true;
    }

    int adcValues[8];
    envelope_data l_env_data = getEnvelope();
    adcValues[0] = l_env_data.env1; // Random value for ADC between 0 and 1023
    adcValues[1] = l_env_data.env2;
    adcValues[2] = 0;
    adcValues[3] = 0;
    adcValues[4] = 0;
    adcValues[5] = 0;
    adcValues[6] = 0;
    adcValues[7] = 0;

    int gyroX = 0; // Random value for gyro between -1000 and 1000
  int gyroY = 0;
  int gyroZ = 0;

  int accX = 0;  // Random value for accelerometer between -1000 and 1000
  int accY = 0;
  int accZ = 0;

  snprintf(jsonString, sizeof(jsonString), JSON_PACKET_TEMPLATE,
         adcValues[0], adcValues[1], adcValues[2], adcValues[3],
         adcValues[4], adcValues[5], adcValues[6], adcValues[7],
         gyroX, gyroY, gyroZ, accX, accY, accZ);


  if (deviceConnected) {
    pTxCharacteristic->setValue(jsonString);
    pTxCharacteristic->notify();
    // txValue++;
    // delay(1000);  // bluetooth stack will go into congestion, if too many packets are sent
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}

void stopBleUart() {
    if (uartServiceStarted) {
        Serial.println("Stopping BLE UART...");

        // Stop advertising
        BLEDevice::getAdvertising()->stop();

        // Disconnect any connected clients
        // BLEServer* pServer = BLEDevice::getServer();
        // if (pServer) {
        //     pServer->disconnect(0); // Assuming single connection
        // }

        // Deinitialize BLE
        BLEDevice::deinit();

        uartServiceStarted = false;
        Serial.println("BLE UART stopped.");
    }
}

// Function to send corresponding grip command based on input
void sendCommand(int gesture) {
    String command;

    // Map gestures to corresponding grip commands
    switch (gesture) {
        case 1: command = "G0\r\n"; break;  // FIST
        case 2: command = "G4\r\n"; break;  // PALM
        case 3: command = "G1\r\n"; break;  // THUMBS
        case 4: command = "G5\r\n"; break;  // POINT
        case 5: command = "G2\r\n"; break;  // PINCH
        case 6: command = "G3\r\n"; break;  // TRIPOD
        default: break;
    }

    // Send the command to the Palm PCB via UART
    static int lastGesture = 0;
    if(lastGesture != gesture)
    {
      mySerial.println(command);
      lastGesture = gesture;
      // Print the command to Serial Monitor for feedback
      Serial.print("Sending command: ");
      Serial.println(command);
    }
    
}
