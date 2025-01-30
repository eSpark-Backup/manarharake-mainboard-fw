#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "adc_controller.h"
#include "driver/timer.h"
#include "esp_log.h"

static const char *TAG = "ADC_CONTROLLER_READER";
void adc_queue_handle_task(void *pvParameters);

typedef enum {
    OFFSET_0 = 0,    // Example offset values
    OFFSET_100 = 100,
    OFFSET_200 = 200,
    OFFSET_300 = 300,
    OFFSET_400 = 400,
    OFFSET_500 = 500,
    OFFSET_600 = 600,
    OFFSET_700 = 700
} adc_offset_t;

typedef struct adc_ctrl_t {
    union {
        adc1_channel_t adc1_channel;   // Channel for ADC1
        adc2_channel_t adc2_channel;   // Channel for ADC2
    } adc_ctl_channel_t;
    
    int raw_value;                    // Store the raw ADC reading
    adc_unit_t type;                  // Type of ADC (ADC1 or ADC2)
    adc_offset_t offset;              // Offset for ADC calibration
    esp_adc_cal_characteristics_t *adc_cal_chars; // Pointer to ADC calibration characteristics
} adc_ctrl_t;

typedef struct {
    adc_ctrl_t adc_channels[NUM_ADC_CHANNELS];  // Array to hold ADC readings
} adc_batch_t;

QueueHandle_t adc_queue;

typedef char* (*adc_formatter_t)(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length);

adc_ctrl_t adc_channels[NUM_ADC_CHANNELS] = {
    { .adc_ctl_channel_t.adc1_channel = ADC1_EXAMPLE_CHANNEL_5, .type = ADC_UNIT_1, .offset = OFFSET_0},
    { .adc_ctl_channel_t.adc1_channel = ADC1_EXAMPLE_CHANNEL_0, .type = ADC_UNIT_1, .offset = OFFSET_100},
    { .adc_ctl_channel_t.adc1_channel = ADC1_EXAMPLE_CHANNEL_1, .type = ADC_UNIT_1, .offset = OFFSET_200},
    { .adc_ctl_channel_t.adc1_channel = ADC1_EXAMPLE_CHANNEL_2, .type = ADC_UNIT_1, .offset = OFFSET_300},
    { .adc_ctl_channel_t.adc1_channel = ADC1_EXAMPLE_CHANNEL_3, .type = ADC_UNIT_1, .offset = OFFSET_400},
    { .adc_ctl_channel_t.adc1_channel = ADC1_EXAMPLE_CHANNEL_4, .type = ADC_UNIT_1, .offset = OFFSET_500},
    { .adc_ctl_channel_t.adc2_channel = ADC2_EXAMPLE_CHANNEL_0, .type = ADC_UNIT_2, .offset = OFFSET_600},
    { .adc_ctl_channel_t.adc2_channel = ADC2_EXAMPLE_CHANNEL_1, .type = ADC_UNIT_2, .offset = OFFSET_700},
};

