#include <stdint.h>

struct rsd_ptr {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_phys_addr;

    uint32_t length;
    uint64_t xsdt_phys_addr;
    uint8_t xsdt_checksum;
    uint8_t reserved[8];
} __attribute((packed));

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oemtableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute((packed));
typedef struct acpi_sdt_header acpi_sdt_header_t;

struct acpi_rsdt {
    acpi_sdt_header_t hdr;
    uint32_t other_sdt[];
} __attribute((packed));

struct acpi_fadt {
    acpi_sdt_header_t hdr;
    uint32_t firmware_ctrl;
    uint32_t dsdt;

    uint8_t reserved;

    uint8_t preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;

    uint8_t acpi_enable;
    uint8_t acpi_disable;

    // add the rest in later
};

struct acpi_dsdt {
    acpi_sdt_header_t hdr;
    uint8_t data[];
};
