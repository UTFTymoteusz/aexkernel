#include "aex/aex.h"
#include "aex/cbufm.h"
#include "aex/mem.h"
#include "aex/mutex.h"
#include "aex/time.h"

#include "aex/proc/proc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kernel/init.h"
#include "aex/dev/input.h"

cbufm_t* input_kb_cbufm = NULL;
cbufm_t* input_ms_cbufm = NULL;

bool pressed_keys[256];
volatile int prscnt = 0;

uint8_t input_flags = 0;

uint8_t def_keymap[1024] = {
    '\0', '\0', '/' , '*' , '-' , '+' , '\r', 127 , '\b', '\t', '\0', '\0', '\0', '\r', 0x0E, 0x0F, // 0x00
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x10
    ' ' , '\0', '\0', '\0', '\0', '\0', '\0', '\'', '\0', '\0', '\0', '\0', ',' , '-' , '.' , '/' , // 0x20
    '0' , '1' , '2' , '3' , '4' , '5' , '6' , '7' , '8' , '9' , '\0', '\0', '\0', '=' , '\0', '\0', // 0x30
    '\0', 'a' , 'b' , 'c' , 'd' , 'e' , 'f' , 'g' , 'h' , 'i' , 'j' , 'k' , 'l' , 'm' , 'n' , 'o' , // 0x40
    'p' , 'q' , 'r' , 's' , 't' , 'u' , 'v' , 'w' , 'x' , 'y' , 'z' , '[' , '\0', ']' , '\0', '\0', // 0x50
    '`' , '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x60
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x70
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x80
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x90
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xA0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xB0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xC0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xD0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xE0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xF0
    // SHIFT
    '\0', '\0', '/' , '*' , '-' , '+' , '\r', 127 , '\b', '\t', '\0', '\0', '\0', '\r', 0x0E, 0x0F, // 0x00
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x10
    ' ' , '\0', '\0', '\0', '\0', '\0', '\0', '"' , '\0', '\0', '\0', '\0', '<' , '_' , '>' , '?' , // 0x20
    ')' , '!' , '@' , '#' , '$' , '%' , '^' , '&' , '*' , '(' , '\0', '\0', '\0', '+' , '\0', '\0', // 0x30
    '\0', 'A' , 'B' , 'C' , 'D' , 'E' , 'F' , 'G' , 'H' , 'I' , 'J' , 'K' , 'L' , 'M' , 'N' , 'O' , // 0x40
    'P' , 'Q' , 'R' , 'S' , 'T' , 'U' , 'V' , 'W' , 'X' , 'Y' , 'Z' , '{' , '\0', '}' , '\0', '\0', // 0x50
    '~' , '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x60
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x70
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x80
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0x90
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xA0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xB0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xC0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xD0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xE0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', // 0xF0
};

mutex_t input_mutex = 0;
bqueue_t input_bqueue;

void input_loop();

void input_init() {
    input_kb_cbufm = kmalloc(sizeof(cbufm_t));
    input_ms_cbufm = kmalloc(sizeof(cbufm_t));

    io_create_bqueue(&input_bqueue);

    cbufm_create(input_kb_cbufm, 512);
    cbufm_create(input_ms_cbufm, 4096);

    struct thread* th = thread_create(process_current, input_loop, true);
    th->name = "Input Thread";

    task_set_priority(th->task, PRIORITY_HIGH);
    thread_start(th);
}

inline void append_key_event(uint8_t key) {
    mutex_acquire(&input_mutex);

    uint16_t key_w = key;

    cbufm_write(input_kb_cbufm, &input_flags, 1);
    cbufm_write(input_kb_cbufm, (uint8_t*) &key_w, 1);

    io_unblockall(&input_bqueue);
    mutex_release(&input_mutex);
}

void input_kb_press(uint8_t key) {
    switch (key) {
        case 0x0E:
        case 0x0F:
            input_flags |= INPUT_SHIFT_FLAG;
            break;
        case 0x17:
            input_flags |= INPUT_ALT_FLAG;
            break;
        case 0x18:
            input_flags |= INPUT_ALT_FLAG | INPUT_CONTROL_FLAG;
            break;
        case 0x15:
        case 0x1B:
            input_flags |= INPUT_CONTROL_FLAG;
            break;
        default:
            if (!pressed_keys[key])
                append_key_event(key);

            prscnt = 0;
            pressed_keys[key] = true;
            break;
    }
}

void input_kb_release(uint8_t key) {
    switch (key) {
        case 0x0E:
        case 0x0F:
            input_flags &= ~INPUT_SHIFT_FLAG;
            break;
        case 0x17:
            input_flags &= ~INPUT_ALT_FLAG;
            break;
        case 0x18:
            input_flags &= ~(INPUT_ALT_FLAG | INPUT_CONTROL_FLAG);
            break;
        case 0x15:
        case 0x1B:
            input_flags &= ~INPUT_CONTROL_FLAG;
            break;
        default:
            prscnt = 0;
            pressed_keys[key] = false;
            break;
    }
}

void input_loop() {
    while (true) {
        sleep(50);
        prscnt++;

        if (prscnt < 10)
            continue;

        for (int i = 0; i < 256; i++)
            if (pressed_keys[i])
                append_key_event(i);
    }
}

int input_fetch_keymap(UNUSED char* name, char keymap[1024]) {
    memcpy(keymap, def_keymap, 1024);
    return 0;
}

size_t input_kb_get(uint8_t* k, uint8_t* modifiers, size_t last) {
    mutex_acquire(&input_mutex);
    
    if (cbufm_available(input_kb_cbufm, last) < 2) {
        *k = 0;
        mutex_release(&input_mutex);
        return last;
    }
    size_t ret = cbufm_read(input_kb_cbufm, modifiers, last, 1);
    ret = cbufm_read(input_kb_cbufm, k, last + 1, 1);
    mutex_release(&input_mutex);

    return ret;
}

uint32_t input_kb_available(size_t last) {
    return cbufm_available(input_kb_cbufm, last) / 2;
}

void input_kb_wait(size_t last) {
    while (true) {
        if (cbufm_available(input_kb_cbufm, last) < 2) {
            io_block(&input_bqueue);
            continue;
        }
        break;
    }
}

size_t input_kb_sync() {
    return cbufm_sync(input_kb_cbufm);
}