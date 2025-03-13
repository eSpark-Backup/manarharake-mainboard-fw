#include "gesture_reader.h"
#include <EEPROM.h>

// Define different constant for different envelope
// Channel 1 variables
int circular_buffer1[BUFFER_SIZE];
int data_index1, sum1;
// Channel 2 varibles
int circular_buffer2[BUFFER_SIZE];
int data_index2, sum2;

// Button to start the serial transaction
const int buttonPin = 4;

// LED to show serial transaction status
const int ledPin = 2;

// Device working status varaiables
int ledState = 0;
int buttonState;
int lastButtonState = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Calibrate threshold values
// Uncomment the below line to view EMG envelope on serial plotter
#define Calibrate

// Uncomment the line below to switch to advanced version of the code which provides better performance but may vary for different users
// #define Advanced

int threshold1 = 40;
int threshold2 = 60;
int getMode;

volatile int latestGesture = 0;
SemaphoreHandle_t xMutex;
int g_envelope1 = 0, g_envelope2 = 0;
SemaphoreHandle_t gestureMutex;
TaskHandle_t gestureTaskHandle = NULL;

// Envelope detection algorithm
// Get CH1 envelope
int getEnvelope1(int abs_emg){
    sum1 -= circular_buffer1[data_index1];
    sum1 += abs_emg;
    circular_buffer1[data_index1] = abs_emg;
    data_index1 = (data_index1 + 1) % BUFFER_SIZE;
    return (sum1/BUFFER_SIZE) * 2;
  }
  // Get CH2 envelope
  int getEnvelope2(int abs_emg){
    sum2 -= circular_buffer2[data_index2];
    sum2 += abs_emg;
    circular_buffer2[data_index2] = abs_emg;
    data_index2 = (data_index2 + 1) % BUFFER_SIZE;
    return (sum2/BUFFER_SIZE) * 2;
  }
  
  // Band-Pass Butterworth IIR digital filter, generated using filter_gen.py.
  // Sampling rate: 500.0 Hz, frequency: [74.5, 149.5] Hz.
  // Filter is order 4, implemented as second-order sections (biquads).
  // Reference: 
  // https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.butter.html
  // https://courses.ideate.cmu.edu/16-223/f2020/Arduino/FilterDemos/filter_gen.py
  
  // CH1 EMG filter
  float EMGFilter1(float input)
  {
    float output = input;
    {
      static float z1, z2; // filter section state
      float x = output - 0.05159732*z1 - 0.36347401*z2;
      output = 0.01856301*x + 0.03712602*z1 + 0.01856301*z2;
      z2 = z1;
      z1 = x;
    }
    {
      static float z1, z2; // filter section state
      float x = output - -0.53945795*z1 - 0.39764934*z2;
      output = 1.00000000*x + -2.00000000*z1 + 1.00000000*z2;
      z2 = z1;
      z1 = x;
    }
    {
      static float z1, z2; // filter section state
      float x = output - 0.47319594*z1 - 0.70744137*z2;
      output = 1.00000000*x + 2.00000000*z1 + 1.00000000*z2;
      z2 = z1;
      z1 = x;
    }
    {
      static float z1, z2; // filter section state
      float x = output - -1.00211112*z1 - 0.74520226*z2;
      output = 1.00000000*x + -2.00000000*z1 + 1.00000000*z2;
      z2 = z1;
      z1 = x;
    }
    return output;
  }
  
  // CH2 EMG filter
  float EMGFilter2(float input2)
  {
    float output = input2;
    {
      static float z1, z2; // filter section state
      float x = output - 0.05159732*z1 - 0.36347401*z2;
      output = 0.01856301*x + 0.03712602*z1 + 0.01856301*z2;
      z2 = z1;
      z1 = x;
    }
    {
      static float z1, z2; // filter section state
      float x = output - -0.53945795*z1 - 0.39764934*z2;
      output = 1.00000000*x + -2.00000000*z1 + 1.00000000*z2;
      z2 = z1;
      z1 = x;
    }
    {
      static float z1, z2; // filter section state
      float x = output - 0.47319594*z1 - 0.70744137*z2;
      output = 1.00000000*x + 2.00000000*z1 + 1.00000000*z2;
      z2 = z1;
      z1 = x;
    }
    {
      static float z1, z2; // filter section state
      float x = output - -1.00211112*z1 - 0.74520226*z2;
      output = 1.00000000*x + -2.00000000*z1 + 1.00000000*z2;
      z2 = z1;
      z1 = x;
    }
    return output;
  }


void write16BitValue(int address, uint16_t value) {
    EEPROM.write(address, value >> 8);       // Store high byte
    EEPROM.write(address + 1, value & 0xFF); // Store low byte
    EEPROM.commit();  // Save changes to flash
}

