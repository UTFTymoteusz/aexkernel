#pragma once

#include "dev/pci.c"

struct pci_address;
typedef struct pci_address pci_address_t;

struct pci_bar;
typedef struct pci_bar pci_bar_t;

struct pci_entry;
typedef struct pci_entry pci_entry_t;

uint16_t pci_config_read_word(pci_address_t address, uint8_t offset);

uint16_t pci_get_vendor(pci_address_t address);
uint16_t pci_get_header_type(pci_address_t address);

void pci_check_function(pci_address_t address);
void pci_check_device(pci_address_t address);
void pci_check_bus(uint8_t bus);

void pci_setup_entry(pci_entry_t* entry);
void pci_enable_busmaster(pci_entry_t* entry);

void pci_init();