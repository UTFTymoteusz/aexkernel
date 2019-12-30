#include "aex/aex.h"
#include "aex/byteswap.h"
#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/string.h"
#include "aex/sys.h"

#include "aex/dev/block.h"
#include "aex/dev/name.h"
#include "aex/dev/pci.h"

#include "aex/fs/part.h"

#include "mem/page.h"

#include <stddef.h>
#include <stdint.h>

#include "ahci_data.h"
#include "ahci.h"

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
    void*  dma_buffers[32];
    size_t dma_phys_addr[32];

    char name[64];
    bool  atapi;
    struct dev_block* dev_block;
};

volatile struct ahci_hba_struct* ahci_hba;

pci_entry_t* ahci_controller;
pci_address_t ahci_address;
volatile void* ahci_bar;

struct ahci_device ahci_devices[32];
uint8_t ahci_device_amount;
uint8_t ahci_max_commands;

struct dev_block_ops ahci_block_ops;
struct dev_block_ops ahci_scsi_block_ops;

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

    while (port->cmd & HBA_CMD_CR)
        ;

    port->cmd |= HBA_CMD_FRE;
    port->cmd |= HBA_CMD_ST;
}

void ahci_stop_cmd(volatile struct ahci_hba_port_struct* port) {
    port->cmd &= ~HBA_CMD_ST;

    while (port->cmd & HBA_CMD_CR)
        ;

    port->cmd &= ~HBA_CMD_FRE;
}

int ahci_scsi_packet(struct ahci_device* dev, uint8_t* packet, int len, void* buffer) {
    //if (((size_t) buffer) & 0x01)
    //    kpanic("ahci_scsi_packet() buffer address not aligned to word");
    size_t addr = (size_t) buffer;

    volatile struct ahci_hba_port_struct* port = dev->port;
    int slot = ahci_find_cmdslot(port);

    volatile struct ahci_command_header* hdr = dev->headers;
    hdr += slot;

    hdr->cfl   = sizeof(volatile struct ahci_fis_reg_h2d) / sizeof(uint32_t) ;
    hdr->w     = 0;
    hdr->a     = 1;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    volatile struct ahci_command_table* tbl = dev->tables[slot];
    tbl->prdt[0].dbc = len - 1;
    if (addr & 1)
        tbl->prdt[0].dba = dev->dma_phys_addr[slot];
    else
        tbl->prdt[0].dba = (size_t) kppaddrof(buffer, NULL);

    if ((addr & 1) > 0) {
        printk("ahci: gotta copy\n");
        memcpy(dev->dma_buffers[slot], buffer, len);
    }
    //tbl->prdt[0].dba = (size_t) kppaddrof(buffer, NULL);

    volatile struct ahci_fis_reg_h2d* fis = (void*) (dev->tables[slot]);
    memset((void*) fis, 0, sizeof(volatile struct ahci_fis_reg_h2d));

    fis->command  = 0xA0;
    fis->c        = 1;
    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;

    // idk this makes it magically work out of a sudden
    fis->lba1 = 0x0008;
    fis->lba2 = 0x0008;

    volatile uint8_t* packet_buffer = ((void*) (dev->tables[slot])) + 64;

    memcpy((void*) packet_buffer, packet, 16);

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
    
    if ((addr & 1) > 0) {
        printk("ahci: gotta copy\n");
        memcpy(buffer, dev->dma_buffers[slot], len);
    }
    return hdr->prdbc;
}

