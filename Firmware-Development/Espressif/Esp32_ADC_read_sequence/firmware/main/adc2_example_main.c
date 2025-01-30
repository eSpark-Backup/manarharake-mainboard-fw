/* ADC2 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "adc_controller.h"

static const char *TAG = "ADC_READER";

void app_main(void)
{
    esp_err_t r;

    r = initialize_adc();
    assert( r == ESP_OK );

    adc_sampling_start();

    while(1)
    {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}
