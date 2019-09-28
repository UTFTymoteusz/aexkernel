#pragma once

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

enum {
    AHCI_FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
    AHCI_FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
    AHCI_FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
    AHCI_FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
    AHCI_FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
    AHCI_FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
    AHCI_FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
    AHCI_FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
};

struct ahci_fis_reg_h2d {
    // DWORD 0
    volatile uint8_t  fis_type;	// FIS_TYPE_REG_H2D
 
    volatile uint8_t  pmport : 4;	// Port multiplier
    volatile uint8_t  rsv0 : 3;		// Reserved
    volatile uint8_t  c : 1;		// 1: Command, 0: Control
 
    volatile uint8_t  command;	// Command register
    volatile uint8_t  featurel;	// Feature register, 7:0
 
    // DWORD 1
    volatile uint8_t  lba0;		// LBA low register, 7:0
    volatile uint8_t  lba1;		// LBA mid register, 15:8
    volatile uint8_t  lba2;		// LBA high register, 23:16
    volatile uint8_t  device;		// Device register
 
    // DWORD 2
    volatile uint8_t  lba3;		// LBA register, 31:24
    volatile uint8_t  lba4;		// LBA register, 39:32
    volatile uint8_t  lba5;		// LBA register, 47:40
    volatile uint8_t  featureh;	// Feature register, 15:8
 
    // DWORD 3
    volatile uint8_t  countl;		// Count register, 7:0
    volatile uint8_t  counth;		// Count register, 15:8
    volatile uint8_t  icc;		// Isochronous command completion
    volatile uint8_t  control;	// Control register
 
    // DWORD 4
    volatile uint8_t  rsv1[4];	// Reserved
} __attribute((packed));

struct ahci_fis_reg_d2h {
    // DWORD 0
    volatile uint8_t  fis_type;	// FIS_TYPE_REG_H2D
 
    volatile uint8_t  pmport : 4;	// Port multiplier
    volatile uint8_t  rsv0 : 3;		// Reserved
    volatile uint8_t  c : 1;		// 1: Command, 0: Control
 
    volatile uint8_t  command;	// Command register
    volatile uint8_t  featurel;	// Feature register, 7:0
 
    // DWORD 1
    volatile uint8_t  lba0;		// LBA low register, 7:0
    volatile uint8_t  lba1;		// LBA mid register, 15:8
    volatile uint8_t  lba2;		// LBA high register, 23:16
    volatile uint8_t  device;		// Device register
 
    // DWORD 2
    volatile uint8_t  lba3;		// LBA register, 31:24
    volatile uint8_t  lba4;		// LBA register, 39:32
    volatile uint8_t  lba5;		// LBA register, 47:40
    volatile uint8_t  featureh;	// Feature register, 15:8
 
    // DWORD 3
    volatile uint8_t  countl;		// Count register, 7:0
    volatile uint8_t  counth;		// Count register, 15:8
    volatile uint8_t  icc;		// Isochronous command completion
    volatile uint8_t  control;	// Control register
 
    // DWORD 4
    volatile uint8_t  rsv1[4];	// Reserved
} __attribute((packed));

struct ahci_fis_pio_setup {
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_PIO_SETUP
 
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		// Data transfer direction, 1 - device to host
	uint8_t  i:1;		// Interrupt bit
	uint8_t  rsv1:1;
 
	uint8_t  status;		// Status register
	uint8_t  error;		// Error register
 
	// DWORD 1
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register
 
	// DWORD 2
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  rsv2;		// Reserved
 
	// DWORD 3
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  rsv3;		// Reserved
	uint8_t  e_status;	// New value of status register
 
	// DWORD 4
	uint16_t tc;		// Transfer count
	uint8_t  rsv4[2];	// Reserved
} __attribute((packed));

struct ahci_fis_dma_setup {
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_DMA_SETUP
 
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		// Data transfer direction, 1 - device to host
	uint8_t  i:1;		// Interrupt bit
	uint8_t  a:1;            // Auto-activate. Specifies if DMA Activate FIS is needed
 
