#pragma once

void timer_set_hz(int hz) {
    int divisor = 1193180 / hz;       /* Calculate our divisor */

    outportb(0x43, 0x36);             /* Set our command byte 0x36 */
    outportb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outportb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

void timer_init(int hz) {
    timer_set_hz(hz);
}