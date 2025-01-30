#include "gestures.h"

typedef enum {
    FIST = 0,
    PALM,
    THUMBS,
    POINT,
    PINCH,
    TRIPOD
} PalmCommand;

void handle_palm_command(GestureCommand gesture);