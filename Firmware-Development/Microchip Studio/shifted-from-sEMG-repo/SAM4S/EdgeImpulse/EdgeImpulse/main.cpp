/*
 * EdgeImpulse.cpp
 *
 * Created: 7/4/2024 3:31:07 PM
 * Author : Abeer
 */ 

#include <asf.h>
#include <string.h>

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

uint64_t g_us_count = 0;
static size_t row = 0;

void SysTick_Handler(void) {
    g_us_count++;
}

uint64_t millis(){
    return g_us_count/1000;
}

uint64_t micros(){
    return g_us_count;
}

void println(const char* str);
void println(const char* str)
{
    udi_cdc_write_buf(str,strlen(str));
    char ch = '\r';
    udi_cdc_putc(ch);
    ch = '\n';
    udi_cdc_putc(ch);
}

extern "C" ssize_t _write(int file, const void *ptr, size_t len) {
    return len;
}

extern "C" ssize_t _read(int file, void *ptr, size_t len) {
    return len;
}

// Callback function declaration
static int get_signal_data(size_t offset, size_t length, float *out_ptr);
// Raw features copied from test sample

static const float features[3][EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {
	-0.0078125,-0.015625,0.015625,0.015625,0.0078125,-0.0078125,-0.015625,-0.015625,0,0,0.0078125,-0.054688,-0.023438,0,-0.0078125,0,-0.023438,-0.03125,-0.0078125,-0.0078125,-0.023438,-0.0078125,0,0.015625,0.015625,-0.023438,-0.0078125,0.0078125,-0.03125,0.0078125,
	-0.015625,0,-0.0078125,-0.0078125,-0.0078125,0.015625,-0.023438,0,-0.023438,-0.0078125,0.0078125,-0.023438,-0.023438,0,-0.0078125,-0.023438,0.015625,-0.023438,0.0078125,-0.023438,-0.023438,0,0,0,0,-0.023438,0.015625,-0.0078125,-0.015625,-0.015625,
	-0.0078125,-0.039063,-0.015625,-0.015625,-0.023438,0,-0.0078125,0,-0.023438,-0.0078125,-0.023438,-0.015625,-0.015625,-0.015625,-0.0078125,-0.023438,-0.023438,-0.0078125,0,-0.015625,-0.015625,0,0,-0.0078125,-0.0078125,-0.0078125,-0.0078125,0.0078125,0,-0.015625
};

uint64_t previousMicros = 0;

extern "C" int main(void)
{
    /* Insert system clock initialization code here (sysclk_init()). */
    sysclk_init();
    
    /* Initialize board */
    board_init();
    
    // timer initialize 1us
    if (SysTick_Config(sysclk_get_cpu_hz() / 1000000)) {
        // Handle error
        while (1);
    }
    
    udc_start();
    
    delay_ms(5000);
    println("Performing Inference");

    // Iterate through each row in features
    for (row = 0; row < sizeof(features) / sizeof(features[0]); row++) {
        signal_t signal;            // Wrapper for raw input buffer
        ei_impulse_result_t result; // Used to store inference output
        EI_IMPULSE_ERROR res;       // Return code from inference

        // Calculate the length of the buffer
        size_t buf_len = sizeof(features[row]) / sizeof(features[row][0]);

        // Make sure that the length of the buffer matches expected input length
        if (buf_len != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            ei_printf("ERROR: The size of the input buffer is not correct.\r\n");
            ei_printf("Expected %d items, but got %d\r\n",
            EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
            (int)buf_len);
            return 1;
        }

        // Assign callback function to fill buffer used for preprocessing/inference
        signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
        signal.get_data = &get_signal_data;

        // Perform DSP pre-processing and inference
        res = run_classifier(&signal, &result, false);

        // Print return code and how long it took to perform inference
        ei_printf("run_classifier returned: %d\r\n", res);
        ei_printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n",
        result.timing.dsp,
        result.timing.classification,
        result.timing.anomaly);

        // Print the prediction results (object detection)
        #if EI_CLASSIFIER_OBJECT_DETECTION == 1
        ei_printf("Object detection bounding boxes:\r\n");
        for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
            ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
            if (bb.value == 0) {
                continue;
            }
            ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
            bb.label,
            bb.value,
            bb.x,
            bb.y,
            bb.width,
            bb.height);
        }

        // Print the prediction results (classification)
        #else
        ei_printf("Predictions:\r\n");
        for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
            ei_printf("%.5f\r\n", result.classification[i].value);
        }
        #endif

        // Print anomaly result (if it exists)
        #if EI_CLASSIFIER_HAS_ANOMALY
        ei_printf("Anomaly prediction: %.3f\r\n", result.anomaly);
        #endif

        #if EI_CLASSIFIER_HAS_VISUAL_ANOMALY
        ei_printf("Visual anomalies:\r\n");
        for (uint32_t i = 0; i < result.visual_ad_count; i++) {
            ei_impulse_result_bounding_box_t bb = result.visual_ad_grid_cells[i];
            if (bb.value == 0) {
                continue;
            }
            ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
            bb.label,
            bb.value,
            bb.x,
            bb.y,
            bb.width,
            bb.height);
        }
        ei_printf("Visual anomaly values: Mean : %.3f Max : %.3f\r\n",
        result.visual_ad_result.mean_value, result.visual_ad_result.max_value);
        #endif
    }

    /* Replace with your application code */
    
    while (1) 
    {
        println("In Loop");
        delay_ms(1000);
    }
}

// Callback: fill a section of the out_ptr buffer when requested
static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (features[row] + offset)[i];
    }

    return EIDSP_OK;
}
