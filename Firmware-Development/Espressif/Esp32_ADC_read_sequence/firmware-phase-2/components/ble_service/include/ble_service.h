#include "esp_err.h"

esp_err_t ble_service_send_data(uint8_t *data_ptr, uint16_t data_len);
esp_err_t ble_service_init(void);
bool ble_service_is_connected(void);
bool ble_service_is_paired(void);