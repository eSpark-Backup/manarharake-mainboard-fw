#ifndef __ADC_CONTROLLER_H__
#define __ADC_CONTROLLER_H__

#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "fir_filters.h"

#define ADC2_EXAMPLE_CHANNEL_0          ADC2_CHANNEL_9
#define ADC2_EXAMPLE_CHANNEL_1          ADC2_CHANNEL_8

#define ADC1_EXAMPLE_CHANNEL_0          ADC1_CHANNEL_5 
#define ADC1_EXAMPLE_CHANNEL_1          ADC1_CHANNEL_4
#define ADC1_EXAMPLE_CHANNEL_2          ADC1_CHANNEL_7
#define ADC1_EXAMPLE_CHANNEL_3          ADC1_CHANNEL_6
#define ADC1_EXAMPLE_CHANNEL_4          ADC1_CHANNEL_3
#define ADC1_EXAMPLE_CHANNEL_5          ADC1_CHANNEL_0

#define TIMER_GROUP    TIMER_GROUP_0
#define TIMER_INDEX    TIMER_0
#define TIMER_DIVIDER  16 // Timer clock divider (80 MHz / 16 = 5 MHz)
#define TIMER_INTERVAL_SEC  0.001  // Sampling interval in seconds

#define MAX_ADC_OUTPUT_LENGTH 512*2  // Adjust size as needed

#define ADC_CON_ATTEN ADC_ATTEN_11db
#define NUM_ADC_CHANNELS 8
#define DEFAULT_VREF 1093//mv 

#define USE_BANDPASS_FILTER 1
#define CALIBRATION_DURATION_MS 10000

#if CONFIG_IDF_TARGET_ESP32
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
#elif CONFIG_IDF_TARGET_ESP32S2
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
#endif

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
        adc1_channel_t adc1_channel;    // Channel for ADC1
        adc2_channel_t adc2_channel;   // Channel for ADC2
    } adc_ctl_channel_t;

    float mvc_value;                    //Maximum Voluntary Contraction (MVC)

    int raw_value;                      // Store the raw ADC reading
    float normalized_value;             // Normalized value (between 0 and 1, or based on max ADC range)
    float mean_value;                   // Mean value (used for centering)
    float centered_value;               // Centered value
    adc_unit_t type;                    // Type of ADC (ADC1 or ADC2)
    adc_offset_t offset;                // Offset for ADC calibration
    esp_adc_cal_characteristics_t *adc_cal_chars; // Pointer to ADC calibration characteristics

    HighPassFilter hpf;                 // High-pass filter instance
    IIRLowPassFilter lpf;               // Low-pass filter instance

    FIRFilter bpf;

    float filtered_value;               // Final filtered value
} adc_ctrl_t;

typedef struct {
    adc_ctrl_t adc_channels[NUM_ADC_CHANNELS];  // Array to hold ADC readings
} adc_batch_t;

typedef esp_err_t (*adc_log_cb_t)(uint8_t *, uint16_t);

esp_err_t initialize_adc(void);
void adc_sampling_start();
void read_adc(void);
void print_adc_values();
void adc_set_logging_callback(adc_log_cb_t log_cb);
void adc_set_plotter_callback(adc_log_cb_t log_cb);

#endif //__ADC_CONTROLLER_H__