#pragma once

#include "aex/klist.h"
#include "aex/kmem.h"
#include "aex/time.h"

#include "dev/cpu.h"
#include "dev/tty.h"

#include "stdio.h"
#include "string.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

struct pci_address {
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    bool success;
};
typedef struct pci_address pci_address_t;

struct pci_bar {
    uint8_t type : 4;
    bool is_io;
    bool prefetchable;
    bool present;

    void* physical_addr;
    void* virtual_addr;
    size_t length;
};
typedef struct pci_bar pci_bar_t;

struct pci_entry {
    pci_address_t address;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class;
    uint8_t subclass;
    
    uint8_t revision_id;
    uint8_t prog_if;

    pci_bar_t bar[6];
};
typedef struct pci_entry pci_entry_t;

struct klist pci_entries;

uint16_t pci_config_read_word(pci_address_t address, uint8_t offset) {

    uint8_t bus    = address.bus; 
    uint8_t device = address.device; 
    uint8_t func   = address.function;

    nointerrupts();

    uint32_t address_d;

    address_d = (uint32_t)((bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outportd(PCI_CONFIG_ADDRESS, address_d);

    uint16_t data = (uint16_t)(inportd(PCI_CONFIG_DATA) >> ((offset & 2) * 8) & 0xFFFF);
    interrupts();

    return data;
}
uint32_t pci_config_read_dword(pci_address_t address, uint8_t offset) {

    uint8_t bus    = address.bus; 
    uint8_t device = address.device; 
    uint8_t func   = address.function;

    nointerrupts();

    uint32_t address_d;

    address_d = (uint32_t)((bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outportd(PCI_CONFIG_ADDRESS, address_d);

    uint32_t data = (uint32_t)inportd(PCI_CONFIG_DATA);
    interrupts();

    return data;
}

void pci_config_write_dword(pci_address_t address, uint8_t offset, uint32_t value) {

    uint8_t bus    = address.bus; 
    uint8_t device = address.device; 
    uint8_t func   = address.function;

    nointerrupts();

    uint32_t address_d;

    address_d = (uint32_t)((bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);

    outportd(PCI_CONFIG_ADDRESS, address_d);
    outportd(PCI_CONFIG_DATA, value);
    interrupts();
}

uint16_t pci_get_vendor(pci_address_t address) {

    uint16_t vendor;

    vendor = pci_config_read_word(address, 0); 

    if (vendor == 0xFFFF)
        return vendor;

    vendor = pci_config_read_word(address, 2);

    return vendor;
}
uint16_t pci_get_header_type(pci_address_t address) {
    return pci_config_read_word(address, 14);
}

void pci_check_function(pci_address_t address) {

    pci_entry_t* entry = (pci_entry_t*)kmalloc(sizeof(pci_entry_t));
    memset((void*)entry, 0, sizeof(pci_entry_t));

    entry->address.bus      = address.bus;
    entry->address.device   = address.device;
    entry->address.function = address.function;
    entry->address.success  = true;

    entry->vendor_id = pci_config_read_word(address, 0);
    entry->device_id = pci_config_read_word(address, 2);

    uint16_t bigbong;
    bigbong = pci_config_read_word(address, 8);
    entry->revision_id = bigbong & 0xFF;
    entry->prog_if     = (bigbong >> 8) & 0xFF;

    bigbong = pci_config_read_word(address, 10);
    entry->class    = (bigbong >> 8) & 0xFF;
    entry->subclass = bigbong & 0xFF;

    uint64_t addr, len;
    uint64_t bar, bar2;
    uint32_t offset = 16;
    uint32_t mask = 0;
    uint8_t type;
    bool io;

    for (int i = 0; i < 6; i++) {
        bar  = pci_config_read_dword(address, offset);
        bar2 = pci_config_read_dword(address, offset + 4);
        type = (bar >> 1) & 0x03;
        io   = (bar & 0x01) > 0;

        if (bar == 0) {
            offset += 4;
            continue;
        }

        mask = io ? 0xFFFFFFFC : 0xFFFFFFF0;
        addr = bar & mask;

        if (type == 0x02)
            addr |= (bar2 << 32);

        entry->bar[i].physical_addr = (void*)addr;
        entry->bar[i].present = true;
        entry->bar[i].is_io   = io;
        entry->bar[i].prefetchable = (bar & 0x08) > 0;

        pci_config_write_dword(address, offset, 0xFFFFFFFF);
        len = ~(pci_config_read_dword(address, offset) & mask) + 1;

        if (type == 0x01)
            len &= 0xFFFF;
        if ((len & 0xFFFF0000) > 0)
            len &= 0xFFFF;

        entry->bar[i].length  = len;

        pci_config_write_dword(address, offset, bar);

        /*write_debug("Addr: 0x%s ", addr, 16);
        write_debug("Len: %s ", entry->bar[i].length, 10);
        printf(type == 0x02 ? "| 64 bit " : "| 32 bit ");
        printf(entry->bar[i].is_io ? "| IO\n" : "| Not IO\n");*/

        if (type == 0x02)
            offset += 4;

        offset += 4;
    }

    printf("PCI ");

    tty_set_color_ansi(93);
    printf("%i:%i:%i", address.bus, address.device, address.function);
    tty_set_color_ansi(97);

    printf(" - 0x%x.0x%x\n", entry->class, entry->subclass);

    klist_set(&pci_entries, pci_entries.count, (void*)entry);

    //sleep(2000);
}
void pci_check_device(pci_address_t address) {

    uint8_t function = 0;
    uint16_t vendor  = pci_get_vendor(address);

    if (vendor == 0xFFFF)
        return;
        
    pci_check_function(address);

    uint16_t header = pci_get_header_type(address);

    if ((header & 0x80) > 0) {

        for (function = 1; function < 8; function++) {

            address.function = function;

            if (pci_get_vendor(address) == 0xFFFF)
                continue;

            pci_check_function(address);
        }
    }
}
void pci_check_bus(uint8_t bus) {

    pci_address_t address;

    address.bus      = bus;
    address.device   = 0;
    address.function = 0;

    for (uint8_t device = 0; device < 32; device++) {
        address.device = device;
        pci_check_device(address);
    }
}

pci_entry_t* pci_find_first_cs(uint8_t class, uint8_t subclass) {

    klist_entry_t* klist_entry = NULL;
    pci_entry_t* entry = NULL;

    while (true) {
        entry = (pci_entry_t*)klist_iter(&pci_entries, &klist_entry);

        if (entry == NULL)
            return NULL;

        if (entry->class == class && entry->subclass == subclass)
            return entry;
    }
    return NULL;
}
pci_entry_t* pci_find_first_csi(uint8_t class, uint8_t subclass, uint8_t prog_if) {

    klist_entry_t* klist_entry = NULL;
    pci_entry_t* entry = NULL;

    while (true) {
        entry = (pci_entry_t*)klist_iter(&pci_entries, &klist_entry);

        if (entry == NULL)
            return NULL;

        if (entry->class == class && entry->subclass == subclass && entry->prog_if == prog_if)
            return entry;
    }
    return NULL;
}
// To do: map prefetchable as write through in paging and improve small io areas merge
void pci_setup_entry(pci_entry_t* entry) {
    
    //uint32_t amnt;
    //uint64_t size;
    uint64_t len;
    size_t addr;
    void* virt_addr;
    void* virt_addr_prev = NULL;
    void* phys_addr_prev = NULL;
    
    for (int i = 0; i < 6; i++) {

        if (!entry->bar[i].present)
            continue;

        if (entry->bar[i].virtual_addr != NULL)
            continue;

        //amnt = 1;
        //size = CPU_PAGE_SIZE;
        len  = entry->bar[i].length;
        addr = (size_t)entry->bar[i].physical_addr;

        /*write_debug("%s. ", i, 10);
        write_debug("Addr: 0x%s ", addr, 16);
        write_debug("Len: %s ", entry->bar[i].length, 10);

        printf("| ");
        switch (entry->bar[i].type) {
            case 0x00:
                printf("32 bit");
                break;
            case 0x01:
                printf("16 bit");
                break;
            case 0x02:
                printf("64 bit");
                break;
            case 0x03:
                printf("Unknown");
                break;
        }
        printf(" ");
        printf(entry->bar[i].is_io ? "| IO\n" : "| Mem\n");*/


        if ((void*)((size_t)phys_addr_prev + len) == entry->bar[i].physical_addr)
            virt_addr = (void*)((size_t)virt_addr_prev + len);
        else
            virt_addr = (void*)(((size_t)mempg_mapto(mempg_to_pages(len), NULL, entry->bar[i].physical_addr, NULL, 0b11011)) + (addr & 0xFFF));
        
        entry->bar[i].virtual_addr = virt_addr;

        phys_addr_prev = entry->bar[i].physical_addr;
        virt_addr_prev = virt_addr;

        //write_debug("Pages: %s\n", mempg_to_pages(len), 10);
        //write_debug("0x%s\n", (size_t)entry->bar[i].virtual_addr & 0xFFFFFFFFFFFFFF, 16);
    }
}

void pci_enable_busmaster(pci_entry_t* entry) {
    if (pci_config_read_dword(entry->address, 0x04) & (1 << 2)) 
        return;

    pci_config_write_dword(entry->address, 0x04, pci_config_read_dword(entry->address, 0x04) | (1 << 2));
}

void pci_init() {

    klist_init(&pci_entries);

    printf("Initializing PCI\n");

    for(uint16_t bus = 0; bus < 256; bus++)
        pci_check_bus(bus);

    printf("\n");
}