int ahci_init_dev(struct ahci_device* dev, volatile struct ahci_hba_port_struct* port) {
    int slot = ahci_find_cmdslot(port);

    volatile struct ahci_command_header* hdr = dev->headers;
    hdr += slot;

    hdr->cfl   = sizeof(volatile struct ahci_fis_reg_h2d) / sizeof(uint32_t) ;
    hdr->w     = 0;
    hdr->a     = 0;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    volatile struct ahci_fis_reg_h2d* fis = (void*) (dev->tables[slot]);
    memset((void*) fis, 0, sizeof(volatile struct ahci_fis_reg_h2d));

    fis->command  = dev->atapi ? 0xA1 : 0xEC;
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
    char* model = (char*)(&(identify[27]));

    //if (!(dev->atapi))
        dev_name_inc("sd@", dev->name);
    //else
    //    dev_name_inc("sr@", dev->name);

    dev_block_t* block_dev = dev->dev_block;

    int i = 0;
    while ((i < 40)) {
        block_dev->model_name[i]     = model[i + 1];
        block_dev->model_name[i + 1] = model[i];

        i += 2;
    }
    block_dev->model_name[i] = '\0';

    printk("/dev/%s: Model: %s\n", dev->name, block_dev->model_name);

    if (!(dev->atapi)) {
        block_dev->flags |= DISK_PARTITIONABLE;
        block_dev->total_sectors = ((uint64_t*) (&identify[100]))[0];
    }
    else {
        uint8_t packet[12] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        uint32_t buffer[2] = { 0, 0 };

        int ret = ahci_scsi_packet(dev, packet, 8, buffer);
        if (ret == 0)
            return -1;

        buffer[0] = uint32_bswap(buffer[0]);
        buffer[1] = uint32_bswap(buffer[1]);

        block_dev->total_sectors = buffer[0];

        if (buffer[1] != 2048)
            return -1;
    }
    block_dev->sector_size = dev->atapi ? 2048 : 512;

    printk("/dev/%s: %i sectors\n", dev->name, block_dev->total_sectors);
    return 0;
}

void ahci_port_rebase(struct ahci_device* dev, volatile struct ahci_hba_port_struct* port) {
    dev->port = port;

    void* clb_virt = kpcalloc(kptopg(0x1000), NULL, 0b10011);
    void* clb_phys = kppaddrof(clb_virt, NULL);

    //write_debug("ahci: clb virt: 0x%s\n", (size_t) clb_virt & 0xFFFFFFFFFFFF, 16);
    //write_debug("ahci: clb phys: 0x%s\n", (size_t) clb_phys & 0xFFFFFFFFFFFF, 16);

    volatile struct ahci_command_header* hdr = (volatile struct ahci_command_header*)clb_virt;
    dev->headers = hdr;

    port->clb  = (size_t) clb_phys;

    void* fb_virt = kpcalloc(kptopg(0x1000), NULL, 0b10011);
    void* fb_phys = kppaddrof(fb_virt, NULL);

    dev->rx_fis = fb_virt;

    //write_debug("ahci: fb virt: 0x%s\n", (size_t) fb_virt & 0xFFFFFFFFFFFF, 16);
    //write_debug("ahci: fb phys: 0x%s\n", (size_t) fb_phys & 0xFFFFFFFFFFFF, 16);

    struct ahci_rxfis* rx_fis = fb_virt;
    rx_fis->dsfis.fis_type = AHCI_FIS_TYPE_DMA_SETUP;
    rx_fis->psfis.fis_type = AHCI_FIS_TYPE_PIO_SETUP;
    rx_fis->rfis.fis_type  = AHCI_FIS_TYPE_REG_D2H;
    rx_fis->sdbfis[0]      = AHCI_FIS_TYPE_DEV_BITS;

    port->fb  = (size_t) fb_phys;
    port->fbu = 0;

    for (size_t i = 0; i < 4; i++) {
        hdr[i].prdtl = 1;

        void* ctba_virt = kpcalloc(kptopg(0x1000), NULL, 0b10011);
        void* ctba_phys = kppaddrof(ctba_virt, NULL);

        dev->tables[i] = ctba_virt;

        hdr[i].ctba = (size_t) ctba_phys;

        void* db_virt = kpcalloc(kptopg(0x4000), NULL, 0b10011);
        void* db_phys = kppaddrof(db_virt, NULL);

        dev->dma_buffers[i] = db_virt;
        dev->dma_phys_addr[i] = (size_t) db_phys;

        volatile struct ahci_command_table* tbl = ctba_virt;
        memset((void*) tbl, 0, sizeof(volatile struct ahci_command_table));

        tbl->prdt[0].dba = (size_t) db_phys;
        tbl->prdt[0].dbc = 0x4000;
        tbl->prdt[0].i   = 1;
    }
}

