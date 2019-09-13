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

struct pci_entry {
    pci_address_t address;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class;
    uint8_t subclass;
    
    uint8_t revision_id;
    uint8_t prog_if;
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

    printf("PCI ");

    tty_set_color_ansi(93);
    write_debug("%s:", address.bus, 10);
    write_debug("%s:", address.device, 10);
    write_debug("%s", address.function, 10);
    tty_set_color_ansi(97);

    write_debug(" - 0x%s.", entry->class, 16);
    write_debug("0x%s\n", entry->subclass, 16);

    klist_set(&pci_entries, pci_entries.count, (void*)entry);
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

pci_entry_t* pci_find_first_by_class_subclass(uint8_t class, uint8_t subclass) {

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

void pci_init() {

    klist_init(&pci_entries);

    printf("Initializing PCI\n");

    for(uint16_t bus = 0; bus < 256; bus++)
        pci_check_bus(bus);
        
    printf("\n");
}