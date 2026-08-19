//
// Created by user on 8/18/2026.
//

#include "panic.h"

#include "system/console/console.h"


/**
 * @brief Report a panic message and halt execution.
 * @param str Message to report, or NULL.
 * @note Disables interrupts, writes to the console, and then waits indefinitely.
 */
_Noreturn void panic(const char *str)
{
    __disable_irq();

    console_puts("PANIC:\n");
    if (str != NULL) {
        console_puts(str);
    } else {
        console_puts("(no message)");
    }
    console_puts("\n");

    /* Do not trigger an unhandled debug exception on production hardware. */
    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U) {
        __BKPT(0);
    }

    for (;;) {
        __DSB();
        __WFI();
    }
}
