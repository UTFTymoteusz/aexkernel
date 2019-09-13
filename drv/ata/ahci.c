#pragma once

#include "dev/pci.h"
#include "stdio.h"

pci_entry_t* ahci_controller;
pci_address_t ahci_address;

void ahci_init() {
    printf("Initializing AHCI\n");

    ahci_controller = pci_find_first_by_class_subclass(0x01, 0x06);
    ahci_address = ahci_controller->address;

    if (ahci_controller == NULL) {
        printf("No AHCI controller found\n\n");
        return;
    }
    printf("\n");
}