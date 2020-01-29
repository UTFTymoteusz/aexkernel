#include "aex/byteswap.h"
#include "aex/irq.h"
#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/string.h"
#include "aex/time.h"

#include "aex/dev/block.h"
#include "aex/dev/name.h"

#include "aex/fs/part.h"

#include "ata.h"

#define ATA_PRIMARY_PORT 0x1F0
#define ATA_SECONDARY_PORT 0x170

#define ATA_PRIMARY_STATUS 0x3F6
#define ATA_SECONDARY_STATUS 0x376

#define ATA_PORT_DATA 0
#define ATA_PORT_ERROR 1
#define ATA_PORT_SECTOR_COUNT 2
#define ATA_PORT_SECTOR_NUMBER 3
#define ATA_PORT_LBA_LO 3
#define ATA_PORT_CYLINDER_LO 4
#define ATA_PORT_LBA_MI 4
#define ATA_PORT_CYLINDER_HI 5
#define ATA_PORT_LBA_HI 5
#define ATA_PORT_DRIVE_SELECT 6
#define ATA_PORT_STATUS 7
#define ATA_PORT_COMMAND 7

#define ATA_FLAG_LBA   0b001
#define ATA_FLAG_LBA28 0b010
#define ATA_FLAG_LBA48 0b100

int selected[2];
const int ports[2] = {0x1F0, 0x170};

uint16_t flags[4];

volatile bool ata_lock[2];
volatile bool irq_wait[2];

struct dev_block_ops ata_block_ops;
struct dev_block_ops ata_scsi_block_ops;

struct ata_device {
    char* name;
    bool  atapi;
    struct dev_block* dev_block;
};

struct ata_device ata_devices[4];

void ata_pri_irq() {
    irq_wait[0] = false;
}

void ata_sec_irq() {
    irq_wait[1] = false;
}

void ata_select_bus_drive(int bus, int drive) {
    if (selected[bus] == drive)
        return;

    for (int i = 0; i < 5; i++)
        outportb(ports[bus] + ATA_PORT_DRIVE_SELECT, (drive == 0 ? 0xA0 : 0xB0) | ((flags[bus * 2 + drive] & ATA_FLAG_LBA) ? 0b01000000 : 0));

    selected[bus] = drive;
}

inline void ata_select_drive(int drive) {
    ata_select_bus_drive(drive / 2, drive % 2);
}

void ata_rw(int drive, uint64_t start, uint16_t count, uint16_t* buffer, bool write) {
    uint8_t bus = drive / 2;

    while (ata_lock[bus]);
    ata_lock[bus] = true;

    ata_select_drive(drive);

    outportb(ports[bus] + ATA_PORT_SECTOR_COUNT, (count >> 8) & 0xFF);
    outportb(ports[bus] + ATA_PORT_LBA_LO, (uint8_t) (start >> 24));
    outportb(ports[bus] + ATA_PORT_LBA_MI, (uint8_t) (start >> 32));
    outportb(ports[bus] + ATA_PORT_LBA_HI, (uint8_t) (start >> 40));

    outportb(ports[bus] + ATA_PORT_SECTOR_COUNT, count & 0xFF);
    outportb(ports[bus] + ATA_PORT_LBA_LO, (uint8_t) (start));
    outportb(ports[bus] + ATA_PORT_LBA_MI, (uint8_t) (start >> 8));
    outportb(ports[bus] + ATA_PORT_LBA_HI, (uint8_t) (start >> 16));

    outportb(ports[bus] + ATA_PORT_COMMAND, write ? 0x34 : 0x24);

    while (!(inportb(ports[bus] + ATA_PORT_STATUS) & 0b1000))
        ;

    if (write) {
        for (int i = 0; i < count * 256; i++)
            outportw(ports[bus] + ATA_PORT_DATA, buffer[i]);

        // Flushing
        outportb(ports[bus] + ATA_PORT_COMMAND, 0xEA);
    }
    else {
        for (int i = 0; i < count * 256; i++)
            buffer[i] = inportw(ports[bus] + ATA_PORT_DATA);
    }
    ata_lock[bus] = false;
}

int ata_scsi_packet(int drive, uint8_t* packet, uint16_t* buffer, int len) {
    uint8_t bus = drive / 2;

    while (ata_lock[bus]);
    ata_lock[bus] = true;

    ata_select_drive(drive);

    outportb(ports[bus] + ATA_PORT_ERROR, 0x00);
    outportb(ports[bus] + ATA_PORT_LBA_MI, len & 0xFF);
    outportb(ports[bus] + ATA_PORT_LBA_HI, len >> 8);

    outportb(ports[bus] + ATA_PORT_COMMAND, 0xA0);

    while (!(inportb(ports[bus] + ATA_PORT_STATUS) & 0b1000))
        ;

    irq_wait[bus] = true;

    uint16_t* packet_w = (uint16_t*) packet;
    for (int i = 0; i < 6; i++)
        outportw(ports[bus] + ATA_PORT_DATA, packet_w[i]);

    while (irq_wait[bus]);

    len /= 2;
    for (int i = 0; i < len; i++)
        buffer[i] = inportw(ports[bus] + ATA_PORT_DATA);

    ata_lock[bus] = false;
    return len;
}

