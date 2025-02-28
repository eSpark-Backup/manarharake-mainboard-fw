
#include <BLEDevice.h> 
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "base64.hpp"

// #define JSON_PACKET_TEMPLATE "{\"ADC\":{\"Value_1\":%d,\"Value_2\":%d,\"Value_3\":%d,\"Value_4\":%d,\"Value_5\":%d,\"Value_6\":%d,\"Value_7\":%d,\"Value_8\":%d},\"Gyro\":{\"X\":%d,\"Y\":%d,\"Z\":%d},\"Acc\":{\"X\":%d,\"Y\":%d,\"Z\":%d}}\r\n"
// #define JSON_PACKET_TEMPLATE "{\"ADC\":{\"Value_1\":%d,\"Value_2\":%d,\"Value_3\":%d,\"Value_4\":%d,\"Value_5\":%d,\"Value_6\":%d,\"Value_7\":%d,\"Value_8\":%d}\r\n"
#define JSON_PACKET_TEMPLATE_FLOAT "{\"ADC\":{\"Value_0\":%.2f,\"Value_1\":%.2f,\"Value_2\":%.2f,\"Value_3\":%.2f,\"Value_4\":%.2f,\"Value_5\":%.2f,\"Value_6\":%.2f,\"Value_7\":%.2f}\r\n"
char jsonString[256];

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
// #define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
// #define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "00000000-0000-0000-0000-000000000000"
#define CHARACTERISTIC_UUID_RX "00000000-0000-0000-0000-000000000001"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  };
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

uint8_t zeroUUID[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void setup() {
  Serial.begin(115200);
  // Create the BLE Device
  BLEDevice::init("ESP_GATTS_DEMO");
  
  BLEDevice::setMTU(517);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  // pTxCharacteristic = pService->createCharacteristic(
  //       BLEUUID(zeroUUID, 16, true),
  //       BLECharacteristic::PROPERTY_NOTIFY
  //   );

  pTxCharacteristic->addDescriptor(new BLE2902());

  // BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

  // pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  int adcValues[8];
   adcValues[0] = random(0, 4096); // Random value for ADC between 0 and 1023
   adcValues[1] = random(0, 4096);
   adcValues[2] = random(0, 4096);
   adcValues[3] = random(0, 4096);
   adcValues[4] = random(0, 4096);
   adcValues[5] = random(0, 4096);
   adcValues[6] = random(0, 4096);
   adcValues[7] = random(0, 4096);

    // adcValues[0] = analogRead(36);  // ADC1_CH0 (GPIO36)
    // adcValues[1] = analogRead(39);  // ADC1_CH3 (GPIO39)
    // adcValues[2] = analogRead(34);  // ADC1_CH6 (GPIO34)
    // adcValues[3] = analogRead(35);  // ADC1_CH7 (GPIO35)
    // adcValues[4] = analogRead(32);  // ADC1_CH4 (GPIO32)
    // adcValues[5] = analogRead(33);  // ADC1_CH5 (GPIO33)
    // adcValues[6] = analogRead(25);  // ADC2_CH8 (GPIO25) - Wi-Fi Conflict
    // adcValues[7] = analogRead(26);  // ADC2_CH9 (GPIO26) - Wi-Fi Conflict

  // int gyroX = random(-1000, 1000); // Random value for gyro between -1000 and 1000
  // int gyroY = random(-1000, 1000);
  // int gyroZ = random(-1000, 1000);

  // int accX = random(-1000, 1000);  // Random value for accelerometer between -1000 and 1000
  // int accY = random(-1000, 1000);
  // int accZ = random(-1000, 1000);

  snprintf(jsonString, sizeof(jsonString), JSON_PACKET_TEMPLATE_FLOAT,
         adcValues[0]/4096.0f, adcValues[1]/4096.0f, adcValues[2]/4096.0f, adcValues[3]/4096.0f,
         adcValues[4]/4096.0f, adcValues[5]/4096.0f, adcValues[6]/4096.0f, adcValues[7]/4096.0f
        //  gyroX, gyroY, gyroZ, accX, accY, accZ);
  );

  int jsonStringLen = strlen(jsonString);
  int encodedLength = 4*(jsonStringLen/3);
  char encodedString[encodedLength + 1];
  encode_base64( (unsigned char *)jsonString, jsonStringLen,  (unsigned char *)encodedString);

  if (deviceConnected) {

    pTxCharacteristic->setValue(encodedString);
    pTxCharacteristic->notify();
    Serial.println(encodedString);
    txValue++;
    delay(50);  // bluetooth stack will go into congestion, if too many packets are sent
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
