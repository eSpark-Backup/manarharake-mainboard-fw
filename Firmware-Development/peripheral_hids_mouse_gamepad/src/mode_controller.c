#include "mode_controller.h"
#include "bluetooth_config.h"

#define MAX_MANAR_MODE MANAR_MODE_VR
#define MIN_MANAR_MODE MANAR_MODE_MOUSE

static manar_gesture_t current_gesture = GEST_UNKNOWN;
static manar_mode_t manar_mode __attribute__((section(".non_init")));

void handle_mode_palm_pcb();
void handle_mode_mouse();
void handle_mode_gamepad(); 
void handle_mode_vr();

char* manar_mode_to_string(manar_mode_t mode) {
    switch (mode) {
        case MANAR_MODE_MOUSE:
            return "Mouse";
        case MANAR_MODE_GAMEPAD:
            return "Gamepad";
        case MANAR_MODE_PALM_PCB:
            return "Palm PCB";
        case MANAR_MODE_VR:
            return "VR";
        default:
            return "Unknown Mode";
    }  
}

const mode_map_t command_table[] = {
    {"M1", MANAR_MODE_PALM_PCB, handle_mode_palm_pcb},
    {"M2", MANAR_MODE_MOUSE, handle_mode_mouse},
    {"M3", MANAR_MODE_GAMEPAD, handle_mode_gamepad},
    {"M4", MANAR_MODE_VR, handle_mode_vr},
};

static gesture_map_t gesture_table[] = {
    {"1", GEST_1, NULL},
    {"2", GEST_2, NULL},
    {"3", GEST_3, NULL},
    {"4", GEST_4, NULL},
    {"5", GEST_5, NULL},
    {"6", GEST_6, NULL},
    {NULL, GEST_UNKNOWN, NULL},
};

static void mode_switcher(manar_mode_t new_mode) {
    if (manar_mode != new_mode) {
        manar_mode = new_mode;
        printk("Switching to mode: %s\n", manar_mode_to_string(manar_mode));
        NVIC_SystemReset();
    } else {
        printk("Already in mode: %s\n", manar_mode_to_string(manar_mode));
    }
}

void handle_mode_palm_pcb(void) {
    printk("%s\n", __FUNCTION__);
    mode_switcher(MANAR_MODE_PALM_PCB);
}

void handle_mode_mouse(void) {
    printk("%s\n", __FUNCTION__);
    mode_switcher(MANAR_MODE_MOUSE);
}

void handle_mode_gamepad(void) {
    printk("%s\n", __FUNCTION__);
    mode_switcher(MANAR_MODE_GAMEPAD);
}

void handle_mode_vr(void) {
	printk("%s\n", __FUNCTION__);
    mode_switcher(MANAR_MODE_VR);
}

void process_uart_input(const char *input) {
    for (int i = 0; command_table[i].cmd != NULL; i++) {
        if (strcmp(input, command_table[i].cmd) == 0) {
            if (command_table[i].handler) {
                command_table[i].handler();
            } else {
                printk("No handler for command: %s\n", input);
            }
            return;
        }
    }

    if(ble_config_ble_paired) {
        for (int i = 0; gesture_table[i].cmd != NULL; i++) {
            if (strcmp(input, gesture_table[i].cmd) == 0) {
                current_gesture = gesture_table[i].gesture;
                if (gesture_table[i].handler) {
                    gesture_table[i].handler();
                } else {
                    printk("No handler for gesture: %s\n", input);
                }
                return;
            }
        }
    }
    else {
        printk("No Piared Devices for Control!");
        return;
    }

    printk("Unknown input: %s\n", input);
}

void mode_controller_set_gesture_callback(manar_gesture_t gesture, mode_handler_t handler) {
    for (int i = 0; gesture_table[i].cmd != NULL; i++) {
        if (gesture_table[i].gesture == gesture) {
            gesture_table[i].handler = handler;
            return;
        }
    }
    printk("Gesture %d not found in table");
};

int manar_mode_init(void) {
    if(manar_mode > MAX_MANAR_MODE) {
        printk("Invalid manar_mode value: %d\n", manar_mode);
        manar_mode = MIN_MANAR_MODE;
    }
    printk("Manar mode initialized: %d\n", manar_mode);
    return manar_mode;
}