    uint8_t  rsved[2];       // Reserved

    //DWORD 1&2

    uint64_t DMAbufferID;    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory. SATA Spec says host specific and not in Spec. Trying AHCI spec might work.

    //DWORD 3
    uint32_t rsvd;           //More reserved

    //DWORD 4
    uint32_t DMAbufOffset;   //Byte offset into buffer. First 2 bits must be 0

    //DWORD 5
    uint32_t TransferCount;  //Number of bytes to transfer. Bit 0 must be 0

    //DWORD 6
    uint32_t resvd;          //Reserved

} __attribute((packed));


struct ahci_hba_port_struct {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;

    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t rsv0;

    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;

    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;

    volatile uint32_t rsv1[11];
    volatile uint32_t vendor[4];
} __attribute((packed));

struct ahci_hba_struct {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;

    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;

    volatile uint32_t en_loc;
    volatile uint32_t en_ctl;

    volatile uint32_t cap2;
    volatile uint32_t bohc;

    volatile uint8_t rsv[0xA0 - 0x2C];

    volatile uint8_t vendor[0x100 - 0xA0];

    volatile struct ahci_hba_port_struct ports[32];
} __attribute((packed));

struct ahci_command_header {
    volatile uint8_t cfl : 5;
    volatile uint8_t a : 1;
    volatile uint8_t w : 1;
    volatile uint8_t p : 1;

    volatile uint8_t r : 1;
    volatile uint8_t b : 1;
    volatile uint8_t c : 1;
    volatile uint8_t rsv0 : 1;
    volatile uint8_t pmp  : 4;

    volatile uint16_t prdtl;

    volatile uint32_t prdbc;

    volatile uint32_t ctba;
    volatile uint32_t ctbau;

    volatile uint32_t rsv1[4];
} __attribute((packed));

struct ahci_prdt_entry {
    volatile uint32_t dba;
    volatile uint32_t dbau;
    volatile uint32_t rsv0;
 
    volatile uint32_t dbc  : 22;
    volatile uint32_t rsv1 :  9;
    volatile uint32_t i    :  1;
} __attribute((packed));

struct ahci_command_table {
    volatile uint8_t cfis[64];
    volatile uint8_t acmd[16];
    volatile uint8_t rsvd[48];

    volatile struct ahci_prdt_entry prdt[2];
} __attribute((packed));

struct ahci_rxfis {
    struct ahci_fis_dma_setup dsfis;
    uint8_t pad0[4];
 
    struct ahci_fis_pio_setup psfis;
    uint8_t pad1[12];
 
    struct ahci_fis_reg_d2h	rfis;
    uint8_t pad2[4];
 
    uint8_t sdbfis[8];
 
    uint8_t ufis[64];
    uint8_t rsv[0x100-0xA0];
} __attribute((packed));

struct ahci_dev {
    uint8_t type;
    
    struct ahci_command_header* command_headers; // Command list structure pointer
    struct ahci_command_table*  command_tables;

    volatile void* tx_fis[32]; // Sending FIS structure virtual addr pointers
    volatile struct ahci_rxfis* rx_fis;     // Receiving FIS structure virtual addr pointer

    volatile void* dma;
    
    struct ahci_hba_port_struct* port;
    //void* data;
};

volatile struct ahci_hba_struct* ahci_hba;

pci_entry_t* ahci_controller;
pci_address_t ahci_address;
volatile void* ahci_bar;

struct ahci_dev ahci_devs[32];
uint8_t ahci_dev_amount;
uint8_t ahci_max_commands;

void* ahci_common;


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
        default:
            return AHCI_DEV_SATA;
    }
}
int ahci_find_free_slot(struct ahci_hba_port_struct* port) {

    uint32_t busy = port->sact | port->ci;

    for (int i = 0; i < ahci_max_commands; i++)
        if (!(busy & (1 << i)))
            return i;

    printf("No free slot\n");
    return -1;
}