esp_err_t __initialize_adc(adc_ctrl_t *adc_channels, size_t num_channels)
{
    gpio_num_t adc_gpio_num;
    esp_err_t r = ESP_FAIL;

    for (size_t i = 0; i < num_channels; i++)
    {
        switch (adc_channels[i].type)
        {
        case ADC_UNIT_1:
            {
                adc1_channel_t channel = adc_channels[i].adc_ctl_channel_t.adc1_channel;

                // Get GPIO for ADC1 channel
                r = adc1_pad_get_io_num(channel, &adc_gpio_num);
                assert(r == ESP_OK);

                printf("ADC1 channel %d @ GPIO %d\n", channel, adc_gpio_num);

                // Configure attenuation for ADC1
                r = adc1_config_channel_atten(channel, ADC_CON_ATTEN);
                assert(r == ESP_OK);

                // Allocate and initialize calibration characteristics for ADC1
                adc_channels[i].adc_cal_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
                if (adc_channels[i].adc_cal_chars == NULL) {
                    return ESP_ERR_NO_MEM;  // Allocation failure
                }

                // Calibrate ADC1
                r = esp_adc_cal_characterize(adc_channels[i].type, ADC_CON_ATTEN, width, DEFAULT_VREF, adc_channels[i].adc_cal_chars);
                assert(r == ESP_OK);
                break;
            }
        case ADC_UNIT_2:
            {
                adc2_channel_t channel = adc_channels[i].adc_ctl_channel_t.adc2_channel;

                // Get GPIO for ADC2 channel
                r = adc2_pad_get_io_num(channel, &adc_gpio_num);
                assert(r == ESP_OK);

                printf("ADC2 channel %d @ GPIO %d\n", channel, adc_gpio_num);

                // Configure attenuation for ADC2
                r = adc2_config_channel_atten(channel, ADC_CON_ATTEN);
                assert(r == ESP_OK);

                // Allocate and initialize calibration characteristics for ADC2
                adc_channels[i].adc_cal_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
                if (adc_channels[i].adc_cal_chars == NULL) {
                    return ESP_ERR_NO_MEM;  // Allocation failure
                }

                // Calibrate ADC2
                r = esp_adc_cal_characterize(adc_channels[i].type, ADC_CON_ATTEN, width, DEFAULT_VREF, adc_channels[i].adc_cal_chars);
                assert(r == ESP_OK);
                break;
            }
        
        default:
            break;
        }

        vTaskDelay(2 * portTICK_PERIOD_MS);
    }

    return r;
}

esp_err_t initialize_adc(void)
{
    esp_err_t r = ESP_FAIL;

    r = __initialize_adc(adc_channels, NUM_ADC_CHANNELS);
    assert( r == ESP_OK );

        adc_queue = xQueueCreate(10, sizeof(adc_batch_t));  // 10 is the max number of batches in the queue
    if (adc_queue == NULL) {
        // Handle queue creation failure
        printf("Failed to create ADC queue\n");
        return;
    }

    // Start the ADC processing task
    xTaskCreate(adc_queue_handle_task, "adc_queue_handle_task", 2048, NULL, 10, NULL);

    return r;
}

void __read_adc(adc_ctrl_t *adc_channels, size_t num_channels, adc_bits_width_t width)
{
    for (size_t i = 0; i < num_channels; i++)
    {
        int read_raw = 0;
        esp_err_t r;

        switch (adc_channels[i].type)
        {
            case ADC_UNIT_1:
            {
                adc1_channel_t channel = adc_channels[i].adc_ctl_channel_t.adc1_channel;
                read_raw = adc1_get_raw(channel);
                adc_channels[i].raw_value = read_raw; // Save the raw value
                break;
            }

            case ADC_UNIT_2:
            {
                adc2_channel_t channel = adc_channels[i].adc_ctl_channel_t.adc2_channel;
                r = adc2_get_raw(channel, width, &read_raw);
                if (r == ESP_OK)
                {
                    adc_channels[i].raw_value = read_raw; // Save the raw value
                }
                else
                {
                    adc_channels[i].raw_value = -1; // Indicate error with -1
                }
                break;
            }

            default:
                adc_channels[i].raw_value = -1; // Invalid or uninitialized type
                break;
        }

    }
}


char* __get_adc_values_string(const adc_ctrl_t *adc_channels, size_t num_channels, adc_formatter_t formatter)
{
    static char output[MAX_ADC_OUTPUT_LENGTH];  // Static buffer to hold the output string

    // Call the provided formatter to format the ADC values into the output buffer
    return formatter(adc_channels, num_channels, output, sizeof(output));
}

