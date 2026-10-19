#pragma once
#include "runtime.h"

typedef struct xinput_state {
    DWORD packet;
    WORD buttons;
    BYTE left_trigger, right_trigger;
    SHORT left_x, left_y, right_x, right_y;
} xinput_state;

BOOL sony_pad_read(xinput_state* state, BOOL scan);

void sony_pad_convert(const void* raw, xinput_state* state);
