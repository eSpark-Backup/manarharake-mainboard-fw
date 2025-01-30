#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "manar_uart_hal.h"
#include "mode_palm.h"

static const char *TAG = "uart_events";

typedef enum {
    MODE_UNKNOWN,
    MODE_PALM,
    MODE_MOUSE,
    MODE_GAMEPAD,
    MODE_VR,
    MODE_MAX,
} ControlMode;

#define INTERRUPT_PIN GPIO_NUM_23
#define DEBOUNCE_TIME_MS 200
#define INDICATION_LED_NUM 2
#define INDICATION_BLINK_INTERVAL 200
#define MANAR_UART_BAUDRATE 38400

static volatile ControlMode current_mode = MODE_UNKNOWN;
static uint64_t last_interrupt_time = 0;
static volatile bool mode_change_indication = false;

// Function to convert ControlMode enum to string
const char* control_mode_to_string(ControlMode mode) {
    switch (mode) {
        case MODE_UNKNOWN:
            return "MODE_UNKNOWN";
        case MODE_PALM:
            return "MODE_PALM";
        case MODE_MOUSE:
            return "MODE_MOUSE";
        case MODE_GAMEPAD:
            return "MODE_GAMEPAD";
        case MODE_VR:
            return "MODE_VR";
        default:
            return "INVALID_MODE";
    }
}

void my_uart_callback(uint8_t *data, size_t len) {
    int gesture = parse_gesture(data, len);
    if (gesture == -1) {

        ESP_LOGE(TAG, "Invalid Gesture");
        return;
    }
    switch (current_mode) {
        case MODE_PALM:
            handle_palm_command(gesture);
            break;
        case MODE_MOUSE:
            // handle_mouse_command(gesture);
            ESP_LOGE(TAG, "%s NOT HANDLED", control_mode_to_string(current_mode));
            break;
        case MODE_GAMEPAD:
            // handle_gamepad_command(gesture);
            ESP_LOGE(TAG, "%s NOT HANDLED", control_mode_to_string(current_mode));
            break;
        case MODE_VR:
            // handle_vr_command(gesture);
            ESP_LOGE(TAG, "%s NOT HANDLED", control_mode_to_string(current_mode));
            break;
        default:
            ESP_LOGE(TAG, "MODE NOT HANDLED");
            break;
    }
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint64_t current_time = esp_timer_get_time() / 1000; 
    
    if (current_time - last_interrupt_time < DEBOUNCE_TIME_MS) {
        return;  
    }

    last_interrupt_time = current_time;

    current_mode++;
    mode_change_indication = true;

    if (current_mode >= MODE_MAX) {
        current_mode = MODE_PALM;  // Wrap around to MODE_PALM
    }
}

void mode_indication_led(uint8_t num_blinks, uint32_t delay_ms) {
    // Configure the LED pin as output
    gpio_set_direction(INDICATION_LED_NUM, GPIO_MODE_OUTPUT);

    for (uint8_t i = 0; i < num_blinks; i++) {
        gpio_set_level(INDICATION_LED_NUM, 1);   // Turn LED ON
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);

        gpio_set_level(INDICATION_LED_NUM, 0);   // Turn LED OFF
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    current_mode = MODE_PALM;

    manar_uart_config_t command_uart_config = {.uart = MANAR_UART_TX, .baudrate = MANAR_UART_BAUDRATE};
    manar_uart_init(command_uart_config, NULL);
    vTaskDelay(1000/portTICK_PERIOD_MS);

    command_uart_config.uart = MANAR_UART_RX;
    manar_uart_init(command_uart_config, my_uart_callback);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << INTERRUPT_PIN),  // Specify the pin for interrupt
        .mode = GPIO_MODE_INPUT,                  // Set pin as input
        .pull_up_en = GPIO_PULLUP_ENABLE,         // Enable pull-up resistor
        .intr_type = GPIO_INTR_NEGEDGE            // Trigger interrupt on rising edge
    };
    gpio_config(&io_conf);

    // Install the ISR service and add the interrupt handler
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(INTERRUPT_PIN, gpio_isr_handler, NULL);

    ESP_LOGI(TAG, "Ready, waiting for interrupt to change modes...");

    while(1)
    {
        if(mode_change_indication)
        {
            ESP_LOGW(TAG, "MODE CHANGED to %s", control_mode_to_string(current_mode));
            mode_indication_led(current_mode, INDICATION_BLINK_INTERVAL);
            mode_change_indication = false;
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}
