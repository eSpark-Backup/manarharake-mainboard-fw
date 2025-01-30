#ifndef __ADC_DATA_FORMATTER_H__
#define __ADC_DATA_FORMATTER_H__

#include "adc_controller.h"

char* serial_plotter_formatter_voltage(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length);
char* serial_plotter_formatter(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length);
char* json_formatter(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length);
char *serial_plotter_mean_formatter(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length);
char* csv_formatter(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length);

#endif //__ADC_DATA_FORMATTER_H__