void ata_rw_scsi(int drive, uint64_t start, uint16_t count, uint16_t* buffer, bool write) {
    uint8_t bus = drive / 2;

    while (ata_lock[bus]);
    ata_lock[bus] = true;

    uint16_t max_size = count * 2048;

    ata_select_drive(drive);

    outportb(ports[bus] + ATA_PORT_ERROR, 0x00);
    outportb(ports[bus] + ATA_PORT_LBA_MI, max_size & 0xFF);
    outportb(ports[bus] + ATA_PORT_LBA_HI, max_size >> 8);

    outportb(ports[bus] + ATA_PORT_COMMAND, 0xA0);

    while (!(inportb(ports[bus] + ATA_PORT_STATUS) & 0b1000));

    uint8_t packet[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    packet[0] = write ? 0xAA : 0xA8;

    packet[9] = count; // Sectors
    packet[2] = (start >> 24) & 0xFF; // LBA bong in big endian cuz scsi = stupid network byte order protocol
    packet[3] = (start >> 16) & 0xFF;
    packet[4] = (start >> 8)  & 0xFF;
    packet[5] = (start >> 0)  & 0xFF;

    irq_wait[bus] = true;

    uint16_t* packet_w = (uint16_t*) packet;
    for (int i = 0; i < 6; i++)
        outportw(ports[bus] + ATA_PORT_DATA, packet_w[i]);

    while (irq_wait[bus]);

    if (write) {
        for (int i = 0; i < count * 1024; i++)
            outportw(ports[bus] + ATA_PORT_DATA, buffer[i]);
    }
    else {
        for (int i = 0; i < count * 1024; i++)
            buffer[i] = inportw(ports[bus] + ATA_PORT_DATA);
    }
    ata_lock[bus] = false;
}

int ata_init_dev(int device, uint16_t* identify) {
    struct ata_device* dev = &(ata_devices[device]);

    struct dev_block* newblock_dev = kmalloc(sizeof(struct dev_block));
    memset(newblock_dev, 0, sizeof(struct dev_block));

    dev->dev_block = newblock_dev;
    
    char* model = (char*)(&identify[27]);

    dev->name = kmalloc(16);

    dev_name_inc("hd@", dev->name);

    dev_block_t* block_dev = dev->dev_block;

    block_dev->internal_id = device;
    if (dev->atapi) {
        uint8_t packet[12] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        uint32_t buffer[2] = { 0, 0 };

        int ret = ata_scsi_packet(device, packet, (uint16_t*) (&buffer), 8);
        if (ret == 0)
            return -1;

        buffer[0] = uint32_bswap(buffer[0]);
        buffer[1] = uint32_bswap(buffer[1]);

        block_dev->block_ops = &ata_scsi_block_ops;
        block_dev->sector_size = 2048;

        block_dev->total_sectors = buffer[0];

        if (buffer[1] != 2048)
            return -1;
    }
    else {
        block_dev->block_ops = &ata_block_ops;
        block_dev->sector_size = 512;

        block_dev->flags |= DISK_PARTITIONABLE;
        block_dev->total_sectors = ((uint64_t*) (&identify[100]))[0];
    }

    int i = 0;
    while (i < 40) {
        block_dev->model_name[i]     = model[i + 1];
        block_dev->model_name[i + 1] = model[i];
        i += 2;
    }
    block_dev->model_name[i] = '\0';

    printk("/dev/%s: Model: %s\n", dev->name, block_dev->model_name);
    printk("/dev/%s: %i sectors\n", dev->name, block_dev->total_sectors);

    int reg_result = dev_register_block(dev->name, block_dev);
    if (reg_result < 0) {
        printk(PRINTK_WARN "/dev/%s: Registration failed\n", dev->name);
        return -1;
    }
    block_dev->max_sectors_at_once = 16;

    fs_enum_partitions(reg_result);
    return 0;
}

int ata_block_init(int drive) {
    printk("ide: Initting %i\n", drive);
    return 0;
}

int ata_block_read(int drive, uint64_t sector, uint16_t count, uint8_t* buffer) {
    ata_rw(drive, sector, count, (uint16_t*) buffer, false);
    return 0;
}

int ata_block_read_scsi(int drive, uint64_t sector, uint16_t count, uint8_t* buffer) {
    ata_rw_scsi(drive, sector, count, (uint16_t*) buffer, false);
    return 0;
}

int ata_block_write(int drive, uint64_t sector, uint16_t count, uint8_t* buffer) {
    ata_rw(drive, sector, count, (uint16_t*) buffer, true);
    return 0;
}

int ata_block_write_scsi(int drive, uint64_t sector, uint16_t count, uint8_t* buffer) {
    ata_rw_scsi(drive, sector, count, (uint16_t*) buffer, true);
    return 0;
}

int ata_block_release(int drive) {
    printk("ide: Releasing %i\n", drive);
    return 0;
}

void ata_init() {
    uint8_t byte;
    int8_t  device = -1;
    uint16_t* buffer = kmalloc(512);

    ata_lock[0] = false;
    ata_lock[1] = false;

    printk(PRINTK_INIT "ide: Initializing\n");

    irq_install(14, ata_pri_irq);
    irq_install(15, ata_sec_irq);
    
    ata_block_ops.init    = ata_block_init;
    ata_block_ops.read    = ata_block_read;
    ata_block_ops.write   = ata_block_write;
    ata_block_ops.release = ata_block_release;

    ata_scsi_block_ops.init    = ata_block_init;
    ata_scsi_block_ops.read    = ata_block_read_scsi;
    ata_scsi_block_ops.write   = ata_block_write_scsi;
    ata_scsi_block_ops.release = ata_block_release;

    for (int bus = 0; bus < 2; bus++) {
        for (int drive = 0; drive < 2; drive++) {
            device++;

            ata_select_bus_drive(bus, drive);

            outportb(ports[bus] + ATA_PORT_SECTOR_COUNT, 0);
            outportb(ports[bus] + ATA_PORT_SECTOR_NUMBER, 0);
            outportb(ports[bus] + ATA_PORT_CYLINDER_LO, 0);
            outportb(ports[bus] + ATA_PORT_CYLINDER_HI, 0);

            outportb(ports[bus] + ATA_PORT_COMMAND, 0xEC);

            byte = inportb(ports[bus] + ATA_PORT_STATUS);

            if (byte == 0) {
                //printk("/dev/%s: Not present\n", names[device]);
                continue;
            }

            memset(buffer, 0, 512);

            while (inportb(ports[bus] + ATA_PORT_STATUS) & 0x80);

            if ((inportb(ports[bus] + ATA_PORT_LBA_MI) != 0) && (inportb(ports[bus] + ATA_PORT_LBA_HI) != 0)) {
                printk("ide: %i: Found ATAPI\n", device);
                ata_devices[device].atapi = true;

                outportb(ports[bus] + ATA_PORT_SECTOR_COUNT, 0);
                outportb(ports[bus] + ATA_PORT_SECTOR_NUMBER, 0);
                outportb(ports[bus] + ATA_PORT_CYLINDER_LO, 0);
                outportb(ports[bus] + ATA_PORT_CYLINDER_HI, 0);

                outportb(ports[bus] + ATA_PORT_COMMAND, 0xA1);
                    
                for (int i = 0; i < 256; i++)
                    buffer[i] = inportw(ports[bus] + ATA_PORT_DATA);

                int ret = ata_init_dev(device, buffer);
                IF_ERROR(ret) {
                    printk(PRINTK_WARN "ide: Device %i is autistic\n", device);
                    continue;
                }
                continue;
            }
            else {
                printk("ide: %i: Found\n", device);

                while ((inportb(ports[bus] + ATA_PORT_STATUS) & 0x1) && (inportb(ports[bus] + ATA_PORT_STATUS) & 0x8));
            }

            for (int i = 0; i < 256; i++)
                buffer[i] = inportw(ports[bus] + ATA_PORT_DATA);

            if (buffer[60] | (buffer[61] << 16))
                flags[drive] |= ATA_FLAG_LBA28;

            if (buffer[83] & (1 << 10))
                flags[drive] |= ATA_FLAG_LBA48;

            if ((flags[drive] & ATA_FLAG_LBA28) | (flags[drive] & ATA_FLAG_LBA48))
                flags[drive] |= ATA_FLAG_LBA;

            int ret = ata_init_dev(device, buffer);
            IF_ERROR(ret) {
                printk(PRINTK_WARN "ide: Device %i is autistic\n", device);
                continue;
            }


            /*if (flags[drive] & ATA_FLAG_LBA28)
                printk("lba28\n");
            if (flags[drive] & ATA_FLAG_LBA48)
                printk("lba48\n");*/
        }
    }
    uint8_t* bufferxx = kmalloc(2048);

    printk("\n");
    ata_rw_scsi(2, 0, 1, (uint16_t*) bufferxx, false);

    printk("%X %X %X %X\n", bufferxx[0], bufferxx[1], bufferxx[2], bufferxx[3]);

    sleep(2000);
}