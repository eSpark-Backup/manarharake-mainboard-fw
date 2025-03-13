#ifndef MODE1_UART_H
#define MODE1_UART_H

#include <Arduino.h>

// UART Configuration for Palm PCB
#define BAUD_RATE 38400
#define RX_PIN 16  // Example RX Pin
#define TX_PIN 17  // Example TX Pin

extern HardwareSerial mySerial;

void handleMode1();  // Mode 1 handler
void sendCommand(int gesture);  // Send UART command
void stopBleUart();

#endif
