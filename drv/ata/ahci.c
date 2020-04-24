#include "aex/aex.h"
#include "aex/byteswap.h"
#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/string.h"

#include "aex/dev/block.h"
#include "aex/dev/name.h"
#include "aex/dev/tree.h"

#include "aex/sys/pci.h"

//#include "aex/fs/part.h"

#include "mem/page.h"

#include <stddef.h>
#include <stdint.h>

#include "ahci/ahci_types.h"
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

struct ahci_device ahci_devices[32];
uint8_t ahci_device_amount;
uint8_t ahci_max_commands;

struct dev_block_ops ahci_block_ops;
struct dev_block_ops ahci_scsi_block_ops;

struct {
    int ata;
    int atapi;
} ahci_counters;

void ahci_enumerate(volatile struct ahci_hba_struct* hba);

static int ahci_check(device_t* device);
static int ahci_bind (device_t* device);

driver_t ahci_pci_driver = {
    .name  = "ahci",
    .check = ahci_check,
    .bind  = ahci_bind,
};

static int ahci_check(device_t* device) {
    if (dev_get_attribute(device, "class") == 0x01 
    && dev_get_attribute(device, "subclass") == 0x06
    && dev_get_attribute(device, "prog_if") == 0x01)
        return 0;

    return -1;
}

static int ahci_bind(device_t* device) {
    phys_addr bar = 0;
    int size   = 0;
    int rss_id = 0;

    dresource_t* dress;
    while ((dress = dev_get_resource(device, rss_id++)) != NULL) {
        if (dress->type != DRT_Memory)
            continue;

        bar = dress->start;
        size = dress->end - dress->start;
    }
    pci_enable_busmaster(device);

    volatile struct ahci_hba_struct* hba = kpmap(kptopg(size), bar, NULL, 
                                            PAGE_NOCACHE | PAGE_WRITE);

    ahci_max_commands = ((hba->cap >> 8) & 0b11111) + 1;
    ahci_enumerate(hba);

    printk("ahci: Bound to pci/%s\n", device->name);
    return 0;
}

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

int ahci_scsi_packet(struct ahci_device* dev, uint8_t* packet, int len, void* buffer);

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

    int i = 0;
    while (i < 40) {
        dev->model[i]     = model[i + 1];
        dev->model[i + 1] = model[i];

        i += 2;
    }
    dev->model[i] = '\0';

    printk("sata/%s: Model: %s\n", dev->name, dev->model);

    if (!dev->atapi)
        dev->sector_count = ((uint64_t*) (&identify[100]))[0];
    else {
        uint8_t packet[12] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        uint32_t buffer[2] = { 0, 0 };

        int ret = ahci_scsi_packet(dev, packet, 8, buffer);
        if (ret == 0)
            return -1;

        buffer[0] = uint32_bswap(buffer[0]);
        buffer[1] = uint32_bswap(buffer[1]);

        dev->sector_count = buffer[0];

        if (buffer[1] != 2048)
            return -1;
    }
    dev->sector_size = dev->atapi ? 2048 : 512;

    printk("/dev/%s: %li sectors\n", dev->name, dev->sector_count);
    return 0;
}

void ahci_port_rebase(struct ahci_device* dev, volatile struct ahci_hba_port_struct* port) {
    dev->port = port;

    void* clb_virt = kpcalloc(kptopg(0x1000), NULL, 0b10011);
    phys_addr clb_phys = kppaddrof(clb_virt, NULL);

    //write_debug("ahci: clb virt: 0x%s\n", (size_t) clb_virt & 0xFFFFFFFFFFFF, 16);
    //write_debug("ahci: clb phys: 0x%s\n", (size_t) clb_phys & 0xFFFFFFFFFFFF, 16);

    volatile struct ahci_command_header* hdr = (volatile struct ahci_command_header*) clb_virt;
    dev->headers = hdr;

    port->clb  = (size_t) clb_phys;

    void* fb_virt = kpcalloc(kptopg(0x1000), NULL, 0b10011);
    phys_addr fb_phys = kppaddrof(fb_virt, NULL);

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
        phys_addr ctba_phys = kppaddrof(ctba_virt, NULL);

        dev->tables[i] = ctba_virt;

        hdr[i].ctba = (size_t) ctba_phys;

        void* db_virt = kpcalloc(kptopg(0x4000), NULL, 0b10011);
        phys_addr db_phys = kppaddrof(db_virt, NULL);

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

void ahci_enumerate(volatile struct ahci_hba_struct* hba) {
    int result;

    uint32_t pi = hba->pi;

    for (int i = 0; i < 32; i++) {
        if (!(pi & (1 << i)))
            continue;

        device_t device = {0};
        ahci_device_t* ahci_device = kzmalloc(sizeof(ahci_device_t));

        device.device_private = ahci_device;

        ahci_device->hba = hba;
        ahci_device->max_commands = ((hba->cap >> 8) & 0b11111) + 1;

        uint8_t sata_type = ahci_probe_port(&hba->ports[i]);
        if (sata_type != AHCI_DEV_SATA && sata_type != AHCI_DEV_SATAPI)
            continue;

        if (sata_type == AHCI_DEV_SATAPI)
            ahci_device->atapi = true;

        if (ahci_device->atapi)
            snprintf(device.name, sizeof(device.name), 
                "atapi%i", ahci_counters.atapi++);
        else
            snprintf(device.name, sizeof(device.name), 
                "ata%i", ahci_counters.ata++);

        strcpy(ahci_device->name, device.name);

        ahci_port_rebase(ahci_device, &hba->ports[i]);
        result = ahci_init_dev(ahci_device, &hba->ports[i]);
        if (result == -1) {
            printk(PRINTK_WARN "sata/%s: Is autistic\n", ahci_device->name);
            continue;
        }

        dev_add("sata", &device);
    }
}

extern void ahci_sddrv_init();
extern void ahci_srdrv_init();

void ahci_init() {
    printk(PRINTK_INIT "ahci: Initializing\n");

    dev_add_bus("sata");
    dev_register_driver("pci", &ahci_pci_driver);

    ahci_sddrv_init();
    ahci_srdrv_init();

    task_tsleep(3000);
}