void ahci_count_devs() {

    uint32_t pi = ahci_hba->pi;
    
    for (int i = 0; i < 32; i++) {

        if (!(pi & (1 << i)))
            continue;

        if ((i + 1) > ahci_dev_amount)
            ahci_dev_amount = i + 1;
    }
}

void ahci_start_cmd(struct ahci_hba_port_struct* port) {
    
    port->cmd &= ~HBA_CMD_ST;

    while (port->cmd & HBA_CMD_CR);

    port->cmd |= HBA_CMD_FRE;
    port->cmd |= HBA_CMD_ST;
}
void ahci_stop_cmd(struct ahci_hba_port_struct* port) {
    
    port->cmd &= ~HBA_CMD_ST;
    while (port->cmd & HBA_CMD_CR);

    port->cmd &= ~HBA_CMD_FRE;
    while (port->cmd & HBA_CMD_FR);
}

void ahci_init_dev(int dev) {
    write_debug("Initializing %s\n", dev, 10);

    struct ahci_hba_port_struct* port = ahci_devs[dev].port;

    port->cmd &= ~HBA_CMD_ST;
    port->cmd &= ~HBA_CMD_FRE;
 
    while (true) {

        if (port->cmd & HBA_CMD_FR)
            continue;
        if (port->cmd & HBA_CMD_CR)
            continue;

        break;
    }

    port->cmd &= 0xFFFF7FFF; // Bit 15 - CR
    port->cmd &= 0xFFFFBFFF; // Bit 14 - FR
    port->cmd &= 0xFFFFFFFE; // Bit 0 - ST
    port->cmd &= 0xFFFFFFF7; // Bit 4 - FRE

    port->serr = 1;
    port->ie = 0;
    port->is = 0;

    /*while (port->cmd & HBA_CMD_CR);*/
 
    //port->cmd |= HBA_CMD_FRE;
    //port->cmd |= HBA_CMD_ST; 

    //port->ie = 0xFFFFFFFF;
    //port->is = 0;
}
void ahci_identify(int dev) {

    struct ahci_hba_port_struct* port = ahci_devs[dev].port;
    int slot = ahci_find_free_slot(port);
    write_debug("slot: %s\n", slot, 10);

    struct ahci_fis_reg_h2d* fis = (struct ahci_fis_reg_h2d*)ahci_devs[dev].tx_fis[slot];
    memset((void*)fis, 0, sizeof(struct ahci_fis_reg_h2d));

    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
    fis->command  = 0xEC;
    fis->c        = 1;
    fis->device   = 0;
    fis->pmport   = 0;

    struct ahci_command_header* header = &(ahci_devs[dev].command_headers[slot]);
    header->cfl = sizeof(struct ahci_fis_reg_h2d) / sizeof(uint32_t);
    header->w   = 0;
    header->prdtl = 1;

    write_debug("hdr 0x%s\n", (size_t)header & 0xFFFFFFFFFFFF, 16);
    write_debug("tfis 0x%s\n", (size_t)fis & 0xFFFFFFFFFFFF, 16);

    while (port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ));

    //write_debug("fis 0x%s\n", fis->fis_type, 16);
    
    //write_debug("pio fis %s\n", ahci_devs[dev].rx_fis->psfis.tc, 10);
    //write_debug("dma fis %s\n", ahci_devs[dev].rx_fis->dsfis.DMAbufferID, 10);
    //write_debug("reg fis %s\n", ahci_devs[dev].rx_fis->rfis.lba0, 10);

    ahci_start_cmd(port);
    port->ci |= 1 << slot;
    //port->ie  |= 1 << slot;

    port->is |= 0xFFFFFFFF;
    write_debug("is 0x%s\n", port->is, 16);

    while (true) {

        if ((port->ci & (1 << slot)) == 0) 
            break;

        if (port->is & (1 << 30)) {
            printf("err\n");
            return;
        }
    }
    if (port->is & (1 << 30)) {
        printf("err\n");
        return;
    }
    write_debug("is 0x%s\n", port->is, 16);

    ahci_stop_cmd(port);

    write_debug("Sector Count: 0x%s\n", ((uint16_t*)ahci_devs[dev].dma)[100], 16);
    write_debug("dmaco %s\n", header->prdbc, 10);
    write_debug("prdtl %s\n", header->prdtl, 10);
    //write_debug("pio fis %s\n", ahci_devs[dev].rx_fis->psfis.tc, 10);
    //write_debug("dma fis %s\n", ahci_devs[dev].rx_fis->dsfis.DMAbufferID, 10);
    //write_debug("reg fis %s\n", ahci_devs[dev].rx_fis->rfis.lba0, 10);

    sleep(5000);
}

