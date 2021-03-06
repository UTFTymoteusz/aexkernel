#include "aex/aex.h"
#include "aex/cbufm.h"
#include "aex/event.h"
#include "aex/mem.h"
#include "aex/spinlock.h"
#include "aex/string.h"

#include "aex/proc/task.h"
#include "aex/sys/sys.h"

#include <stdbool.h>
#include <stdint.h>

#include "kernel/init.h"
#include "aex/dev/input.h"

cbufm_t* input_kb_cbufm = NULL;
cbufm_t* input_ms_cbufm = NULL;

bool pressed_keys[256];
volatile int prscnt = 0;

uint8_t input_flags = 0;

event_t input_event = {0};

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

spinlock_t input_spinlock = {
    .val = 0,
    .name = "input",
};

void input_loop();

void input_init() {
    input_kb_cbufm = kmalloc(sizeof(cbufm_t));
    input_ms_cbufm = kmalloc(sizeof(cbufm_t));

    cbufm_create(input_kb_cbufm, 512);
    cbufm_create(input_ms_cbufm, 4096);

    tid_t th_id = task_tcreate(KERNEL_PROCESS, input_loop, true);
    task_tstart(th_id);
}

static void append_key_event(uint8_t key) {
    static bool sysrq = false;

    if (sysrq) {
        sys_sysrq(key);
        sysrq = false;
        
        return;
    }

    if (key == 0x75 && (input_flags & INPUT_CONTROL_FLAG) && (input_flags & INPUT_ALT_FLAG)) {
        sysrq = true;
        return;
    }
    else if (key >= 0x6A && key <= 0x71 && (input_flags & INPUT_CONTROL_FLAG) && (input_flags & INPUT_ALT_FLAG)) {
        sys_funckey(key);
        return;
    }
    
    spinlock_acquire(&input_spinlock);

    uint16_t key_w = key;

    cbufm_write(input_kb_cbufm, &input_flags, 1);
    cbufm_write(input_kb_cbufm, (uint8_t*) &key_w, 1);

    event_trigger_all(&input_event);
    spinlock_release(&input_spinlock);
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
        task_tsleep(50);
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

size_t input_kb_get(uint16_t* k, uint8_t* modifiers, size_t last) {
    spinlock_acquire(&input_spinlock);
    
    *k = 0;

    if (cbufm_available(input_kb_cbufm, last) < 2) {
        spinlock_release(&input_spinlock);
        return last;
    }
    size_t ret = cbufm_read(input_kb_cbufm, modifiers, last, 1);
    ret = cbufm_read(input_kb_cbufm, (uint8_t*) k, last + 1, 1);
    spinlock_release(&input_spinlock);

    return ret;
}

uint32_t input_kb_available(size_t last) {
    spinlock_acquire(&input_spinlock);
    uint32_t ret = cbufm_available(input_kb_cbufm, last) / 2;
    
    spinlock_release(&input_spinlock);
    return ret;
}

size_t input_kb_sync() {
    return cbufm_sync(input_kb_cbufm);
}