char* serial_plotter_formatter_voltage(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length)
{
    size_t offset = 0;

    // Loop through the ADC channels and format each channel's data for the Serial Plotter
    for (size_t i = 0; i < num_channels; i++)
    {
        int written = 0;  // Number of characters written in this iteration
        
        // Start with an empty line, formatting each ADC reading with its corresponding label
        switch (adc_channels[i].type)
        {
            case ADC_UNIT_1:
                written = snprintf(output + offset, max_output_length - offset,
                                   "%d", esp_adc_cal_raw_to_voltage(adc_channels[i].raw_value, adc_channels[i].adc_cal_chars) + adc_channels[i].offset);
                break;

            case ADC_UNIT_2:
                written = snprintf(output + offset, max_output_length - offset,
                                   "%d", esp_adc_cal_raw_to_voltage(adc_channels[i].raw_value, adc_channels[i].adc_cal_chars) + adc_channels[i].offset);                
                break;

            default:
                written = snprintf(output + offset, max_output_length - offset,
                                   "No ADC data available");
                break;
        }

        // If written was negative or we are about to overflow, stop writing more data
        if (written < 0 || written >= max_output_length - offset)
        {
            break;
        }

        // Move the offset ahead by the number of characters written
        offset += written;

        // If this is not the last channel, add a comma to separate the values for the plotter
        if (i < num_channels - 1)
        {
            written = snprintf(output + offset, max_output_length - offset, ",");
            offset += written;
        }
    }

    // After all values are added for this sample, add a newline to separate the sample from the next one
    snprintf(output + offset, max_output_length - offset, "\n");

    return output;
}

char* serial_plotter_formatter(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length)
{
    size_t offset = 0;

    // Loop through the ADC channels and format each channel's data for the Serial Plotter
    for (size_t i = 0; i < num_channels; i++)
    {
        int written = 0;  // Number of characters written in this iteration
        
        // Start with an empty line, formatting each ADC reading with its corresponding label
        switch (adc_channels[i].type)
        {
            case ADC_UNIT_1:
                written = snprintf(output + offset, max_output_length - offset,
                                   "%d", adc_channels[i].raw_value + adc_channels[i].offset);
                break;

            case ADC_UNIT_2:
                written = snprintf(output + offset, max_output_length - offset,
                                   "%d", adc_channels[i].raw_value + adc_channels[i].offset);           
                break;

            default:
                written = snprintf(output + offset, max_output_length - offset,
                                   "No ADC data available");
                break;
        }

        // If written was negative or we are about to overflow, stop writing more data
        if (written < 0 || written >= max_output_length - offset)
        {
            break;
        }

        // Move the offset ahead by the number of characters written
        offset += written;

        // If this is not the last channel, add a comma to separate the values for the plotter
        if (i < num_channels - 1)
        {
            written = snprintf(output + offset, max_output_length - offset, ",");
            offset += written;
        }
    }

    // After all values are added for this sample, add a newline to separate the sample from the next one
    snprintf(output + offset, max_output_length - offset, "\n");

    return output;
}

char* json_formatter(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length)
{
    size_t offset = 0;
    offset += snprintf(output + offset, max_output_length - offset, "{\n");

    for (size_t i = 0; i < num_channels; i++)
    {
        switch (adc_channels[i].type)
        {
            case ADC_UNIT_1:
                offset += snprintf(output + offset, max_output_length - offset,
                                   "  \"ADC1_Channel_%d\": %d,\n",
                                   adc_channels[i].adc_ctl_channel_t.adc1_channel,
                                   adc_channels[i].raw_value);
                break;

            case ADC_UNIT_2:
                offset += snprintf(output + offset, max_output_length - offset,
                                   "  \"ADC2_Channel_%d\": %d,\n",
                                   adc_channels[i].adc_ctl_channel_t.adc2_channel,
                                   adc_channels[i].raw_value);
                break;

            default:
                offset += snprintf(output + offset, max_output_length - offset,
                                   "  \"Channel_%zu\": \"No ADC data available\",\n", i);
                break;
        }
    }

    // Remove the last comma and add closing bracket
    if (offset > 2)
        output[offset - 2] = '\0';  // Remove last comma

    offset += snprintf(output + offset, max_output_length - offset, "\n}\n");

    return output;
}

