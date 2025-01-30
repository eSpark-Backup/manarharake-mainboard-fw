#include "gestures.h"
#include "esp_log.h"
#include <string.h>

int parse_gesture(uint8_t *data, size_t len) {
    // Assuming data is a string representing a number from 1 to 6
    if (len == 0) {
        return -1; // Invalid data length
    }

    // Convert the data to an integer
    int gesture = atoi((char *)data);

    // Check if the gesture is within the valid range (1-6)
    if (gesture >= 1 && gesture <= NUMBER_OF_GESTURES) {
        return gesture;  // Return 0-based index for the gesture
    } else {
        return -1;  // Invalid gesture
    }
}