uint16_t read16BitValue(int address) {
    return (EEPROM.read(address) << 8) | EEPROM.read(address + 1);
}
void gestureTask(void *pvParameters) {
    // int reading = digitalRead(buttonPin);
    // if (reading != lastButtonState) {
    //   // reset the debouncing timer
    //   lastDebounceTime = millis();
    // }
  
    // if ((millis() - lastDebounceTime) > debounceDelay) {
    //   // whatever the reading is at, it's been there for longer than the debounce
    //   // delay, so take it as the actual current state:
  
    //   // if the button state has changed:
    //   if (reading != buttonState) {
    //     buttonState = reading;
  
    //     // only toggle the LED if the new button state is HIGH
    //     if (buttonState == HIGH) {
    //       ledState = !ledState;
    //     }
    //   }
    // }
   
    // Calculate elapsed time
    while(1)
    {

      static unsigned long past = 0;
      unsigned long present = micros();
      unsigned long interval = present - past;
      past = present;
      
      // Run timer
      static long timer = 0;
      timer -= interval;
      
      // Sample and get envelope
      if(timer < 0) {
        timer += 1000000 / SAMPLE_RATE;
        // RAW EMG
        int sensor_value1 = analogRead(INPUT_PIN1);
        
        // Filtered EMG
        int signal1 = EMGFilter1(sensor_value1);
        
        // EMG envelope
        int envelope1 = getEnvelope1(abs(signal1));
        int sensor_value2 = analogRead(INPUT_PIN2);
        
        // Filtered EMG
        int signal2 = EMGFilter2(sensor_value2);
        
        // EMG envelope
        int envelope2 = getEnvelope2(abs(signal2));
        int gesture = 0;
        
        // If set to calibrate show envelope data on serial monitor/plotter
        #ifdef Calibrate
        if(getMode == 4)
        {
          Serial.print(envelope1);
          Serial.print('\t');
          Serial.println(envelope2);
        }
        
        #endif
        
        #ifdef Advanced
        
        // LED ON when one hand move otherwise off, same for other hand
        // envelope should be higher than the threshold and also the envelope of other hand
        // output as "1" for CH1 and "2" for CH2
        if(envelope1 > envelope2 && envelope1 > threshold1 && envelope2 < 20)
        { 
          if(envelope2>10)
          {
            //Serial.println("4");
            gesture = 4;
          }
          else
          {
            //Serial.println("1");
            gesture = 1;
          }
        }
        else if(envelope2 > envelope1 && envelope2 > threshold2)
        { 
          if(envelope1>20)
          {
            //Serial.println("5");
            gesture = 5;
          }
          else
          {
            //Serial.println("2");
            gesture = 2;
          }
          }
          // else if(envelope1 > 20 && envelope1 < threshold1 && envelope2 < threshold2 && envelope2 > 10)
          // { 
          //   //Serial.println("3");
          //   gesture = 3;
          // }
          else
          { 
            //Serial.println("0");
            gesture = 0;
          }
          
          #else
          
          if(envelope1 > envelope2 && envelope1 > threshold1)
          { 
            //Serial.println("1");
            gesture = 1;
          }
          else if(envelope2 > envelope1 && envelope2 > threshold2)
          { 
            //Serial.println("2");
            gesture = 2;
          }
          // else if(envelope1 > 20 && envelope1 < threshold1 && envelope2 < threshold2 && envelope2 > 10)
          // { 
          //   //Serial.println("3");
          //   gesture = 3;
          // }
          else
          { 
            //Serial.println("0");
            gesture = 0;
          }
          
          if (xSemaphoreTake(gestureMutex, portMAX_DELAY)) {
            latestGesture = gesture;
            g_envelope1 = envelope1;
            g_envelope2 = envelope2;
            xSemaphoreGive(gestureMutex);
          }
          
          #endif
        }
        //     digitalWrite(ledPin, ledState);
        //    lastButtonState = reading;
        vTaskDelay(2);
      }
}


int getGesture() {
  int gesture = 0;
  if (xSemaphoreTake(gestureMutex, portMAX_DELAY)) {
      gesture = latestGesture;
      latestGesture = 0;  // Reset after reading
      xSemaphoreGive(gestureMutex);
  }
  return gesture;
}

envelope_data getEnvelope() {
  envelope_data data;
  if (xSemaphoreTake(gestureMutex, portMAX_DELAY)) {
      data.env1 = g_envelope1;
      data.env2 = g_envelope2;
      xSemaphoreGive(gestureMutex);
  }
  return data;
}
