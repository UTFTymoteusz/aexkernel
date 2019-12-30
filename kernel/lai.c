#include "aex/aex.h"
#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/time.h"
#include "aex/sys.h"

#include "aex/dev/pci.h"

#include "kernel/acpi.h"

#include "lai/host.h"

void laihost_log(int level, const char *msg) {
    switch (level) {
        case LAI_WARN_LOG:
            printk(PRINTK_WARN "lai: %s\n", msg);
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
    return kpmap(kptopg(count), (void*) address, NULL, 0b11011);
}

void laihost_unmap(void* pointer, size_t count) {
    kpunmap(pointer, kptopg(count), NULL);
}

void* laihost_scan(const char* sig, size_t index) {
    return acpi_find_table((char*) sig, index);
}

void laihost_outb(uint16_t port, uint8_t val) {
    outportb(port, val);
}
void laihost_outw(uint16_t port, uint16_t val) {
    outportw(port, val);
}
void laihost_outd(uint16_t port, uint32_t val) {
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

void laihost_pci_writeb(UNUSED uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val) {
    pci_address_t address = {
        .bus = bus,
        .device = slot,
        .function = fun,
    };
    pci_config_write_byte(address, offset, val);
}
void laihost_pci_writew(UNUSED uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val) {
    pci_address_t address = {
        .bus = bus,
        .device = slot,
        .function = fun,
    };
    pci_config_write_word(address, offset, val);
}
void laihost_pci_writed(UNUSED uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val) {
    pci_address_t address = {
        .bus = bus,
        .device = slot,
        .function = fun,
    };
    pci_config_write_dword(address, offset, val);
}

uint8_t laihost_pci_readb(UNUSED uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
    pci_address_t address = {
        .bus = bus,
        .device = slot,
        .function = fun,
    };
    return pci_config_read_byte(address, offset);
}
uint16_t laihost_pci_readw(UNUSED uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
    pci_address_t address = {
        .bus = bus,
        .device = slot,
        .function = fun,
    };
    return pci_config_read_word(address, offset);
}
uint32_t laihost_pci_readd(UNUSED uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
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