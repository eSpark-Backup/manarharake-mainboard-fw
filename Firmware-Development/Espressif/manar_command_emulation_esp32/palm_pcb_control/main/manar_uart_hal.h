#include <stddef.h>
#include <stdint.h>
#include "driver/uart.h"

typedef void (*manar_uart_data_callback_t)(uint8_t *data, size_t len);

typedef enum {
    MANAR_UART_RX = UART_NUM_0,
    MANAR_UART_TX = UART_NUM_2
}manar_uart_t;

typedef struct {
    manar_uart_t uart;
    int baudrate;
}manar_uart_config_t;

void manar_uart_init(manar_uart_config_t uart_config, manar_uart_data_callback_t callback);

int manar_uart_read(manar_uart_t uart, char *buffer, size_t length);

void manar_uart_write(manar_uart_t uart, const char *data, size_t length);