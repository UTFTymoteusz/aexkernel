#include "aex/proc/exec.h"

#include "ps2.h"
#include "pcuart/pcuart.h"
#include "exec_elf.h"

#include "dev/arch.h"

void arch_init() {
    ps2_init();
    pcuart_init();
    
    exec_register_executor("elf_amd64", elf_load);
}