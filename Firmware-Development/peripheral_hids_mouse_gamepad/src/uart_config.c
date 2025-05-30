#include "uart_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <zephyr/kernel.h>

#include <zephyr/drivers/uart.h>
#include "uart_config.h"

/* Uart node */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define MSG_SIZE 32
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);
static const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if(c == '\n') {
			//do nothing
			continue;
		}
		if ((c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

void UART_Init() {
    if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}

	printk("UART device found!");

	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}
	uart_irq_rx_enable(uart_dev);

	print_uart("Hello! I'm your echo bot.\r\n");
	print_uart("Tell me something and press enter:\r\n");
}

void UART_SendData(const uint8_t *data, size_t length) {
    if (data == NULL || length == 0) {
        return;
    }

    for (size_t i = 0; i < length; i++) {
        UART_WriteByte(data[i]);
    }
}

size_t UART_ReceiveData(uint8_t *buffer, size_t bufferLength) {
    if (buffer == NULL || bufferLength == 0) {
        return 0;
    }
    static char tx_buf[MSG_SIZE];

    size_t received = 0;

    while (k_msgq_get(&uart_msgq, buffer, K_MSEC(100)) == 0) {
        // buffer = tx_buf;
    }
    received = strlen((char*)buffer);

    return received;
}

void UART_WriteByte(uint8_t byte) {
    uart_poll_out(uart_dev, byte);
}


bool UART_DataAvailable(void) {
    int char_ready = uart_irq_rx_ready(uart_dev);
    if(char_ready > 0) {
        return true;
    }
    printk("No data available in UART buffer.\n");
    return false;
}