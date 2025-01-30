#include "encoder_manager.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include <stdlib.h>

static const char *TAG = "ENCODER_MANAGER";

// Global pointer to the user-provided encoder function
static encoder_func_t custom_encoder = NULL;

// Set a custom encoder function
void set_custom_encoder(encoder_func_t encoder)
{
    custom_encoder = encoder;
    ESP_LOGI(TAG, "Custom encoder set successfully.");
}

// Encode and send data via BLE
esp_err_t ble_service_send_data_encoded(uint8_t* data, uint16_t len)
{
    if (custom_encoder == NULL) {
        ESP_LOGE(TAG, "Custom encoder not set.");
        return ESP_ERR_INVALID_STATE;
    }

    // Calculate output buffer size (Base64 requires ~4/3 of input size plus padding)
    size_t output_len = (len * 4 / 3) + 4; // Adjust based on encoder requirements
    uint8_t* encoded_data = malloc(output_len);
    if (encoded_data == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed.");
        return ESP_ERR_NO_MEM;
    }

    // Encode the data using the custom encoder
    esp_err_t ret = custom_encoder(data, len, encoded_data, &output_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Encoding failed with error code: %d", ret);
        free(encoded_data);
        return ret;
    }

    // Send the encoded data over BLE (replace this with your BLE send function)
    ble_service_send_data(encoded_data, output_len);

    // Free the allocated buffer
    free(encoded_data);

    return ESP_OK;
}

// Default encoder function using mbedTLS
esp_err_t mbedtls_base64_encode_wrapper(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len)
{
    size_t olen = 0;
    int ret = mbedtls_base64_encode(output, *output_len, &olen, input, input_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedTLS Base64 encoding failed.");
        return ESP_FAIL;
    }
    *output_len = olen;
    return ESP_OK;
}

esp_err_t pass_through_encoder(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len)
{
    if (*output_len < input_len) {
        ESP_LOGE("PASS_THROUGH_ENCODER", "Output buffer too small for input data.");
        return ESP_ERR_NO_MEM; // Output buffer is too small
    }

    // Copy input to output
    memcpy(output, input, input_len);
    *output_len = input_len; // Set the output length equal to the input length

    return ESP_OK;
}