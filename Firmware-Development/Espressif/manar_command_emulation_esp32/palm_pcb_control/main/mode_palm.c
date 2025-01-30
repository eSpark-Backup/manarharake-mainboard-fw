#include "mode_palm.h"
#include "manar_uart_hal.h"
#include "esp_log.h"
#include <string.h>

void handle_palm_command(GestureCommand gesture) {
    const char *cmd = NULL;

    // Select the appropriate command based on the gesture
    switch (gesture) {
        case GESTURE_1:
            cmd = "G1";
            break;
        case GESTURE_2:
            cmd = "G2";
            break;
        case GESTURE_3:
            cmd = "G3";
            break;
        case GESTURE_4:
            cmd = "G4";
            break;
        case GESTURE_5:
            cmd = "G5";
            break;
        case GESTURE_6:
            cmd = "G6";
            break;
        default:
            ESP_LOGW("PALM", "Invalid gesture received.");
            return;
    }

    // Log and write the command to UART
    ESP_LOGW("PALM", "HANDLING PALM COMMAND: %s", cmd);
    manar_uart_write(MANAR_UART_TX, cmd, strlen(cmd));  // Write command
    manar_uart_write(MANAR_UART_TX, "\n", 1);           // Add newline for clarity
}