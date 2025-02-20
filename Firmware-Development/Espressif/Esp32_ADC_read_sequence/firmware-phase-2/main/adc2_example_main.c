#include <stdio.h>
#include <stdlib.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "adc_controller.h"
#include "ble_service.h"
#include "encoder_manager.h"

static const char *TAG = "ADC_READER_PHASE_2";

adc_log_cb_t printf_logging(uint8_t* data, uint16_t len)
{
    printf("%s", data);
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t r;

    ble_service_init();

    // Wait for BLE connection and pairing
    while (!ble_service_is_connected() || !ble_service_is_paired()) {
        printf("Waiting for BLE connection and pairing...\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    r = initialize_adc();
    assert(r == ESP_OK);

    set_custom_encoder(mbedtls_base64_encode_wrapper);
    // set_custom_encoder(pass_through_encoder);

    // adc_set_logging_callback((adc_log_cb_t)printf_logging);
    adc_set_plotter_callback((adc_log_cb_t)printf_logging);
    // adc_set_logging_callback((adc_log_cb_t)ble_service_send_data);
    adc_set_logging_callback((adc_log_cb_t)ble_service_send_data_encoded);
    adc_sampling_start();

}
