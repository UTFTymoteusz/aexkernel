#pragma once

#include <string.h>

#include "aex/kmem.h"

#include "dev/cpu.h"
#include "kernel/syscall.h"

#define BASE_STACK_SIZE 128
#define KERNEL_STACK_SIZE 8192

enum task_state {
    TASK_STATE_NORMAL = 0,
    TASK_STATE_SLEEP = 1,
};

struct task_descriptor {
    void* kernel_stack;
    void* paging_root;
    
    size_t id;

    bool kernelmode;

    size_t state;

    size_t sreg_a;
    size_t sreg_b;

    struct task_context* context;

    struct task_descriptor* next;
};
typedef struct task_descriptor task_descriptor_t;

task_descriptor_t* task_current;
task_context_t* task_current_context;

task_descriptor_t* task0;
task_descriptor_t* idle_task;

size_t next_id = 1;

void bigbong() {

	while (true) {
		printf("bigbong B\n");
		waitforinterrupt();
		waitforinterrupt();
		waitforinterrupt();
	}
}
void userbong() {
    //asm volatile("xchg bx, bx");
    //asm volatile("mov r12, 5; mov rdi, 0x111; mov rsi, 0x222; mov rdx, 0x333; mov r8, 0x444; mov r9, 0x555; mov r10, 0x666; syscall");

	while (true)
        asm volatile("mov r12, 0; mov rdi, 1000; syscall");
}

void idle_task_loop() {
	while (true)
		waitforinterrupt();
}

task_descriptor_t* task_find_last() {
    task_descriptor_t* task = idle_task;

    //static char boibuffer[24];
    //printf("first task: %s\n", itoa(task0->id, boibuffer, 10));

    while (task->next != NULL)
        task = task->next;

    //printf("last task: %s\n", itoa(task->id, boibuffer, 10));

    return task;
}

task_descriptor_t* task_create(bool kernelmode, void* entry, size_t page_dir_addr) {
    task_descriptor_t* new_task = kmalloc(sizeof(task_descriptor_t));
    task_context_t* new_context = kmalloc(sizeof(task_context_t));

    memset(new_task, 0, sizeof(task_descriptor_t));
    memset(new_context, 0, sizeof(task_context_t));

    cpu_fill_context(new_context, kernelmode, entry, page_dir_addr);
    cpu_set_stack(new_context, kmalloc(BASE_STACK_SIZE), BASE_STACK_SIZE);
    
    new_task->kernel_stack = (void*)((size_t)kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE);

    //static char boibuffer[24];
    //printf("Created task %s ", itoa(next_id, boibuffer, 10));
    //printf("with kernel stack @ 0x%s\n", itoa(((uint64_t)(new_task->kernel_stack)) & 0xFFFFFFFFFFFF, boibuffer, 16));
    //printf("At pos 0x%s\n", itoa(((uint64_t)new_task) & 0xFFFFFFFFFFFF, boibuffer, 16));

    new_task->context = new_context;
    new_task->kernelmode = kernelmode;

    new_task->id = next_id++;
    new_task->next = NULL;

    return new_task;
}
void task_insert(task_descriptor_t* task) {
    task_find_last()->next = task;
}

void dump() {
    static char boibuffer[24];
    printf("id: %s\n", itoa((uint32_t)(task_current->id), boibuffer, 10));

    printf("rax: 0x%s ", itoa((uint32_t)(task_current_context->rax), boibuffer, 16));
    printf("rbx: 0x%s ", itoa((uint32_t)(task_current_context->rbx), boibuffer, 16));
    printf("rcx: 0x%s ", itoa((uint32_t)(task_current_context->rcx), boibuffer, 16));
    printf("rdx: 0x%s ", itoa((uint32_t)(task_current_context->rdx), boibuffer, 16));
    printf("\n");
    printf("rsp: 0x%s ", itoa((uint32_t)(task_current_context->rsp), boibuffer, 16));
    printf("rbp: 0x%s ", itoa((uint32_t)(task_current_context->rbp), boibuffer, 16));
    printf("\n");
    printf("rip: 0x%s ", itoa((uint32_t)(task_current_context->rip), boibuffer, 16));
    printf("cs: 0x%s ", itoa((uint32_t)(task_current_context->cs), boibuffer, 16));
    printf("rfl: 0x%s ", itoa((uint32_t)(task_current_context->rflags), boibuffer, 16));
    printf("rsp: 0x%s ", itoa((uint32_t)(task_current_context->rsp), boibuffer, 16));
    printf("ss: 0x%s ", itoa((uint32_t)(task_current_context->ss), boibuffer, 16));
    printf("\n\n");
}

extern void task_enter();
void task_switch() {

    // Task state has been saved, hopefully

    task_current = task_current->next;

    if (task_current == NULL)
        task_current = idle_task;

    task_current_context = task_current->context;

    //dump();
    task_enter();
}

void hcf_cunt() {
    while (true) ;
}

void sleep(size_t delay) {
    write_debug("I'm tired for %sms...\n", delay, 10);

    task_current->state = TASK_STATE_SLEEP;
    task_current->sreg_a = delay;

    // Make this not reliant on x64
    asm volatile("\
    mov rax, ss; \
    push rax; \
    push rsp; \
    pushfq;   \
    mov rax, cs; \
    push rax; \
    \
    pushq offset 1f;  \
    \
    call task_tss;   \
    call task_save;  \
    \
    jmp task_switch; \
    \
    1: \
    add rsp, 8 * 2; \
    ret");
}

void task_init() {

    asm volatile("xchg bx, bx");

    idle_task = task_create(true, idle_task_loop, 0);

    task0 = task_create(true, NULL, 0);
    task_insert(task0);

    task_insert(task_create(false, userbong, 0));

    //static char boibuffer[24];
    //printf("bigbong: 0x%s\n", itoa((uint64_t)bigbong, boibuffer, 16));

    write_debug("bigboi 0x%s\n", (size_t)sleep & 0xFFFFFFFFFFFFFF, 16);

    //mem_page_assign(userbong, (void*)((size_t)userbong & 0xFFFFFFF), NULL, 0x07);

    task_current = task0;
    task_current_context = task0->context;

    syscalls[0] = sleep;
}