int ahci_rw(struct ahci_device* dev, uint64_t start, uint16_t count, uint8_t* buffer, bool write) {
    if (((size_t) buffer) & 0x01)
        kpanic("ahci_rw() buffer address not aligned to word");

    volatile struct ahci_hba_port_struct* port = dev->port;
    int slot = ahci_find_cmdslot(port);

    volatile struct ahci_command_header* hdr = dev->headers;
    hdr += slot;

    hdr->cfl   = sizeof(volatile struct ahci_fis_reg_h2d) / sizeof(uint32_t) ;
    hdr->w     = write;
    hdr->a     = 0;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    volatile struct ahci_command_table* tbl = dev->tables[slot];
    tbl->prdt[0].dbc = (count * 512) - 1;

    size_t addr = (size_t) buffer;
    if (addr & 1)
        tbl->prdt[0].dba = dev->dma_phys_addr[slot];
    else
        tbl->prdt[0].dba = (size_t) kppaddrof(buffer, NULL);

    if (write && ((addr & 1) > 0)) {
        printk("ahci: gotta copy\n");
        memcpy(dev->dma_buffers[slot], buffer, count * 512);
    }
    volatile struct ahci_fis_reg_h2d* fis = (void*) (dev->tables[slot]);

    memset((void*) fis, 0, sizeof(volatile struct ahci_fis_reg_h2d));

    fis->command  = write ? 0x35 : 0x25;
    fis->c        = 1;
    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;

    fis->lba0 = (uint8_t) ( start & 0x0000FF);
    fis->lba1 = (uint8_t) ((start & 0x00FF00) >> 8);
    fis->lba2 = (uint8_t) ((start & 0xFF0000) >> 16);

    // lba48 assumption, ree
    fis->device = 1 << 6;

    fis->lba3 = (uint8_t) ((start & 0x0000FF000000) >> 24);
    fis->lba4 = (uint8_t) ((start & 0x00FF00000000) >> 32);
    fis->lba5 = (uint8_t) ((start & 0xFF0000000000) >> 40);

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
        printk("ahci: gotta copy\n");
        memcpy(buffer, dev->dma_buffers[slot], count * 512);
    }
    return 0;
}

