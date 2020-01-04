#pragma once

#define OS_NAME "AEX"
#define OS_VERSION "0.11"

#define DEFAULT_COLOR 97
#define HIGHLIGHT_COLOR 93

void init_print_header();
void init_print_osinfo();

void dev_init();
void input_init();
void pci_init();

void fs_init();

void proc_init();
void proc_initsys();
void task_init();