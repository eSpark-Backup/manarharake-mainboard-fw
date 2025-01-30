#include <stdint.h>
#include <stddef.h>

typedef enum {
    GESTURE_UNKNOWN = 0,
    GESTURE_1,   // Gesture 1
    GESTURE_2,   // Gesture 2
    GESTURE_3,   // Gesture 3
    GESTURE_4,   // Gesture 4
    GESTURE_5,   // Gesture 5
    GESTURE_6,   // Gesture 6
    GESTURES_MAX
} GestureCommand;

#define NUMBER_OF_GESTURES (GESTURES_MAX - 1)


int parse_gesture(uint8_t *data, size_t len);