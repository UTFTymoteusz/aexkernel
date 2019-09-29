#pragma once

#include "pci.c"

struct pci_bar;
struct pci_address;
struct pci_entry;

typedef struct pci_bar pci_bar_t;
typedef struct pci_address pci_address_t;
typedef struct pci_entry   pci_entry_t;

void pci_init();

pci_entry_t* pci_find_first_cs(uint8_t class, uint8_t subclass);
pci_entry_t* pci_find_first_csi(uint8_t class, uint8_t subclass, uint8_t prog_if);

void pci_setup_entry(pci_entry_t* entry);
void pci_enable_busmaster(pci_entry_t* entry);