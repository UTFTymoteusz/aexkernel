#include "aex/time.h"

#include "dev/cpu.h"
#include "dev/input.h"

#include <stdint.h>
#include <stdio.h>

#include "ps2.h"

#define PS2_DATA 0x60
#define PS2_COMMAND 0x64
#define PS2_STATUS PS2_COMMAND

const char translation[256] = {
    0x00, 0x72, 0x00, 0x6E, // ~    F9   ~    F5
    0x6C, 0x6A, 0x6B, 0x75, // F3   F1   F2   F12
    0x00, 0x73, 0x71, 0x6F, // ~    F10  F8   F6
    0x6D, 0x09, 0x60, 0x00, // F4   tab  `    ~
    0x00, 0x17, 0x0E, 0x00, // ~    lalt lshf ~
    0x15, 0x51, 0x31, 0x00, // lctr Q    1    ~
    0x00, 0x00, 0x5A, 0x53, // ~    ~    Z    S
    0x41, 0x57, 0x32, 0x00, // A    W    2    ~ 
    0x00, 0x43, 0x58, 0x44, // ~    C    X    D
    0x45, 0x34, 0x33, 0x00, // E    4    3    ~
    0x00, 0x20, 0x56, 0x46, // ~    spac V    F
    0x54, 0x52, 0x35, 0x00, // T    R    5    ~
    0x00, 0x4E, 0x42, 0x48, // ~    N    B    H
    0x47, 0x59, 0x36, 0x00, // G    Y    6    ~
    0x00, 0x00, 0x4D, 0x4A, // ~    ~    M    J
    0x55, 0x37, 0x38, 0x00, // U    7    8    ~
    0x00, 0x2C, 0x4B, 0x49, // ~    ,    K    I
    0x4F, 0x30, 0x39, 0x00, // O    0    9    ~
    0x00, 0x2E, 0x2F, 0x4C, // ~    .    /    L
    0x3B, 0x50, 0x2D, 0x00, // ;    P    -    ~
    0x00, 0x00, 0x27, 0x00, // ~    ~    '    ~
    0x5B, 0x3D, 0x00, 0x00, // [    =    ~    ~
    0x1C, 0x0F, 0x0D, 0x5D, // cpsl rshf ret  ]
    0x00, 0x5C, 0x00, 0x00, // ~    \    ~    ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
    0x00, 0x00, 0x08, 0x00, // ~    ~    bspc ~
    0x00, 0x6B, 0x00, 0x6E, // ~    kp1  ~    kp4
    0x71, 0x00, 0x00, 0x00, // kp7  ~    ~    ~
    0x6A, 0x07, 0x6C, 0x6F, // kp0  kp.  kp2  kp5
    0x70, 0x72, 0x1B, 0x01, // kp6  kp8  esc  nmlk
    0x74, 0x05, 0x6D, 0x04, // F11  kp+  kp3  kp-
    0x03, 0x73, 0x68, 0x00, // kp*  kp9  sclk ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
    0x00, 0x00, 0x00, 0x00, // ~    ~    ~    ~
};

bool dual = false;
volatile bool release = false;
volatile bool pressed[256];
volatile uint8_t last_cmd_byte = 0;

volatile uint8_t leds = 0;

uint8_t ps2kb_send_cmd(uint8_t cmd) {
    uint8_t count = 0;
again:
    if (count > 3)
        return 0x00;

    ++count;
    last_cmd_byte = 0;

    while (inportb(PS2_STATUS) & 0x03);
    outportb(PS2_DATA, cmd);

    // spinlock pls
    while (true) {
        if (last_cmd_byte == 0xFE)
            goto again;
        else if (last_cmd_byte != 0)
            return last_cmd_byte;
    }
}

void ps2kb_update_leds() {
    ps2kb_send_cmd(0xED);
    ps2kb_send_cmd(leds);
}

void ps2kb_scrolllock(bool on) {
    if (on)
        leds |= 0b00000001;
    else
        leds &= ~0b00000001;

    ps2kb_update_leds();
}

void ps2kb_numlock(bool on) {
    if (on)
        leds |= 0b00000010;
    else
        leds &= ~0b00000010;

    ps2kb_update_leds();
}

void ps2kb_capslock(bool on) {
    if (on)
        leds |= 0b00000100;
    else
        leds &= ~0b00000100;

    ps2kb_update_leds();
}

