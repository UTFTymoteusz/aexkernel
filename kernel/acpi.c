#include "aex/irq.h"
#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/string.h"
#include "aex/sys.h"

#include "aex/dev/cpu.h"

#include "mem/page.h"

#include "lai/core.h"
#include "lai/helpers/pm.h"
#include "lai/helpers/sci.h"

#include <stdint.h>
#include <stddef.h>

#include "acpi_tables.h"
#include "acpi.h"

struct acpi_location {
    void* addr;
    struct acpi_location* next;
};
typedef struct acpi_location acpi_location_t;

acpi_location_t table_list;

bool acpi_validate_bare(void* ptr, int len) {
    char* cptr = (char*) ptr;
    uint8_t sum = 0;

    for (int i = 0; i < len; i++)
        sum += cptr[i];

    return sum == 0;
}

bool acpi_validate(acpi_sdt_header_t* hdr) {
    char* cptr = (char*) hdr;
    uint8_t sum = 0;

    for (uint32_t i = 0; i < hdr->length; i++)
        sum += cptr[i];

    return sum == 0;
}

void* acpi_map_sdt(size_t phys) {
    void* phys_aligned = (void*) (phys & ~0xFFF);
    void* virt = kpmap(kptopg(sizeof(acpi_sdt_header_t) + CPU_PAGE_SIZE), phys_aligned, NULL, 0x03);

    acpi_sdt_header_t* hdr = virt + (phys & 0xFFF);
    int len = hdr->length;
    kpunmap(virt, kptopg(sizeof(acpi_sdt_header_t) + CPU_PAGE_SIZE), NULL);

    return kpmap(kptopg(len + CPU_PAGE_SIZE), (void*) phys_aligned, NULL, 0x03) + (phys & 0xFFF);
}

void* look_for_rsdt(size_t* phys) {
    void* bda = kpmap(kptopg(4096), (void*) 0x0000, NULL, 0x03);
    size_t ebda_segment = *((uint16_t*) (bda + 0x40E));

    kpunmap(bda, kptopg(4096), NULL);

    void* ebda_pages = kpmap(kptopg(262144 + CPU_PAGE_SIZE * 4), (void*) ((ebda_segment * 0x10) & 0xFFFFF000), NULL, 0x03);
    uint16_t ebda_offset = (ebda_segment * 0x10) & 0x0FFF;

    void* ebda = ebda_pages + ebda_offset;

    struct rsd_ptr* rsd = NULL;

    for (int i = 0; i < 16384; i++)
        if (memcmp(ebda + i * 16, "RSD PTR ", 8) == 0) {
            rsd = (struct rsd_ptr*) (ebda + i * 16);
            break;
        }

    void* idk = NULL;
    if (rsd == NULL) {
        idk = kpmap(kptopg(0x000FFFFF - 0x000E0000), (void*) 0x000E0000, NULL, 0x03);

        for (int i = 0; i < 8192; i++)
            if (memcmp(idk + i * 16, "RSD PTR ", 8) == 0) {
                rsd = (struct rsd_ptr*) (idk + i * 16);
                break;
            }
    }

    if (rsd == NULL) {
        printk("acpi: Apparently there's no ACPI on this system\n");
        return NULL;
    }
    printk("acpi: Revision 0x%02X\n", rsd->revision);

    if (!acpi_validate_bare((void*) rsd, 20)) {
        printk(PRINTK_WARN "acpi: RSDP doesn't have a valid checksum\n");
        return NULL;
    }
    *phys = rsd->rsdt_phys_addr;
    void* rsdt = acpi_map_sdt(rsd->rsdt_phys_addr);

    kpunmap(ebda_pages, kptopg(262144 + CPU_PAGE_SIZE * 4), NULL);

    if (idk != NULL)
        kpunmap(idk, kptopg(0x000FFFFF - 0x000E0000), NULL);

    if (!acpi_validate(rsdt)) {
        printk(PRINTK_WARN "acpi: RSDT doesn't have a valid checksum\n");
        return NULL;
    }
    return rsdt;
}

void acpi_add_table(void* table) {
    if (table_list.addr == NULL) {
        table_list.addr = table;
        return;
    }
    acpi_location_t* current = &table_list;

    while (current->next != NULL)
        current = current->next;

    current->next = kmalloc(sizeof(acpi_location_t));
    memset(current->next, 0, sizeof(acpi_location_t));

    current->next->addr = table;
}

void* acpi_find_table(char signature[4], int index) {
    acpi_location_t* current = &table_list;

    while (current != NULL) {
        if (memcmp(signature, ((acpi_sdt_header_t*) current->addr)->signature, 4) == 0 && (index-- == 0))
            return current->addr;

        current = current->next;
    }
    //printk("acpi: Warning: Table %c%c%c%c not found\n", signature[0], signature[1], signature[2], signature[3]);
    return NULL;
}

void understand_fadt(void* fadt_addr) {
    struct acpi_fadt* fadt = (struct acpi_fadt*) fadt_addr;

    void* dsdt = acpi_map_sdt(fadt->dsdt);
    if (!acpi_validate(dsdt)) {
        printk("acpi: DSDT doesn't have a valid checksum\n");
        dsdt = NULL;
    }
    else
        acpi_add_table(dsdt);
}

void acpi_sci_handler() {
    uint16_t event = lai_get_sci_event();
    printk("acpi: Event 0b%b\n", event);

    if (event & ACPI_POWER_BUTTON) {
        static bool handling = false;
        if (handling) {
            printk("acpi: Cease pressing the power button, let me shutdown properly!\n");
            return;
        }
        handling = true;
        shutdown();
    }
}

void acpi_shutdown() {
    lai_enter_sleep(5);
}

void acpi_init() {
    printk(PRINTK_INIT "acpi: Initializing\n");

    size_t phys = 0;
    struct acpi_rsdt* rsdt = look_for_rsdt(&phys);
    if (rsdt == NULL) {
        printk(PRINTK_WARN "acpi: Failed\n");
        return;
    }
    int table_count = (rsdt->hdr.length - sizeof(acpi_sdt_header_t)) / 4;

    memset(&table_list, 0, sizeof(table_list));

    for (int i = 0; i < table_count; i++) {
        void* table = acpi_map_sdt(rsdt->other_sdt[i]);

        if (!acpi_validate(table)) {
            printk(PRINTK_WARN "acpi: Table %i doesn't have a valid checksum\n", i);
            table = NULL;
        }
        if (table == NULL)
            continue;

        acpi_add_table(table);
    }

    if (acpi_find_table("PSDT", 0) != NULL) {
        printk(PRINTK_WARN "acpi: System has a PSDT table, disabling ACPI to be safe\n");
        printk(PRINTK_WARN "acpi: Failed\n");
        return;
    }

    struct acpi_fadt* fadt = acpi_find_table("FACP", 0);
    if (fadt == NULL) {
        printk(PRINTK_WARN "acpi: System has no FACP table? What the hell\n");
        printk(PRINTK_WARN "acpi: Failed\n");
        return;
    }
    understand_fadt(fadt);

    lai_set_acpi_revision(rsdt->hdr.revision);
    lai_create_namespace();

    irq_install(fadt->sci_interrupt, acpi_sci_handler);
    
    printk("acpi: SCI @ %i\n", fadt->sci_interrupt);
    printk("acpi: Asking LAI to enable ACPI\n");

    lai_enable_acpi(0);
    lai_set_sci_event(ACPI_POWER_BUTTON);

    printk(PRINTK_OK "acpi: Enabled\n");
    register_shutdown(acpi_shutdown);
}