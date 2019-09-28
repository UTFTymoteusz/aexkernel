#pragma once

#include "ahci_data.c"

#include "dev/pci.h"
#include "stdio.h"

#define SATA_SIG_ATA   0x00000101
#define SATA_SIG_ATAPI 0xEB140101
#define SATA_SIG_SEMB  0xC33C0101
#define SATA_SIG_PM    0x96690101

#define AHCI_DEV_NULL   0
#define AHCI_DEV_SATA   1
#define AHCI_DEV_SATAPI 2
#define AHCI_DEV_SEMB   3
#define AHCI_DEV_PM     4

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

#define HBA_CMD_ST  0x0001
#define HBA_CMD_FRE 0x0010
#define HBA_CMD_FR  0x4000
#define HBA_CMD_CR  0x8000

struct ahci_device {
    volatile struct ahci_hba_port_struct* port;

    volatile struct ahci_command_header* headers;
    volatile struct ahci_command_table*  tables[32];

    volatile void* rx_fis;
    void* dma_buffers[32];
    size_t dma_phys_addr[32];

    char* name;
};

volatile struct ahci_hba_struct* ahci_hba;

pci_entry_t* ahci_controller;
pci_address_t ahci_address;
volatile void* ahci_bar;

struct ahci_device ahci_devices[32];
uint8_t ahci_device_amount;
uint8_t ahci_max_commands;

uint16_t ahci_probe_port(volatile struct ahci_hba_port_struct* port) {

    uint32_t ssts = port->ssts;

    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;

    if ((ipm != 0x01) || (det != 0x03))
        return AHCI_DEV_NULL;

    switch (port->sig) {
        case SATA_SIG_ATAPI:
            return AHCI_DEV_SATAPI;
        case SATA_SIG_SEMB:
            return AHCI_DEV_SEMB;
        case SATA_SIG_PM:
            return AHCI_DEV_PM;
        case SATA_SIG_ATA:
            return AHCI_DEV_SATA;
    }
    return AHCI_DEV_NULL;
}

int ahci_find_cmdslot(volatile struct ahci_hba_port_struct* port) {

    uint32_t slots = (port->sact | port->ci);

    for (int i = 0; i < ahci_max_commands; i++)
        if (!(slots & (1 << i)))
            return i;

    return -1;
}

void ahci_start_cmd(volatile struct ahci_hba_port_struct* port) {

    port->cmd &= ~HBA_CMD_ST;

    while (port->cmd & HBA_CMD_CR);

    port->cmd |= HBA_CMD_FRE;
    port->cmd |= HBA_CMD_ST;
}
void ahci_stop_cmd(volatile struct ahci_hba_port_struct* port) {
    
    port->cmd &= ~HBA_CMD_ST;

    while (port->cmd & HBA_CMD_CR);

    port->cmd &= ~HBA_CMD_FRE;
}

int ahci_init_dev(struct ahci_device* dev, volatile struct ahci_hba_port_struct* port) {

    int slot = ahci_find_cmdslot(port);

    volatile struct ahci_command_header* hdr = dev->headers;
    hdr += slot;

    hdr->cfl   = sizeof(volatile struct ahci_fis_reg_h2d) / sizeof(uint32_t);
    hdr->w     = 0;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    volatile struct ahci_fis_reg_h2d* fis = (void*)(dev->tables[slot]);
    memset((void*)fis, 0, sizeof(volatile struct ahci_fis_reg_h2d));

    fis->command  = 0xEC;
    fis->c        = 1;
    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;

    while (port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ));

    ahci_start_cmd(port);
    port->ci = 1 << slot;

    while (true) {
        if (!(port->ci & (1 << slot)))
            break;

        if (port->is & (1 << 30))
            return -1;
    }

    if (port->is & (1 << 30))
        return -1;
    
    ahci_stop_cmd(port);

    uint16_t* identify = dev->dma_buffers[slot];
    char* model = (char*)(&identify[27]);

    dev->name = kmalloc(8);
    dev_increment_name("sd@", dev->name);

    printf("/dev/%s: Model: ", dev->name);

    int i = 0;
    while ((i < 40)) {
        putchar(model[i + 1]);
        putchar(model[i]);

        i += 2;
    }
    printf("\n");

    printf("/dev/%s: ", dev->name);
    write_debug("%s sectors\n", ((uint64_t*)(&identify[100]))[0], 10);

    return 0;
}

