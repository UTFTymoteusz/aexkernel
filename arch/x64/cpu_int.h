#pragma once

#define CPU_ARCH "AMD64"
#define CPU_PAGE_SIZE 0x1000

static inline int cpuid_string(int code, uint32_t where[4]) {
    asm volatile("cpuid":"=a"(*where),"=b"(*(where+1)),
                "=c"(*(where+2)),"=d"(*(where+3)):"a"(code));
    return (int)where[0];
}

static const int cpu_id_reg_order[3] = {1, 3, 2};
static inline char* cpu_get_vendor(char* ret) {
    ret[12] = 0;

    uint32_t where[4];
    cpuid_string(0, where);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            ret[i*4 + j] = (where[cpu_id_reg_order[i]] >> (j * 8)) & 0xFF;

    return ret;
}

static inline uint8_t inportb(uint16_t _port) {
	uint8_t rv;
	asm volatile("inb %0, %1" : "=a" (rv) : "dN" (_port));
	return rv;
}

static inline void outportb(uint16_t _port, uint8_t _data) {
	asm volatile("outb %0, %1" : : "dN" (_port), "a" (_data));
}

static inline uint16_t inportw(uint16_t _port) {
	uint16_t rv;
	asm volatile("inw %0, %1" : "=a" (rv) : "dN" (_port));
	return rv;
}

static inline void outportw(uint16_t _port, uint16_t _data) {
	asm volatile("outw %0, %1" : : "dN" (_port), "a" (_data));
}

static inline uint32_t inportd(uint16_t _port) {
	uint32_t rv;
	asm volatile("ind eax, dx" : "=a" (rv) : "d" (_port));
    
	return rv;
}

static inline void outportd(uint16_t _port, uint32_t _data) {
	asm volatile("outd dx, eax" : : "d" (_port), "a" (_data));
}

static inline void interrupts() {
    asm volatile("sti");
}

static inline void nointerrupts() {
    asm volatile("cli");
}

static inline void waitforinterrupt() {
    asm volatile("hlt");
}

static inline void halt() {
    while (true)
        asm volatile("hlt");
}