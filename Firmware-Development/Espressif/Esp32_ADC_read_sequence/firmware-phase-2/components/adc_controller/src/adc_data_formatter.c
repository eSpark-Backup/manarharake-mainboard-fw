#include "adc_data_formatter.h"

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
    offset += snprintf(output + offset, max_output_length - offset, "{\"ADC\":{");

    for (size_t i = 0; i < num_channels; i++)
    {
        switch (adc_channels[i].type)
        {
            case ADC_UNIT_1:
                offset += snprintf(output + offset, max_output_length - offset,
                                   "\"Value_%d\":%.2f,",
                                //    adc_channels[i].adc_ctl_channel_t.adc1_channel,
                                    i,
                                   (float)adc_channels[i].filtered_value);
                break;

            case ADC_UNIT_2:
                offset += snprintf(output + offset, max_output_length - offset,
                                   "\"Value_%d\":%.2f,",
                                //    adc_channels[i].adc_ctl_channel_t.adc2_channel,
                                    i,
                                   (float)adc_channels[i].filtered_value);
                break;

            default:
                // offset += snprintf(output + offset, max_output_length - offset,
                //                    "  \"Channel_%zu\": \"No ADC data available\",\n", i);
                break;
        }
    }

    // Remove the last comma and add closing bracket
    if (offset > 2)
        output[offset - 1] = '}';  // Remove last comma

    offset += snprintf(output + offset, max_output_length - offset, "}\n");
    return output;
}

char *serial_plotter_mean_formatter(const adc_ctrl_t *adc_channels, size_t num_channels, char *output, size_t max_output_length)
{
    size_t offset = 0;

    // Loop through the ADC channels and format each channel's mean value
    for (size_t i = 0; i < num_channels; i++)
    {
        int written = 0; // Number of characters written in this iteration

        // Check if enough space is left in the buffer
        if (offset >= max_output_length - 1)
        {
            break;
        }

        // Format the mean value (multiplied by 1000 for better resolution)
        written = snprintf(output + offset, max_output_length - offset, "%.2f", (float)adc_channels[i].filtered_value);
        // written = snprintf(output + offset, max_output_length - offset, "%.4f", (float)adc_channels[i].centered_value);
        // written = snprintf(output + offset, max_output_length - offset, "%d", (int)adc_channels[i].filtered_value);

        // If snprintf failed or would overflow, stop formatting
        if (written < 0 || written >= (int)(max_output_length - offset))
        {
            break;
        }

        // Move the offset ahead by the number of characters written
        offset += written;

        // Add a comma separator if this is not the last channel
        if (i < num_channels - 1)
        {
            if (offset < max_output_length - 1)
            {
                output[offset++] = ','; // Add a comma
            }
            else
            {
                break; // Stop if there's no room for the comma
            }
        }
    }

    // Add a newline at the end if there's space
    if (offset < max_output_length - 1)
    {
        output[offset++] = '\n';
    }

    // Null-terminate the string
    output[offset] = '\0';

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