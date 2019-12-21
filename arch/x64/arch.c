#include "ps2.h"
#include "pcuart/pcuart.h"

#include "dev/arch.h"

void arch_init() {
    ps2_init();
    pcuart_init();
}