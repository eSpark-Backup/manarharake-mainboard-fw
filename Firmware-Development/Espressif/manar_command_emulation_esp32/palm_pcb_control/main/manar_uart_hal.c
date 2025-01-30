#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include "manar_uart_hal.h"

static const char *TAG = "uart_hal";

#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart0_queue;
static QueueHandle_t uart2_queue;

typedef struct {
    QueueHandle_t uart_queue;
    int uart_num;
    manar_uart_data_callback_t data_callback;
} uart_task_params_t;

static void uart_event_task(void *pvParameters)
{
    uart_task_params_t *params = (uart_task_params_t *)pvParameters;
    QueueHandle_t uart_queue = params->uart_queue;
    int uart_num = params->uart_num;
    manar_uart_data_callback_t data_callback = params->data_callback;
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            // ESP_LOGI(TAG, "uart[%d] event:", uart_num);
            switch (event.type) {
            //Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                // ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(uart_num, dtmp, event.size, portMAX_DELAY);
                // ESP_LOGI(TAG, "[DATA EVT]:");
                // uart_write_bytes(uart_num, (const char*) dtmp, event.size);

                if (data_callback) {
                    data_callback(dtmp, event.size);
                }
                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(uart_num);
                xQueueReset(uart_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(uart_num);
                xQueueReset(uart_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(uart_num, &buffered_size);
                int pos = uart_pattern_pop_pos(uart_num);
                ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                    // record the position. We should set a larger queue size.
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(uart_num);
                } else {
                    uart_read_bytes(uart_num, dtmp, pos, 100 / portTICK_PERIOD_MS);
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(uart_num, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "read data: %s", dtmp);
                    ESP_LOGI(TAG, "read pat : %s", pat);
                }
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void manar_uart_init(manar_uart_config_t manar_uart_config, manar_uart_data_callback_t callback) {

    QueueHandle_t uart_queue;
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = manar_uart_config.baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    //Install UART driver, and get the queue.
    if(manar_uart_config.uart == MANAR_UART_RX)
    {
        uart_queue = uart0_queue;
    }
    if(manar_uart_config.uart == MANAR_UART_TX)
    {
        uart_queue = uart2_queue;
    }
    uart_driver_install(manar_uart_config.uart, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0);
    uart_param_config(manar_uart_config.uart, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);

    //Set UART pins (using UART0 default pins ie no changes.)
    // uart_set_pin(manar_uart_config.uart, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if(manar_uart_config.uart == MANAR_UART_RX)
    {
    uart_set_pin(manar_uart_config.uart, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if(manar_uart_config.uart == MANAR_UART_TX)
    {
    uart_set_pin(manar_uart_config.uart, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }

    //Set uart pattern detect function.
    // uart_enable_pattern_det_baud_intr(manar_uart_config.uart, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    // uart_pattern_queue_reset(manar_uart_config.uart, 20);

    //Create a task to handler UART event from ISR
    uart_task_params_t *task_params = malloc(sizeof(uart_task_params_t));
    task_params->uart_queue = uart_queue;
    task_params->uart_num = manar_uart_config.uart;
    task_params->data_callback = callback;
    ESP_LOGI(TAG, "Initializing UART %d with baudrate %d", manar_uart_config.uart, manar_uart_config.baudrate);

    xTaskCreate(uart_event_task, "uart_event_task", 2048, task_params, 12, NULL);
}

int manar_uart_read(manar_uart_t uart, char *buffer, size_t length) {
    // return bytes_read;
    return 0;
}

void manar_uart_write(manar_uart_t uart, const char *data, size_t length) {
    uart_write_bytes(uart, (const char*) data, length);
}
