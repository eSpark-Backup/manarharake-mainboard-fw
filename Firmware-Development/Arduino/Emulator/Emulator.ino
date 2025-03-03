
#include <BLEDevice.h> 
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "base64.hpp"

// #define JSON_PACKET_TEMPLATE_FLOAT "{\"ADC\":{\"Value_1\":%.2f,\"Value_2\":%.2f,\"Value_3\":%.2f,\"Value_4\":%.2f,\"Value_5\":%.2f,\"Value_6\":%.2f,\"Value_7\":%.2f,\"Value_8\":%.2f}}"
#define JSON_PACKET_TEMPLATE_FLOAT_V2 "{\"FIL\":[%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f], \"RAW\":[%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f]}\r\n"
char jsonString[256];

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
bool notificationsEnabled = false;
BLE2902* p2902Descriptor = new BLE2902();

bool calibrated = false;
int calibrationStartTime = 0;
bool calibStarted = false;
int lastNotifyTime = 0;

#define NUM_CHANNELS 8  // Number of ADC channels
#define WINDOW_SIZE 100 // Number of samples for rolling average
#define ADC_MAX 4095.0  // ESP32 ADC 12-bit maximum value

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
// #define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
// #define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "00000000-0000-0000-0000-000000000000"

int adcPins[NUM_CHANNELS] = {36, 39, 34, 35, 32, 33, 25, 26};  // ADC pins
int adcBuffer[NUM_CHANNELS][WINDOW_SIZE] = {0}; // Buffers for each channel
int bufferIndex[NUM_CHANNELS] = {0}; // Current index in each buffer
int centeredADCValues[NUM_CHANNELS] = {0}; // Current index in each buffer
int maxADCValues[NUM_CHANNELS] = {0};
long sum[NUM_CHANNELS] = {0}; // Sum for rolling average

float prevHighPass[NUM_CHANNELS] = {0}; // Store previous high-pass values
float prevCentered[NUM_CHANNELS] = {0}; // Store previous high-pass values
float prevLowPass[NUM_CHANNELS] = {0};  // Store previous low-pass values

float getMean(int channel) {
    return sum[channel] / (float)WINDOW_SIZE;
}
float getCenteredValue(int rawValue, int channel) {
    float meanValue = getMean(channel);  // Compute mean from the last 100 samples
    return rawValue - meanValue;  // Centered value
}

void updateADCValues() {
    for (int i = 0; i < NUM_CHANNELS; i++) {
        int newValue = analogRead(adcPins[i]);

        sum[i] -= adcBuffer[i][bufferIndex[i]]; // Remove oldest value
        adcBuffer[i][bufferIndex[i]] = newValue; // Store new value
        sum[i] += newValue; // Add new value to sum

        bufferIndex[i] = (bufferIndex[i] + 1) % WINDOW_SIZE; // Circular buffer

        prevCentered[i] = centeredADCValues[i];
        centeredADCValues[i] = getCenteredValue(newValue, i);

        // Update max ADC value ever read
        float absCenteredValue = (fabs(centeredADCValues[i]));
        if (calibStarted && ( absCenteredValue > maxADCValues[i])) {
            maxADCValues[i] = absCenteredValue;
        }
    }
}


float normalize(float value, int channel) {
    return value / maxADCValues[channel];
}

float highPassFilter(float input, int channel, float alpha = 0.9) {
    float output = alpha * (prevHighPass[channel] + input - prevCentered[channel] );
    prevHighPass[channel] = output;  // Store previous value for next iteration
    return output;
}

float lowPassFilter(float input, int channel, float alpha = 0.1) {
    prevLowPass[channel] = alpha * input + (1 - alpha) * prevLowPass[channel];
    return prevLowPass[channel];
}

float rectifySignal(float input) {
    return fabs(input);  // Absolute value to remove negative values
}

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

class MyDescriptorCallbacks : public BLEDescriptorCallbacks {
  void onWrite(BLEDescriptor* pDescriptor) {
    uint8_t* data = pDescriptor->getValue();
    if (data[0] == 0x01) {  // Notifications enabled (0x01 = Notify, 0x02 = Indicate)
      Serial.println("Notifications enabled");
      notificationsEnabled = true;
    } else {
      Serial.println("Notifications disabled");
      notificationsEnabled = false;
    }
  }
};

void setup() {
  Serial.begin(921600);
  BLEDevice::init("ESP_GATTS_DEMO_V2");
  
  BLEDevice::setMTU(247);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  p2902Descriptor->setNotifications(true);
  p2902Descriptor->setCallbacks(new MyDescriptorCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic = pService->createCharacteristic(BLEUUID((uint16_t)0x0000), BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(p2902Descriptor);

  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  int adcValues[8];
  float filterValues[8];
  updateADCValues();

  for (int i = 0; i < NUM_CHANNELS; i++) {
        float centered = centeredADCValues[i]; 
        float highPass = highPassFilter(centered, i);  // Apply high-pass filter
        float lowPass = lowPassFilter(highPass, i); // Apply low-pass filter
        float norm = normalize(lowPass, i);     // Normalize
        float rect = rectifySignal(norm);
        filterValues[i] = rect;
        Serial.print(rect); // Final processed signal
        Serial.print(",");
        Serial.print(lowPass);
        Serial.print(",");
  }
  // Serial.print(maxADCValues[0]); //this show the max MVC value
  Serial.println();

  if (deviceConnected && notificationsEnabled) {
    if(!calibStarted && !calibrated){
      calibrationStartTime = millis();
      calibStarted = true;
    }

    if(calibStarted && !calibrated){
      if(millis() - calibrationStartTime > 10000){
        calibStarted = false;
        calibrated = true;
      }
    }
    if((millis() - lastNotifyTime) > 30)
    {

      snprintf(jsonString, sizeof(jsonString), JSON_PACKET_TEMPLATE_FLOAT_V2,
          filterValues[0], filterValues[1], filterValues[2], filterValues[3],
          filterValues[4], filterValues[5], filterValues[6], filterValues[7],
          prevLowPass[0], prevLowPass[1], prevLowPass[2], prevLowPass[3],
          prevLowPass[4], prevLowPass[5], prevLowPass[6], prevLowPass[7]
          //  gyroX, gyroY, gyroZ, accX, accY, accZ);
      );

      int jsonStringLen = strlen(jsonString);
      int encodedLength = 4*(jsonStringLen/3);
      char encodedString[encodedLength + 1];
      encode_base64( (unsigned char *)jsonString, jsonStringLen,  (unsigned char *)encodedString);

      pTxCharacteristic->setValue(encodedString);
      pTxCharacteristic->notify();
      // Serial.println("Sent notification: " + String(encodedString));
      txValue++;
      lastNotifyTime = millis();
    }
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