char* csv_formatter(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length)
{
    size_t offset = 0;
    offset += snprintf(output + offset, max_output_length - offset, "Channel,Type,Raw Value\n");

    for (size_t i = 0; i < num_channels; i++)
    {
        switch (adc_channels[i].type)
        {
            case ADC_UNIT_1:
                offset += snprintf(output + offset, max_output_length - offset,
                                   "ADC1_Channel_%d,ADC1,%d\n",
                                   adc_channels[i].adc_ctl_channel_t.adc1_channel,
                                   adc_channels[i].raw_value);
                break;

            case ADC_UNIT_2:
                offset += snprintf(output + offset, max_output_length - offset,
                                   "ADC2_Channel_%d,ADC2,%d\n",
                                   adc_channels[i].adc_ctl_channel_t.adc2_channel,
                                   adc_channels[i].raw_value);
                break;

            default:
                offset += snprintf(output + offset, max_output_length - offset,
                                   "Channel_%zu,Unknown,No ADC data available\n", i);
                break;
        }
    }

    return output;
}


void print_adc_values()
{
    char *adc_values_string = __get_adc_values_string(adc_channels, NUM_ADC_CHANNELS, serial_plotter_formatter);
    // char *adc_values_string = __get_adc_values_string(adc_channels, NUM_ADC_CHANNELS, json_formatter);
    // char *adc_values_string = __get_adc_values_string(adc_channels, NUM_ADC_CHANNELS, csv_formatter);
    printf("%s", adc_values_string);
}


void read_adc(void)
{
    __read_adc(adc_channels, NUM_ADC_CHANNELS, width);
    __send_adc_batch_to_queue(adc_channels);
}

// Interrupt handler for the timer
void IRAM_ATTR timer_isr_handler(void *param) {
    // Clear the interrupt flag
    timer_group_clr_intr_status_in_isr(TIMER_GROUP, TIMER_INDEX);

    // Re-enable the alarm
    timer_group_enable_alarm_in_isr(TIMER_GROUP, TIMER_INDEX);

    read_adc();
}

void adc_timer_init() {
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true
    };

    timer_init(TIMER_GROUP, TIMER_INDEX, &config);
    timer_set_counter_value(TIMER_GROUP, TIMER_INDEX, 0x00000000);
    timer_set_alarm_value(TIMER_GROUP, TIMER_INDEX, TIMER_INTERVAL_SEC * (TIMER_BASE_CLK / TIMER_DIVIDER));
    timer_enable_intr(TIMER_GROUP, TIMER_INDEX);

    timer_isr_register(TIMER_GROUP, TIMER_INDEX, timer_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP, TIMER_INDEX);
}

void adc_sampling_start()
{
    adc_timer_init();
}


void __send_adc_batch_to_queue(adc_ctrl_t *adc_channels)
{
    adc_batch_t adc_batch;  // Structure to hold all ADC readings

    // Copy the adc_channels array to adc_batch
    memcpy(adc_batch.adc_channels, adc_channels, sizeof(adc_batch.adc_channels));

    // Send the entire batch of ADC readings to the queue
    if (xQueueSend(adc_queue, &adc_batch, portMAX_DELAY) != pdPASS)
    {
        // Handle queue send error (e.g., queue full)
        printf("Failed to send ADC batch to the queue\n");
    }
}

void adc_queue_handle_task(void *pvParameters)
{
    adc_batch_t received_batch;  // Structure to hold the received batch of ADC data

    while (1)
    {
        // Wait for the batch of ADC data to arrive in the queue
        if (xQueueReceive(adc_queue, &received_batch, portMAX_DELAY) == pdPASS)
        {
            // Process the received ADC batch
            // char *adc_values_string = __get_adc_values_string(adc_channels, NUM_ADC_CHANNELS, serial_plotter_formatter_voltage);
            char *adc_values_string = __get_adc_values_string(adc_channels, NUM_ADC_CHANNELS, serial_plotter_formatter);
            // char *adc_values_string = __get_adc_values_string(adc_channels, NUM_ADC_CHANNELS, json_formatter);
            // char *adc_values_string = __get_adc_values_string(adc_channels, NUM_ADC_CHANNELS, csv_formatter);
            printf("%s", adc_values_string);
        }
        else
        {
            // Handle the case where receiving from the queue failed
            printf("Failed to receive ADC batch from the queue\n");
        }
    }
}

