#ifndef ENCODER_MANAGER_H
#define ENCODER_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// Encoder function type
typedef esp_err_t (*encoder_func_t)(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len);

// Set a custom encoder function
void set_custom_encoder(encoder_func_t encoder);

// Encode and send data via BLE
esp_err_t ble_service_send_data_encoded(uint8_t* data, uint16_t len);

// Default encoder function using mbedTLS
esp_err_t mbedtls_base64_encode_wrapper(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len);
esp_err_t pass_through_encoder(const uint8_t* input, size_t input_len, uint8_t* output, size_t* output_len);
#endif // ENCODER_MANAGER_H
