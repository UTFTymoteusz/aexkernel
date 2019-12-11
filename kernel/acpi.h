#pragma once

void acpi_init();

void* acpi_find_table(char signature[4], int index);