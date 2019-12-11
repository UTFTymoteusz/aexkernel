#pragma once

#define CPU_ARCH "AMD64"
#define CPU_PAGE_SIZE 0x1000

#define CPU_ENTRY_CALLER_SIZE 32

typedef uint64_t cpu_addr;

struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute((packed));

struct task_context {
    uint64_t cr3;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute((packed));
typedef struct task_context task_context_t;

struct tss {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;

    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;

    uint64_t reserved2;
    uint32_t reserved3;
} __attribute((packed));

static inline int cpuid_string(int code, uint32_t where[4]) {
    asm volatile("cpuid" : "=a"(*where), "=b"(*(where + 1)),
                "=c"(*(where + 2)), "=d"(*(where + 3)) : "a"(code));
    return (int) where[0];
}

static const int cpu_id_reg_order[3] = {1, 3, 2};
static inline char* cpu_get_vendor(char* ret) {
    ret[12] = '\0';

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
	asm volatile("ind %0, %1" : "=a" (rv) : "d" (_port));
	return rv;
}

static inline void outportd(uint16_t _port, uint32_t _data) {
	asm volatile("outd %0, %1" : : "d" (_port), "a" (_data));
}

static inline void interrupts() {
    asm volatile("sti");
}

static inline void nointerrupts() {
    asm volatile("cli");
}

static inline bool checkinterrupts() {
    size_t flags;
    asm volatile("pushf ;\
                  pop %0;" : "=r"(flags) :);
    return ((flags) & 0x200) > 0;
}

static inline void waitforinterrupt() {
    asm volatile("hlt");
}

static inline void halt() {
    while (true)
        asm volatile("hlt");
}

void idt_set_entry(uint16_t index, void* ptr, uint8_t attributes);

void irq_init();
void timer_init(int hz);