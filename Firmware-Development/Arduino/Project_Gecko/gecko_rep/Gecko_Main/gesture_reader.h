#ifndef GESTURE_READER_H
#define GESTURE_READER_H

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Samples per second
#define SAMPLE_RATE 500

#define INPUT_PIN1 34
#define INPUT_PIN2 35

// CH1 & CH2 status LEDs
#define CH1_STATUS_LED 13
#define CH2_STATUS_LED 14

// Envelope buffer size
// High value -> smooth but less responsive
// Low value -> not smooth but responsive
#define BUFFER_SIZE 64

struct envelope_data
{
  int env1;
  int env2;
};

extern volatile int latestGesture;
extern SemaphoreHandle_t gestureMutex;
extern TaskHandle_t gestureTaskHandle;

extern int threshold1;
extern int threshold2;
extern int getMode;
#define TH_1_ADD 10
#define TH_2_ADD 20

int getGesture();
void gestureTask(void *pvParameters);
envelope_data getEnvelope();

uint16_t read16BitValue(int address);
void write16BitValue(int address, uint16_t value);

#endif //GESTURE_READER_H