void ahci_find_devs() {

    uint32_t pi = ahci_hba->pi;
    uint32_t type;
    void* offsetv, * offsetp;

    ahci_dev_amount = 0;

    for (int i = 0; i < 32; i++) {

        if (!(pi & (1 << i)))
            continue;

        type = ahci_probe_port(&(ahci_hba->ports[i]));

        if (type == AHCI_DEV_NULL)
            continue;
        //if (ret == AHCI_DEV_NULL)
        //    stop_cmd(&(ahci_hba->ports[i]));

        /*asm volatile("\
        mov eax, cr0;        \
        or eax,  0 << 30;    \
        and eax, ~(1 << 29); \
        mov cr0, eax;        \
        \
        wbinvd; \
        \
        xor eax, eax;    \
        xor edx, edx;    \
        mov ecx, 0x2FF; \
        wrmsr;  \
        ");*/

        struct ahci_hba_port_struct* port = ahci_devs[i].port;

        //ahci_start_cmd(port);

        //port->cmd &= 0xFFFF7FFF; // Bit 15 - CR
        //port->cmd &= 0xFFFFBFFF; // Bit 14 - FR
        //port->cmd &= 0xFFFFFFFE; // Bit 0 - ST
        //port->cmd &= 0xFFFFFFF7; // Bit 4 - FRE

        //port->serr = 1;
        //port->is = 0;
        //port->ie = 0;

        //printf("before memory shit\n");

        offsetv = ahci_common + (0x8000 * i);
        offsetp = mem_page_get_phys_addr_of(ahci_common, NULL) + (0x8000 * i);
        
        ahci_devs[i].type   = type;
        ahci_devs[i].port   = &(ahci_hba->ports[i]);
        ahci_devs[i].rx_fis = (struct ahci_rxfis*)(((size_t)offsetv) + 0x1000);
        ahci_devs[i].command_headers = (struct ahci_command_header*)((size_t)offsetv);
        ahci_devs[i].command_tables  = (struct ahci_command_table*) ((size_t)offsetv) + 0x2000;

        ahci_devs[i].dma = (void*)(((size_t)offsetv) + 0x6000);

        for (int j = 0; j < 32; j++) {

            ahci_devs[i].command_headers[j].ctba  = (size_t)(((size_t)offsetp) + 0x2000 + (sizeof(struct ahci_command_table) * 0x100));
            ahci_devs[i].command_headers[j].prdtl = 1;
            
            ahci_devs[i].command_tables[j].prdt[0].dba = (size_t)(((size_t)offsetp) + 0x6000);
            ahci_devs[i].command_tables[j].prdt[0].dbc = 511;
            ahci_devs[i].command_tables[j].prdt[0].i = 1;

            ahci_devs[i].tx_fis[j] = (void*)&(ahci_devs[i].command_tables[j]);

            if (j == 0) {
                write_debug("hdr0 0x%s\n", (size_t)ahci_devs[i].command_headers & 0xFFFFFFFFFFFF, 16);
                write_debug("tfis0 0x%s\n", (size_t)ahci_devs[i].tx_fis[0] & 0xFFFFFFFFFFFF, 16);
            }
        }

        ahci_devs[i].rx_fis->dsfis.fis_type = AHCI_FIS_TYPE_DMA_SETUP;
        ahci_devs[i].rx_fis->psfis.fis_type = AHCI_FIS_TYPE_PIO_SETUP;
        ahci_devs[i].rx_fis->rfis.fis_type  = AHCI_FIS_TYPE_REG_D2H;
        ahci_devs[i].rx_fis->sdbfis[0]      = AHCI_FIS_TYPE_DEV_BITS;

        ahci_hba->ports[i].clb = (size_t) ((size_t)offsetp);
        ahci_hba->ports[i].fb  = (size_t)(((size_t)offsetp) + 0x1000);

        //printf("before cr loop\n");
        //while (port->cmd & HBA_CMD_CR);
        //printf("after cr loop\n");

        //port->is = 0;
        //port->ie = 0xFFFFFFFF;

        switch (type) {
            case AHCI_DEV_SATA:
                write_debug("%s: SATA\n", i, 10);
                ahci_init_dev(i);
                ahci_identify(i);
                break;
            case AHCI_DEV_SATAPI:
                write_debug("%s: SATAPI\n", i, 10);
                break;
            case AHCI_DEV_SEMB:
                write_debug("%s: SEMB\n", i, 10);
                break;
            case AHCI_DEV_PM:
                write_debug("%s: PM\n", i, 10);
                break;

            default:
            case AHCI_DEV_NULL:
                write_debug("%s: Not found\n", i, 10);
                break;
        }
    }
}

