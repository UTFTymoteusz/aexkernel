#pragma once

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
    volatile uint64_t clb;
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

    volatile struct ahci_hba_port_struct ports[];
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

    volatile uint64_t ctba;

    volatile uint32_t rsv1[4];
} __attribute((packed));

struct ahci_prdt_entry {
    volatile uint64_t dba;
    volatile uint32_t rsv0;
 
    volatile uint32_t dbc  : 22;
    volatile uint32_t rsv1 :  9;
    volatile uint32_t i    :  1;
} __attribute((packed));

struct ahci_command_table {
    volatile uint8_t cfis[64];
    volatile uint8_t acmd[16];
    volatile uint8_t rsvd[48];

    volatile struct ahci_prdt_entry prdt[1];
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