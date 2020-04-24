#pragma once

struct ahci_device {
    volatile struct ahci_hba_struct* hba;
    volatile struct ahci_hba_port_struct* port;

    volatile struct ahci_command_header* headers;
    volatile struct ahci_command_table*  tables[32];

    uint8_t max_commands;

    volatile void* rx_fis;
    void*  dma_buffers[32];
    size_t dma_phys_addr[32];

    char name[12];
    char model[64];

    uint64_t sector_count;
    uint16_t sector_size;

    bool atapi;
    struct dev_block* dev_block;
};
typedef struct ahci_device ahci_device_t;

void ahci_init();