int ahci_rw_scsi(struct ahci_device* dev, uint64_t start, uint16_t count, uint8_t* buffer, bool write) {
    //if (((size_t) buffer) & 0x01)
    //    kpanic("ahci_rw_scsi() buffer address not aligned to word");

    uint8_t packet[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    packet[0] = write ? 0xAA : 0xA8;

    packet[9] = count; // Sectors
    packet[2] = (start >> 24) & 0xFF; // LBA bong in big endian cuz scsi = stupid network byte order protocol
    packet[3] = (start >> 16) & 0xFF;
    packet[4] = (start >> 8)  & 0xFF;
    packet[5] = (start >> 0)  & 0xFF;

    int amnt = ahci_scsi_packet(dev, packet, count * 2048, buffer);

    return amnt ? 0 : -1;
}

int ahci_flush(struct ahci_device* dev) {
    volatile struct ahci_hba_port_struct* port = dev->port;
    int slot = ahci_find_cmdslot(port);

    volatile struct ahci_command_header* hdr = dev->headers;
    hdr += slot;

    hdr->cfl   = sizeof(volatile struct ahci_fis_reg_h2d) / sizeof(uint32_t) ;
    hdr->w     = 1;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    volatile struct ahci_command_table* tbl = dev->tables[slot];
    tbl->prdt[0].dba = dev->dma_phys_addr[slot];
    tbl->prdt[0].dbc = 256;

    volatile struct ahci_fis_reg_h2d* fis = (void*) (dev->tables[slot]);
    memset((void*) fis, 0, sizeof(volatile struct ahci_fis_reg_h2d));

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
        uint8_t sata_type = ahci_probe_port(&(ahci_hba->ports[i]));

        if (sata_type != AHCI_DEV_SATA && sata_type != AHCI_DEV_SATAPI)
            continue;

        if (sata_type == AHCI_DEV_SATAPI)
            ahci_devices[i].atapi = true;

        struct dev_block* newblock_dev = kmalloc(sizeof(struct dev_block));

        memset(newblock_dev, 0, sizeof(struct dev_block));

        ahci_devices[i].dev_block = newblock_dev;

        ahci_port_rebase(&ahci_devices[i], &ahci_hba->ports[i]);

        result = ahci_init_dev(&ahci_devices[i], &ahci_hba->ports[i]);

        if (result == -1) {
            printk(PRINTK_WARN "ahci: Device at port %i is autistic\n", i);
            continue;
        }
        newblock_dev->internal_id = i;

        if (ahci_devices[i].atapi) {
            newblock_dev->type = DISK_TYPE_OPTICAL;
            newblock_dev->block_ops = &ahci_scsi_block_ops;
        }
        else {
            newblock_dev->type = DISK_TYPE_DISK;
            newblock_dev->block_ops = &ahci_block_ops;
        }
        newblock_dev->max_sectors_at_once = 16;

        int reg_result = dev_register_block(ahci_devices[i].name, newblock_dev);
        if (reg_result < 0) {
            printk(PRINTK_WARN "/dev/%s: Registration failed\n", ahci_devices[i].name);
            continue;
        }
        fs_enum_partitions(reg_result);
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

int ahci_block_init(UNUSED int drive) {
    //printk("ahci: Initting %i\n", drive);
    return 0;
}

int ahci_block_read(int drive, uint64_t sector, uint16_t count, uint8_t* buffer) {
    ahci_rw(&(ahci_devices[drive]), sector, count, buffer, false);
    return 0;
}

int ahci_block_read_scsi(int drive, uint64_t sector, uint16_t count, uint8_t* buffer) {
    ahci_rw_scsi(&(ahci_devices[drive]), sector, count, buffer, false);
    return 0;
}

int ahci_block_write(int drive, uint64_t sector, uint16_t count, uint8_t* buffer) {
    ahci_rw(&(ahci_devices[drive]), sector, count, buffer, true);
    return 0;
}

int ahci_block_write_scsi(int drive, uint64_t sector, uint16_t count, uint8_t* buffer) {
    ahci_rw_scsi(&(ahci_devices[drive]), sector, count, buffer, true);
    return 0;
}

int ahci_block_release(int drive) {
    printk("ahci: Releasing %i\n", drive);
    return 0;
}

void ahci_init() {
    printk(PRINTK_INIT "ahci: Initializing\n");

    ahci_controller = pci_find_first_csi(0x01, 0x06, 0x01);

    if (ahci_controller == NULL) {
        printk("ahci: No controller found\n\n");
        return;
    }
    ahci_address = ahci_controller->address;

    pci_setup_entry(ahci_controller);
    pci_enable_busmaster(ahci_controller);

    int last = 0;
    for (int i = 0; i < 6; i++) {
        if (ahci_controller->bar[i].present)
            last = i;
    }

    ahci_bar = ahci_controller->bar[last].virtual_addr;
    //printk("ahci: ABAR V: 0x%X\n", (size_t) ahci_bar & 0xFFFFFFFFFFFF);
    //printk("ahci: ABAR P: 0x%X\n", (size_t) ahci_controller->bar[last].physical_addr & 0xFFFFFFFFFFFF);

    ahci_hba          = (struct ahci_hba_struct*)ahci_bar;
    ahci_max_commands = ((ahci_hba->cap >> 8) & 0b11111) + 1;

    ahci_block_ops.init    = ahci_block_init;
    ahci_block_ops.read    = ahci_block_read;
    ahci_block_ops.write   = ahci_block_write;
    ahci_block_ops.release = ahci_block_release;

    ahci_scsi_block_ops.init    = ahci_block_init;
    ahci_scsi_block_ops.read    = ahci_block_read_scsi;
    ahci_scsi_block_ops.write   = ahci_block_write_scsi;
    ahci_scsi_block_ops.release = ahci_block_release;

    ahci_count_devs();
    ahci_enumerate();
}