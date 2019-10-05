#pragma once

#include "aex/time.h"

#include "dev/cpu.h"
#include "dev/disk.h"

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

#define ATA_CMD_IDENTIFY 0xEC

int selected[2];
const int ports[2] = {0x1F0, 0x170};

const char* names[4] = {"hda", "hdb", "hdc", "hdd"};

uint16_t* buffers[4][256];

uint16_t flags[4];

bool ata_lock;

void ata_select_bus_drive(int bus, int drive) {
    if (selected[bus] == drive)
        return;

    for (int i = 0; i < 5; i++)
        outportb(ports[bus] + ATA_PORT_DRIVE_SELECT, (drive == 0 ? 0xA0 : 0xB0) | ((flags[bus * 2 + drive] & ATA_FLAG_LBA) ? 0b01000000 : 0));

    selected[bus] = drive;
}

void ata_select_drive(int drive) {
    ata_select_bus_drive(drive / 2, drive % 2);
}

void ata_write_sector(int drive, uint64_t sector, uint16_t* data) {
    while (ata_lock)
        yield();

    ata_lock = true;

    uint8_t bus = drive / 2;

    ata_select_drive(drive);

    outportb(ports[bus] + ATA_PORT_SECTOR_COUNT, 0);
    outportb(ports[bus] + ATA_PORT_LBA_LO, (uint8_t)(sector >> 24));
    outportb(ports[bus] + ATA_PORT_LBA_MI, (uint8_t)(sector >> 32));
    outportb(ports[bus] + ATA_PORT_LBA_HI, (uint8_t)(sector >> 40));

    outportb(ports[bus] + ATA_PORT_SECTOR_COUNT, 1);
    outportb(ports[bus] + ATA_PORT_LBA_LO, (uint8_t)(sector));
    outportb(ports[bus] + ATA_PORT_LBA_MI, (uint8_t)(sector >> 8));
    outportb(ports[bus] + ATA_PORT_LBA_HI, (uint8_t)(sector >> 16));

    outportb(ports[bus] + ATA_PORT_COMMAND, 0x34);

    while (!(inportb(ports[bus] + ATA_PORT_STATUS) & 0b1000))
        yield();

    for (int i = 0; i < 256; i++)
        outportw(ports[bus] + ATA_PORT_DATA, data[i]);

    // Flushing
    outportb(ports[bus] + ATA_PORT_COMMAND, 0xEA);

    printf("%s: Wrote sector\n", names[drive]);

    ata_lock = false;
}

void ata_init() {
    uint8_t byte;
    int8_t  device = -1;
    uint16_t* buffer;

    printf("Detecting and initializing ATA devices\n");

    for (int bus = 0; bus < 2; bus++)
        for (int drive = 0; drive < 2; drive++) {
            device++;

            ata_select_bus_drive(bus, drive);

            outportb(ports[bus] + ATA_PORT_SECTOR_COUNT, 0);
            outportb(ports[bus] + ATA_PORT_SECTOR_NUMBER, 0);
            outportb(ports[bus] + ATA_PORT_CYLINDER_LO, 0);
            outportb(ports[bus] + ATA_PORT_CYLINDER_HI, 0);

            outportb(ports[bus] + ATA_PORT_COMMAND, ATA_CMD_IDENTIFY);

            byte = inportb(ports[bus] + ATA_PORT_STATUS);

            if (byte == 0) {
                printf("%s: Not present\n", names[device]);
                continue;
            }

            while (inportb(ports[0] + ATA_PORT_STATUS) & 0x80);

            if ((inportb(ports[bus] + ATA_PORT_LBA_MI) != 0) && (inportb(ports[bus] + ATA_PORT_LBA_HI) != 0)) {
                printf("%s: Found ATAPI\n", names[device]);
                continue;
            }
            else {
                printf("%s: Found\n", names[device]);

                while ((inportb(ports[bus] + ATA_PORT_STATUS) & 0x1) && (inportb(ports[bus] + ATA_PORT_STATUS) & 0x8));
            }

            buffer = (uint16_t*)buffers[drive];

            for (int i = 0; i < 256; i++)
                buffer[i] = inportw(ports[bus] + ATA_PORT_DATA);

            if (buffer[60] | (buffer[61] << 16))
                flags[drive] |= ATA_FLAG_LBA28;

            if (buffer[83] & (1 << 10))
                flags[drive] |= ATA_FLAG_LBA48;

            if ((flags[drive] & ATA_FLAG_LBA28) | (flags[drive] & ATA_FLAG_LBA48))
                flags[drive] |= ATA_FLAG_LBA;

            /*if (flags[drive] & ATA_FLAG_LBA28)
                printf("lba28\n");
            if (flags[drive] & ATA_FLAG_LBA48)
                printf("lba48\n");*/
        }

    printf("\n");
    ata_write_sector(0, 0, (uint16_t*)"bigboi biggurl 4 bigboi biggurl bigboi biggurl bigboi biggurl bigboi biggurl bigboi biggurl bigboi biggurl bigboi biggurl ");
}