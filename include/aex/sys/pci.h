#pragma once

#include "aex/dev/tree.h"

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

    phys_addr physical_addr;
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

pci_entry_t* pci_find_first_cs(uint8_t class, uint8_t subclass);
pci_entry_t* pci_find_first_csi(uint8_t class, uint8_t subclass, uint8_t prog_if);

void pci_setup_entry(pci_entry_t* entry);
void pci_enable_busmaster(device_t* pci_dev);

uint8_t pci_config_read_byte(pci_address_t address, uint8_t offset);
uint16_t pci_config_read_word(pci_address_t address, uint8_t offset);
uint32_t pci_config_read_dword(pci_address_t address, uint8_t offset);

void pci_config_write_byte(pci_address_t address, uint8_t offset, uint8_t value);
void pci_config_write_word(pci_address_t address, uint8_t offset, uint16_t value);
void pci_config_write_dword(pci_address_t address, uint8_t offset, uint32_t value);

uint16_t pci_get_vendor(pci_address_t address);
uint16_t pci_get_header_type(pci_address_t address);