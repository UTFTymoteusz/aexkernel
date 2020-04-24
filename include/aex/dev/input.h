#pragma once

#include "aex/event.h"

#include <stddef.h>
#include <stdint.h>

#define INPUT_SHIFT_FLAG   0x01
#define INPUT_CONTROL_FLAG 0x02
#define INPUT_ALT_FLAG     0x04

event_t input_event;

// Signals a key press
void input_kb_press(uint8_t key);
// Signals a key release
void input_kb_release(uint8_t key);

int input_fetch_keymap(char* name, char keymap[1024]);

size_t   input_kb_get(uint16_t* k, uint8_t* modifiers, size_t last);
uint32_t input_kb_available(size_t last);
size_t   input_kb_sync();