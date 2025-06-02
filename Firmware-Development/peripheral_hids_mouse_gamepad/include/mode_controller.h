#ifndef MODE_CONTROLLER_H
#define MODE_CONTROLLER_H

typedef enum {
	MANAR_MODE_MOUSE,
	MANAR_MODE_GAMEPAD,
	MANAR_MODE_PALM_PCB,
	MANAR_MODE_VR,
}manar_mode_t;

typedef enum {
    GEST_UNKNOWN = 0,
    GEST_1,
    GEST_2,
    GEST_3,
    GEST_4,
    GEST_5,
    GEST_6,
} manar_gesture_t;

typedef void (*mode_handler_t)(void);

typedef struct {
    const char *cmd;
    manar_mode_t mode;
    mode_handler_t handler;
} mode_map_t;

typedef struct {
	const char *cmd;
	manar_gesture_t gesture;
	mode_handler_t handler;
} gesture_map_t;

void process_uart_input(const char *input);
int manar_mode_init(void);

#endif // MODE_CONTROLLER_H