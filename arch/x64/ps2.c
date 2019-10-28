#include "aex/time.h"

#include "dev/cpu.h"

#include <stdint.h>
#include <stdio.h>

#include "ps2.h"

#define PS2_DATA 0x60
#define PS2_COMMAND 0x64
#define PS2_STATUS PS2_COMMAND

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
    if (scancode == 0xF0) {
        release = true;
        return;
    }
    if (release) {
        pressed[scancode] = false;
        printf("ps2: R 0x%x\n", scancode);

        release = false;
    }
    else {
        if (pressed[scancode])
            return;

        pressed[scancode] = true;
        printf("ps2: P 0x%x\n", scancode);
    }
}

void ps2_init() {
    uint8_t resp;

    // Disabling devices
    outportb(PS2_COMMAND, 0xAD);
    outportb(PS2_COMMAND, 0xA7);

    printf("ps2: Devices disabled\n");

    // Flushing buffers
    while (inportb(PS2_STATUS) & 0x01)
        inportb(PS2_DATA);

    printf("ps2: Buffers flushed\n");

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

    printf("ps2: Configuration byte written\n");

    // Self test
    int count = 0;
    while (count++ < 4) {
        outportb(PS2_COMMAND, 0xAA);

        while (!(inportb(PS2_STATUS) & 0x01));
        resp = inportb(PS2_DATA);
        if (resp != 0x55) {
            printf("ps2: Self test not passed (0x%x)\n", resp);
            continue;
        }
        break;
    }
    if (count >= 4) {
        sleep(2000);
        return;
    }
        
    printf("ps2: Self test passed\n");
    
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
    printf("ps2: Interface test passed\n");

    // And again for enabling the devices
    outportb(PS2_COMMAND, 0x60);
    outportb(PS2_DATA, cfg);
    
    // Enabling 
    outportb(PS2_COMMAND, 0xAE);
    if (dual)
        outportb(PS2_COMMAND, 0xA8);

    printf("ps2: Devices enabled\n");

    // Resetting 
    outportb(PS2_COMMAND, 0xFF);
    if (dual)
        outportb(PS2_COMMAND, 0xFF);

    while (inportb(PS2_STATUS) & 0x01)
        inportb(PS2_DATA);

    printf("ps2: Devices reset\n");

    irq_install(1, ps2kb_irq);

    printf("ps2: Setup complete\n");  

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