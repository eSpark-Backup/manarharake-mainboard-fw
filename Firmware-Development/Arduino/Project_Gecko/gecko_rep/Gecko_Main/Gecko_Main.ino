#include <Arduino.h>
#include "Mode4_BluetoothKeyboard.h"
#include "Mode1_UART.h"  
#include "Mode2_BluetoothMouse.h"  
#include "Mode3_BluetoothGamepad.h"  
#include "gesture_reader.h"
#include <EEPROM.h>

#define EEPROM_SIZE 32

// Button & LED Pins
#define BUTTON_PIN 33  
#define LED_RED 13    
#define LED_GREEN 14  
#define LED_BLUE 27   

// Define modes
enum Mode { MODE_1, MODE_2, MODE_3, MODE_4, MODE_5 };
Mode currentMode = MODE_1;
Mode previousMode = MODE_1;  // Track the last active mode

unsigned long lastButtonPress = 0;  
const int debounceDelay = 300;   
int eeprom_address = 0;
static int savedMode;   

// Function Prototypes
void setLEDColor(Mode mode);
void stopPreviousMode(Mode prevMode);
void handleMode3();  // Placeholder
void handleMode4();  // Placeholder

void setLEDColor(Mode mode) {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);

    switch (mode) {
        case MODE_1:
            digitalWrite(LED_RED, HIGH);  
            Serial.println("Mode 1: Palm PCB Control (Red LED)");
            break;
        case MODE_2:
            digitalWrite(LED_GREEN, HIGH);  
            Serial.println("Mode 2: Mouse Control (Green LED)");
            break;
        case MODE_3:
            digitalWrite(LED_BLUE, HIGH);  
            Serial.println("Mode 3: Gamepad (Blue LED)");
            break;
        case MODE_4:
            digitalWrite(LED_RED, HIGH);
            digitalWrite(LED_GREEN, HIGH);  
            Serial.println("Mode 4: Mobile (Yellow LED)");
            break;
        case MODE_5:
            digitalWrite(LED_RED, HIGH);
            digitalWrite(LED_GREEN, HIGH);
            digitalWrite(LED_BLUE, HIGH);
            Serial.println("Mode 5: Debug Mode (White LED)");
            break;
    }
}

void print_mode(enum Mode mode) {
    switch (mode) {
        case MODE_1: Serial.println("Mode 1, Gecko"); break;
        case MODE_2: Serial.println("Mode 2, Gecko Mouse"); break;
        case MODE_3: Serial.println("MODE 3, Gecko Gamepad"); break;
        case MODE_4: Serial.println("MODE 4, Gecko Mobile"); break;
        case MODE_5: Serial.println("MODE 5, Debug Mode"); break;
        default: Serial.println("Unknown mode\n");
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);
    EEPROM.begin(EEPROM_SIZE);
    savedMode = EEPROM.read(eeprom_address);
    threshold1 = read16BitValue(TH_1_ADD);
    threshold2 = read16BitValue(TH_2_ADD);
    print_mode(static_cast<Mode>(savedMode));
    Serial.print("Threshold1 : ");
          Serial.println(threshold1);

    Serial.print("Threshold2 : ");
          Serial.println(threshold2);
    currentMode = static_cast<Mode>(savedMode);
    getMode = savedMode;
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    pinMode(INPUT_PIN1, INPUT);
    pinMode(CH1_STATUS_LED, OUTPUT);
    
    pinMode(INPUT_PIN2, INPUT);
    pinMode(CH2_STATUS_LED, OUTPUT);


    setLEDColor(currentMode);
    Serial.println("System Initialized...");

    gestureMutex = xSemaphoreCreateMutex();

    vTaskDelay(5000);

    xTaskCreate(
        gestureTask,      // Task function
        "GestureTask",    // Task name
        4096,             // Stack size (adjust if needed)
        NULL,             // Task parameters
        6,                // Task priority
        &gestureTaskHandle // Task handle
    );
    
    // Initialize UART for Mode 1
    mySerial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
}

void loop() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - lastButtonPress > debounceDelay) {
            lastButtonPress = millis();
            

            // Switch to the next mode
            previousMode = currentMode;
            // Stop the previous mode before switching
            currentMode = static_cast<Mode>((currentMode + 1) % 5);
            EEPROM.write(eeprom_address, currentMode);
            EEPROM.commit();
            setLEDColor(currentMode);
            esp_restart();
            // stopPreviousMode(previousMode);
        }
        while (digitalRead(BUTTON_PIN) == LOW);
    }

    // Call the corresponding mode function
    switch (currentMode) {
        case MODE_1:
            handleMode1();
            break;
        case MODE_2:
            handleMode2();
            break;
        case MODE_3:
            handleMode3();
            break;
        case MODE_4:
            handleMode4();
        case MODE_5:
            break;
    }
    vTaskDelay(20);
}

// Function to stop previous mode before switching
void stopPreviousMode(Mode prevMode) {
    if (prevMode == MODE_2) {
        stopBleMouse();  // Stop BLE Mouse safely
    }
    if(prevMode == MODE_3) {
        stopBleGamepad();
    }
    if(prevMode == MODE_1) {
        stopBleUart();
    }
}