void ps2kb_irq() {
    //printf("ps2irq");
    uint8_t scancode = inportb(PS2_DATA);

    if (scancode >= 0xFA) {
        last_cmd_byte = scancode;
        return;
    }
    if (scancode == 0xE0) {
        printf("ps2: E\n");
        return;
    }
    if (scancode == 0xF0) {
        release = true;
        return;
    }
    if (release) {
        pressed[scancode] = false;
        //printf("ps2: R 0x%x\n", translation[scancode]);

        input_kb_release(translation[scancode]);

        release = false;
    }
    else {
        if (pressed[scancode])
            return;

        pressed[scancode] = true;
        //printf("ps2: P 0x%x\n", translation[scancode]);

        input_kb_press(translation[scancode]);
    }
}

void ps2_init() {
    uint8_t resp;

    printf("ps2: Initializing\n");

    // Disabling devices
    outportb(PS2_COMMAND, 0xAD);
    outportb(PS2_COMMAND, 0xA7);

    // Flushing buffers
    while (inportb(PS2_STATUS) & 0x01)
        inportb(PS2_DATA);

    //printf("ps2: Buffers flushed\n");

    // Reading configuration byte
    outportb(PS2_COMMAND, 0x20);
    while (!(inportb(PS2_STATUS) & 0x01));

    uint8_t cfg = inportb(PS2_DATA);
    if (cfg & 0b00010000)
        dual = true;

    if (dual)
        printf("ps2: Two devices found\n");

    cfg &= 0b10111100;

    // Writing cfg byte
    outportb(PS2_COMMAND, 0x60);
    outportb(PS2_DATA, cfg);

    //printf("ps2: Configuration byte written\n");

    // Self test
    int count = 0;
    while (count++ < 4) {
        outportb(PS2_COMMAND, 0xAA);

        while (!(inportb(PS2_STATUS) & 0x01));
        resp = inportb(PS2_DATA);
        if (resp != 0x55) {
            //printf("ps2: Self test not passed (0x%x)\n", resp);
            continue;
        }
        break;
    }
    if (count >= 4) {
        sleep(2000);
        return;
    }
        
    //printf("ps2: Self test passed\n");
    
    // Writing cfg byte again just incase
    outportb(PS2_COMMAND, 0x60);
    outportb(PS2_DATA, cfg);

    cfg |= 0x01;
    if (dual)
        cfg |= 0x02;

    // Interface tests
    outportb(PS2_COMMAND, 0xAB);

    while (!(inportb(PS2_STATUS) & 0x01));
    resp = inportb(PS2_DATA);
    if (resp != 0x00)
        ;

    if (dual) {
        outportb(PS2_COMMAND, 0xA9);

        while (!(inportb(PS2_STATUS) & 0x01));
        uint8_t resp = inportb(PS2_DATA);

        if (resp != 0x00)
            ;
    }
    //printf("ps2: Interface test passed\n");

    // And again for enabling the devices
    outportb(PS2_COMMAND, 0x60);
    outportb(PS2_DATA, cfg);
    
    // Enabling 
    outportb(PS2_COMMAND, 0xAE);
    if (dual)
        outportb(PS2_COMMAND, 0xA8);

    //printf("ps2: Devices enabled\n");

    // Resetting 
    outportb(PS2_COMMAND, 0xFF);
    if (dual)
        outportb(PS2_COMMAND, 0xFF);

    while (inportb(PS2_STATUS) & 0x01)
        inportb(PS2_DATA);

    //printf("ps2: Devices reset\n");

    irq_install(1, ps2kb_irq);

    printf("ps2: Initialized\n");  

    //ps2kb_numlock(true);
    //ps2kb_capslock(true);
    //ps2kb_scrolllock(true);

    //sleep(15000);
    //while (inportb(PS2_STATUS) & 0x02);
    //outportb(PS2_COMMAND, 0xFE);

    /*while (true) {
        sleep(250);
        ps2kb_scrolllock(true);
        sleep(250);
        ps2kb_capslock(true);
        sleep(250);
        ps2kb_numlock(true);

        sleep(250);
        ps2kb_scrolllock(false);
        sleep(250);
        ps2kb_capslock(false);
        sleep(250);
        ps2kb_numlock(false);
    }*/
}