#include "aex/string.h"

#include "aex/dev/block.h"
#include "aex/dev/name.h"
#include "aex/dev/tree.h"

#include "aex/dev/class/disk.h"

#include "drv/ata/ahci.h"
#include "drv/ata/ahci/ahci_types.h"

int ahci_srdrv_check(device_t* device);
int ahci_srdrv_bind (device_t* device);

int ahci_sr_read (device_t* dev, uint64_t sector, uint16_t count, uint8_t* buffer);
int ahci_sr_write(device_t* dev, uint64_t sector, uint16_t count, uint8_t* buffer);

driver_t ahci_srdrv = {
    .name  = "sr",
    .check = ahci_srdrv_check,
    .bind  = ahci_srdrv_bind,
};

void ahci_srdrv_init() {
    dev_register_driver("sata", &ahci_srdrv);
}

int ahci_srdrv_check(device_t* device) {
    ahci_device_t* ahci_dev = device->device_private;
    if (!ahci_dev->atapi)
        return -1;

    return 0;
}

int ahci_srdrv_bind(device_t* device) {
    device->class_data = kzmalloc(sizeof(class_disk_t));
    class_disk_t* data = device->class_data;

    ahci_device_t* ahci_dev = device->device_private;
    data->sector_count = ahci_dev->sector_count;
    data->sector_size  = ahci_dev->sector_size;
    data->burst_max    = 16;

    data->type = DISK_TYPE_OPTICAL;

    data->read  = ahci_sr_read;
    data->write = ahci_sr_write;

    dev_name_inc("sr@", data->name);
    dev_set_class(device, "disk");

    printk("sr: Bound to '%s'\n", device->name);
    return 0;
}


static int ahci_find_cmdslot(ahci_device_t* dev) {
    uint32_t slots = (dev->port->sact | dev->port->ci);

    for (int i = 0; i < dev->max_commands; i++)
        if (!(slots & (1 << i)))
            return i;

    return -1;
}

static void ahci_start_cmd(volatile struct ahci_hba_port_struct* port) {
    port->cmd &= ~HBA_CMD_ST;

    while (port->cmd & HBA_CMD_CR)
        ;

    port->cmd |= HBA_CMD_FRE;
    port->cmd |= HBA_CMD_ST;
}

static void ahci_stop_cmd(volatile struct ahci_hba_port_struct* port) {
    port->cmd &= ~HBA_CMD_ST;

    while (port->cmd & HBA_CMD_CR)
        ;

    port->cmd &= ~HBA_CMD_FRE;
}


int ahci_scsi_packet(struct ahci_device* dev, uint8_t* packet, int len, void* buffer) {
    int slot = ahci_find_cmdslot(dev);
    volatile struct ahci_hba_port_struct* port = dev->port;

    volatile struct ahci_command_header* hdr = dev->headers;
    hdr += slot;

    hdr->cfl   = sizeof(volatile struct ahci_fis_reg_h2d) / sizeof(uint32_t) ;
    hdr->w     = 0;
    hdr->a     = 1;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    // Make DMA smarter later

    volatile struct ahci_command_table* tbl = dev->tables[slot];
    tbl->prdt[0].dbc = len - 1;
    tbl->prdt[0].dba = dev->dma_phys_addr[slot];
        
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
    memcpy(buffer, dev->dma_buffers[slot], len);

    return hdr->prdbc;
}


int ahci_rw_scsi(ahci_device_t* dev, uint64_t start, uint16_t count, uint8_t* buffer, bool write) {
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

int ahci_sr_read(device_t* dev, uint64_t sector, uint16_t count, uint8_t* buffer) {
    return ahci_rw_scsi(dev->device_private, sector, count, buffer, false);
}

int ahci_sr_write(device_t* dev, uint64_t sector, uint16_t count, uint8_t* buffer) {
    return ahci_rw_scsi(dev->device_private, sector, count, buffer, true);
}
