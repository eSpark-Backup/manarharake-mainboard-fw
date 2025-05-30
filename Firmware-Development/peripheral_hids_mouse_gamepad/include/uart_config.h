#ifndef UART_CONFIG_H
#define UART_CONFIG_H

#include <stdint.h>
#include <stddef.h>

void UART_Init(void);
void UART_SendData(const uint8_t *data, size_t length);
size_t UART_ReceiveData(uint8_t *buffer, size_t bufferLength);;
// void UART_SendByte(uint8_t byte);
// uint8_t UART_ReceiveByte(void);

#endif // UART_CONFIG_H