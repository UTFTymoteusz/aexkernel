#include <aex/aex.h>
#include <aex/cbufm.h>
#include <aex/irq.h>
#include <aex/mem.h>
#include <aex/mutex.h>
#include <aex/rcode.h>

#include <aex/dev/cpu.h>
#include <aex/dev/char.h>
#include <aex/dev/dev.h>
#include <aex/dev/name.h>

#include <stdint.h>
#include <stdio.h>

#include "pcuart.h"

#define MAX_SERIAL 4

const uint16_t pcuart_ioports[] = {0x3F8, 0x2F8, 0x3E8, 0x2E8};
bool    pcuart_present[MAX_SERIAL];
cbufm_t pcuart_buffers[MAX_SERIAL];
int     pcuart_last   [MAX_SERIAL];

#define REG_DATA     0
#define REG_INT      1
#define REG_DIV_LO   0
#define REG_DIV_HI   1
#define REG_ID       2
#define REG_LCONTROL 3
#define REG_MCONTROL 4
#define REG_LSTATUS  5
#define REG_MSTATUS  6
#define REG_SCRATCH  7

int pcuart_open(int fd);
int pcuart_read(int fd, uint8_t* buffer, int len);
int pcuart_write(int fd, uint8_t* buffer, int len);
void pcuart_close(int fd);
long pcuart_ioctl(int fd, long code, void* mem);

struct dev_file_ops pcuart_ops = {
    .open  = pcuart_open,
    .read  = pcuart_read,
    .write = pcuart_write,
    .close = pcuart_close,
    .ioctl = pcuart_ioctl,
};

static mutex_t write_mutexes  [4];
static mutex_t read_mutexes   [4];

void pcuart_set_baud_div(int port, uint16_t divisor) {
    uint8_t tmp = inportb(pcuart_ioports[port] + REG_LCONTROL);
    outportb(pcuart_ioports[port] + REG_LCONTROL, tmp | 0b10000000);
    outportb(pcuart_ioports[port] + REG_DIV_LO, divisor & 0xFF);
    outportb(pcuart_ioports[port] + REG_DIV_HI, (divisor >> 8) & 0xFF);
    outportb(pcuart_ioports[port] + REG_LCONTROL, tmp);
}

void pcuart_set_params(int port, uint8_t data_bits, bool stop_bits1_5or2, uint8_t parity) {
    outportb(pcuart_ioports[port] + REG_LCONTROL, ((parity - 5) & 0x03) + (stop_bits1_5or2 ? 0b100 : 0b000) + ((parity & 0x07) << 3));
}

static inline bool verify_port(int port) {
    if (port < 0 || port > 3)
        return false;

    return true;
}

void pcuart_0_2_irq() {
    uint8_t c;
    if (pcuart_present[0]) {
        while (inportb(pcuart_ioports[0] + REG_LSTATUS) & 1) {
            c = inportb(pcuart_ioports[0] + REG_DATA);
            cbufm_write(&(pcuart_buffers[0]), &c, 1);
        }
    }
    if (pcuart_present[2]) {
        while (inportb(pcuart_ioports[2] + REG_LSTATUS) & 1) {
            c = inportb(pcuart_ioports[2] + REG_DATA);
            cbufm_write(&(pcuart_buffers[2]), &c, 1);
        }
    }
}

void pcuart_1_3_irq() {
    printf("1 or 3 idk\n");
}

void pcuart_init() {
    printf("pcuart: Initializing\n");

    int cnt = 0;

    for (int i = 0; i < MAX_SERIAL; i++) {
        outportb(pcuart_ioports[i] + REG_SCRATCH, 0x34);
        uint8_t ret = inportb(pcuart_ioports[i] + REG_SCRATCH);

        if (ret != 0x34)
            continue;

        outportb(pcuart_ioports[i] + REG_INT, 0x00);

        printf("pcuart: Found serial port %i\n", i);

        pcuart_set_baud_div(i, 2);
        pcuart_set_params(i, 8, false, 0);

        outportb(pcuart_ioports[i] + REG_MCONTROL, 0b1011);
        outportb(pcuart_ioports[i] + REG_INT, 0x01);

        struct dev_char* tts = kmalloc(sizeof(struct dev_char));
        tts->internal_id = i;
        tts->ops = &pcuart_ops;

        char name[8];
        sprintf(name, "tts%i", cnt++);
        dev_register_char(name, tts);

        pcuart_present[i] = true;
        cbufm_create(&(pcuart_buffers[i]), 256);
    }
    irq_install_immediate(4, pcuart_0_2_irq);
    irq_install_immediate(3, pcuart_1_3_irq);
}

int pcuart_open(UNUSED int fd) {
    return 0;
}

int pcuart_read(int fd, uint8_t* buffer, int len) {
    if (len > 64)
        len = 64;

    uint32_t _len = (uint32_t) len;

    while (cbufm_available(&(pcuart_buffers[fd]), pcuart_last[fd]) < _len) {
        cbufm_wait(&(pcuart_buffers[fd]), pcuart_last[fd]);
        continue;
    }
    mutex_acquire(&(read_mutexes[fd]));
    pcuart_last[fd] = cbufm_read(&(pcuart_buffers[fd]), buffer, pcuart_last[fd], len);

    mutex_release(&(read_mutexes[fd]));
    return len;
}

int pcuart_write(int fd, uint8_t* buffer, int len) {
    mutex_acquire(&(write_mutexes[fd]));

    uint16_t port = pcuart_ioports[fd] + REG_DATA;
    for (int i = 0; i < len; i++) {
        outportb(port, *buffer);
        buffer++;
    }
    mutex_release(&(write_mutexes[fd]));
    return len;
}

void pcuart_close(UNUSED int fd) {

}

long pcuart_ioctl(int fd, long code, void* mem) {
    switch (code) {
        case 0x71:
            mutex_acquire(&(read_mutexes[fd]));
            *((int*) mem) = cbufm_available(&(pcuart_buffers[fd]), pcuart_last[fd]);
            mutex_release(&(read_mutexes[fd]));
            return 0;
        default:
            return ERR_NOT_SUPPORTED;
    }
}