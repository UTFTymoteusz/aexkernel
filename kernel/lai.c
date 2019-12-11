#include "aex/kmem.h"
#include "aex/time.h"

#include "dev/cpu.h"
#include "dev/pci.h"

#include "kernel/acpi.h"
#include "kernel/sys.h"

#include "mem/page.h"

#include <stdio.h>

#include "lai/host.h"

void laihost_log(int level, const char *msg) {
    switch (level) {
        case LAI_WARN_LOG:
            printf("lai: %s\n", msg);
            break;
    }
}

void laihost_panic(const char* msg) {
    kpanic((char*) msg);
    while (1);
}

void* laihost_malloc(size_t size) {
    return kmalloc(size);
}

void* laihost_realloc(void* ptr, size_t size) {
    return krealloc(ptr, size);
}

void laihost_free(void* ptr) {
    kfree(ptr);
}

void* laihost_map(size_t address, size_t count) {
    //printf("map attempt to 0x%x, len %i\n", address, count);
    void* ret = mempg_mapto(mempg_to_pages(count), (void*) address, NULL, 0b11011);

    //printf("ret 0x%x\n", (size_t) ret & 0xFFFFFFFFFFFF);
    return ret;
}

void laihost_unmap(void* pointer, size_t count) {
    //printf("unmap 0x%x, len %i\n", (size_t) pointer & 0xFFFFFFFFFFFF);
    mempg_unmap(pointer, mempg_to_pages(count), NULL);
}

void* laihost_scan(const char* sig, size_t index) {
    //printf("laihost_scan %c%c%c%c, %i\n", sig[0], sig[1], sig[2], sig[3], index);
    void* ret = acpi_find_table((char*) sig, index);
    //printf("boi: 0x%x\n", (size_t) ret & 0xFFFFFFFFFFFF);
    return ret;
}

void laihost_outb(uint16_t port, uint8_t val) {
    //printf("outb 0x%x: 0x%x\n", port, val);
    outportb(port, val);
}
void laihost_outw(uint16_t port, uint16_t val) {
    //printf("outw 0x%x: 0x%x\n", port, val);
    outportw(port, val);
}
void laihost_outd(uint16_t port, uint32_t val) {
    //printf("outd 0x%x: 0x%x\n", port, val);
    outportd(port, val);
}

uint8_t laihost_inb(uint16_t port) {
    return inportb(port);
}
uint16_t laihost_inw(uint16_t port) {
    return inportw(port);
}
uint32_t laihost_ind(uint16_t port) {
    return inportd(port);
}

void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val) {
    kpanic("laihost_pci_writeb()");
}
void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val) {
    kpanic("laihost_pci_writew()");
}
void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val) {
    kpanic("laihost_pci_writed()");
}

uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
    pci_address_t address = {
        .bus = bus,
        .device = slot,
        .function = fun,
    };
    return pci_config_read_byte(address, offset);
}
uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
    pci_address_t address = {
        .bus = bus,
        .device = slot,
        .function = fun,
    };
    return pci_config_read_word(address, offset);
}
uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
    pci_address_t address = {
        .bus = bus,
        .device = slot,
        .function = fun,
    };
    return pci_config_read_dword(address, offset);
}

void laihost_sleep(uint64_t ms) {
    sleep(ms);
}