void ahci_port_rebase(struct ahci_device* dev, volatile struct ahci_hba_port_struct* port) {

    dev->port = port;

    void* clb_virt = mempg_nextc(mempg_to_pages(0x1000), NULL, NULL, 0b10011);
    void* clb_phys = mempg_paddr(clb_virt, NULL);

    //write_debug("ahci: clb virt: 0x%s\n", (size_t)clb_virt & 0xFFFFFFFFFFFF, 16);
    //write_debug("ahci: clb phys: 0x%s\n", (size_t)clb_phys & 0xFFFFFFFFFFFF, 16);

    volatile struct ahci_command_header* hdr = (volatile struct ahci_command_header*)clb_virt;
    dev->headers = hdr;

    port->clb  = (size_t)clb_phys;
    //port->clbu = 0;

    void* fb_virt = mempg_nextc(mempg_to_pages(0x1000), NULL, NULL, 0b10011);
    void* fb_phys = mempg_paddr(fb_virt, NULL);

    dev->rx_fis = fb_virt;

    //write_debug("ahci: fb virt: 0x%s\n", (size_t)fb_virt & 0xFFFFFFFFFFFF, 16);
    //write_debug("ahci: fb phys: 0x%s\n", (size_t)fb_phys & 0xFFFFFFFFFFFF, 16);

    struct ahci_rxfis* rx_fis = fb_virt;
    rx_fis->dsfis.fis_type = AHCI_FIS_TYPE_DMA_SETUP;
    rx_fis->psfis.fis_type = AHCI_FIS_TYPE_PIO_SETUP;
    rx_fis->rfis.fis_type  = AHCI_FIS_TYPE_REG_D2H;
    rx_fis->sdbfis[0]      = AHCI_FIS_TYPE_DEV_BITS;

    port->fb  = (size_t)fb_phys;
    port->fbu = 0;

    for (size_t i = 0; i < 8; i++) {
        hdr[i].prdtl = 2;

        void* ctba_virt = mempg_nextc(mempg_to_pages(0x1000), NULL, NULL, 0b10011);
        void* ctba_phys = mempg_paddr(ctba_virt, NULL);

        dev->tables[i] = ctba_virt;

        hdr[i].ctba = (size_t)ctba_phys;

        void* db_virt = mempg_nextc(mempg_to_pages(0x1000), NULL, NULL, 0b10011);
        void* db_phys = mempg_paddr(db_virt, NULL);

        dev->dma_buffers[i] = db_virt;
        dev->dma_phys_addr[i] = (size_t)db_phys;

        volatile struct ahci_command_table* tbl = ctba_virt;
        memset((void*)tbl, 0, sizeof(volatile struct ahci_command_table));

        tbl->prdt[0].dba = (size_t)db_phys;
        tbl->prdt[0].dbc = 4096;
        tbl->prdt[0].i   = 1;
    }
}

int ahci_rw(struct ahci_device* dev, uint64_t start, uint16_t count, uint8_t* buffer, bool write) {

    volatile struct ahci_hba_port_struct* port = dev->port;
    int slot = ahci_find_cmdslot(port);

    volatile struct ahci_command_header* hdr = dev->headers;
    hdr += slot;

    hdr->cfl   = sizeof(volatile struct ahci_fis_reg_h2d) / sizeof(uint32_t);
    hdr->w     = write;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    volatile struct ahci_command_table* tbl = dev->tables[slot];
    tbl->prdt[0].dbc = (count * 512) - 1;

    size_t addr = (size_t)buffer;
    if (addr & 1)
        tbl->prdt[0].dba = dev->dma_phys_addr[slot];
    else
        tbl->prdt[0].dba = (size_t)mempg_paddr(buffer, NULL);

    if (write && ((addr & 1) > 0)) {
        printf("ahci: gotta copy\n");
        memcpy(dev->dma_buffers[slot], buffer, count * 512);
    }

    volatile struct ahci_fis_reg_h2d* fis = (void*)(dev->tables[slot]);
    memset((void*)fis, 0, sizeof(volatile struct ahci_fis_reg_h2d));

    fis->command  = write ? 0x35 : 0x25;
    fis->c        = 1;
    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;

    fis->lba0 = (uint8_t)( start & 0x0000FF);
    fis->lba1 = (uint8_t)((start & 0x00FF00) >> 8);
    fis->lba2 = (uint8_t)((start & 0xFF0000) >> 16);

    // lba48 assumption, ree
    fis->device = 1 << 6;

    fis->lba3 = (uint8_t)((start & 0x0000FF000000) >> 24);
    fis->lba4 = (uint8_t)((start & 0x00FF00000000) >> 32);
    fis->lba5 = (uint8_t)((start & 0xFF0000000000) >> 40);

    fis->countl = count & 0xff;
    fis->counth = (count >> 8) & 0xff;

    while (port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ));

    ahci_start_cmd(port);
    port->ci = 1 << slot;

    while (true) {
        if (!(port->ci & (1 << slot)))
            break;

        if (port->is & (1 << 30))
            return -1;
    }

    if (port->is & (1 << 30))
        return -1;
    
    ahci_stop_cmd(port);

    if (!write && ((addr & 1) > 0)) {
        printf("ahci: gotta copy\n");
        memcpy(buffer, dev->dma_buffers[slot], count * 512);
    }

    return 0;
}