void ahci_init() {
    printf("Initializing AHCI\n");

    ahci_controller = pci_find_first(0x01, 0x06, 0x01);

    if (ahci_controller == NULL) {
        printf("No AHCI controller found\n\n");
        return;
    }

    ahci_address = ahci_controller->address;

    pci_setup_entry(ahci_controller);
    pci_enable_busmaster(ahci_controller);

    ahci_bar = ahci_controller->bar[5].virtual_addr;
    write_debug("AHCI ABAR V: 0x%s\n", (size_t)ahci_bar & 0xFFFFFFFFFFFF, 16);
    write_debug("AHCI ABAR P: 0x%s\n", (size_t)ahci_controller->bar[5].physical_addr & 0xFFFFFFFFFFFF, 16);

    ahci_max_commands = ((ahci_hba->cap >> 8) & 0b11111) + 1;

    //write_debug("hdr_init: %s\n", (size_t)&(ahci_devs[i].command_headers[j]) & 0xFFFFFFFFFFFF, 16);

    ahci_hba = (struct ahci_hba_struct*)ahci_bar;
    write_debug("slots : %s\n", ahci_max_commands, 10);
    write_debug("pi    : 0b%s\n", ahci_hba->pi, 2);

    //write_debug("preghc 0b%s\n", ahci_hba->ghc, 2);
    ahci_hba->ghc |= (uint32_t)1 << 31; // AE
    ahci_hba->ghc |= (uint32_t)1 << 0;  // HR
    //write_debug("midghc 0b%s\n", ahci_hba->ghc, 2);
    ahci_hba->ghc |= (uint32_t)1 << 31; // AE
    //ahci_hba->ghc |= (uint32_t)1 << 1;  // IE
    //write_debug("aftghc 0b%s\n", ahci_hba->ghc, 2);

    ahci_count_devs();
    ahci_common = mem_page_next_contiguous(ahci_dev_amount * 8, NULL, NULL, 0b11011);
    write_debug("common : 0x%s\n", (size_t)ahci_common & 0xFFFFFFFFFFFFFF, 16);

    memset(ahci_common, 0, ahci_dev_amount * CPU_PAGE_SIZE * 8);
    ahci_find_devs();

    //ahci_identify(0);
    //ahci_write_sector(0, 0, "xdddddddddd");
    //write_debug("boi : 0x%s\n", ((uint8_t*)0x100000)[0], 16);
    //write_debug("boi : 0x%s\n", sizeof(struct ahci_hba_port_struct), 16);

    printf("\n");
}