int ahci_flush(struct ahci_device* dev) {

    volatile struct ahci_hba_port_struct* port = dev->port;
    int slot = ahci_find_cmdslot(port);

    volatile struct ahci_command_header* hdr = dev->headers;
    hdr += slot;

    hdr->cfl   = sizeof(volatile struct ahci_fis_reg_h2d) / sizeof(uint32_t);
    hdr->w     = 1;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    volatile struct ahci_command_table* tbl = dev->tables[slot];
    tbl->prdt[0].dba = dev->dma_phys_addr[slot];
    tbl->prdt[0].dbc = 256;

    volatile struct ahci_fis_reg_h2d* fis = (void*)(dev->tables[slot]);
    memset((void*)fis, 0, sizeof(volatile struct ahci_fis_reg_h2d));

    fis->command  = 0xE7;
    fis->c        = 1;
    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;

    while (port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ));

    ahci_start_cmd(port);
    port->ci = 1 << slot;

    while (true) {
        if (!(port->ci & (1 << slot)))
            break;

        if (port->is & (1 << 30))
            return -1;
    }

    if (port->is & (1 << 30))
        return -1;
    
    ahci_stop_cmd(port);
    return 0;
}

void ahci_enumerate() {
    int result;

    for (int i = 0; i < ahci_device_amount; i++) {

        if (ahci_probe_port(&(ahci_hba->ports[i])) != AHCI_DEV_SATA)
            continue;

        ahci_port_rebase(&ahci_devices[i], &ahci_hba->ports[i]);

        result = ahci_init_dev(&ahci_devices[i], &ahci_hba->ports[i]);

        if (result == -1) {
            write_debug("ahci: Device at port %s is autistic\n", i, 10);
            return;
        }

        // blah blah blah whatever, initialization
        //write_debug("ahci: Device at port %s is actually working, finally\n", i, 10);
    }
}

void ahci_count_devs() {

    uint32_t pi = ahci_hba->pi;
    
    for (int i = 0; i < 32; i++) {

        if (!(pi & (1 << i)))
            continue;

        if ((i + 1) > ahci_device_amount)
            ahci_device_amount = i + 1;
    }
}

void ahci_init() {
    printf("ahci: Initializing\n");

    ahci_controller = pci_find_first_csi(0x01, 0x06, 0x01);

    if (ahci_controller == NULL) {
        printf("ahci: No controller found\n\n");
        return;
    }

    ahci_address = ahci_controller->address;

    pci_setup_entry(ahci_controller);
    pci_enable_busmaster(ahci_controller);

    ahci_bar = ahci_controller->bar[5].virtual_addr;
    //write_debug("ahci: ABAR V: 0x%s\n", (size_t)ahci_bar & 0xFFFFFFFFFFFF, 16);
    //write_debug("ahci: ABAR P: 0x%s\n", (size_t)ahci_controller->bar[5].physical_addr & 0xFFFFFFFFFFFF, 16);

    ahci_max_commands = ((ahci_hba->cap >> 8) & 0b11111) + 1;

    ahci_hba = (struct ahci_hba_struct*)ahci_bar;
    //ahci_common = (void*)0x400000; // Temporary

    ahci_count_devs();
    ahci_enumerate();

    uint8_t* buffer = kmalloc(1024);

    /*for (int i = 0; i < 1024; i++)
        buffer[i] = '1';//i % 128;
        //buffer[i] = '\0';//i % 128;

    for (int i = 2; i < 6; i += 2) {
        ahci_rw(&(ahci_devices[0]), i, 2, buffer, true);
        //ahci_flush(&(ahci_devices[0]));

        //putchar(buffer[0x45]);
        //putchar('\n');
    }*/
}