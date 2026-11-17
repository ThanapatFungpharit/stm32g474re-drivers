#include "panic.h"

#include "stm32g474xx.h"
#include "system/console/console.h"


static bool debugger_attached(void)
{
    return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U;
}

/**
 * @brief Stop after a fatal error has been reported.
 */
static _Noreturn void halt(void)
{
    if (debugger_attached()) {
        __BKPT(0);
    }

#if PANIC_RESET_ON_FAULT
    if (!debugger_attached()) {
        NVIC_SystemReset();
    }
#endif

    for (;;) {
        __DSB();
        __WFI();
    }
}

_Noreturn void panic_at(const char *msg, const char *file, const int line)
{
    __disable_irq();

    console_puts("\r\nPANIC: ");
    console_puts((msg != NULL) ? msg : "(no message)");
    if (file != NULL) {
        console_puts(" @ ");
        console_puts(file);
        console_putc(':');
        console_put_dec(line);
    }
    console_puts("\r\n");

    halt();
}

_Noreturn void panic(const char *msg)
{
    panic_at(msg, NULL, 0);
}

void panic_enable_fault_exceptions(void)
{
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk |
                  SCB_SHCSR_BUSFAULTENA_Msk |
                  SCB_SHCSR_USGFAULTENA_Msk;
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    __DSB();
    __ISB();
}


#if !PANIC_NO_FAULT_HANDLERS

static void put_reg(const char *name, const uint32_t value)
{
    console_puts(name);
    console_puts("=0x");
    console_put_hex(value, 8U);
    console_puts("\r\n");
}

/**
 * @brief Print the fault context and halt.
 * @param frame Exception stack frame (r0-r3, r12, lr, pc, xpsr).
 * @param exc_return EXC_RETURN value from LR on exception entry.
 * @note Called from the naked handlers below; must not return.
 */
_Noreturn void panic_fault_report(const uint32_t *frame, uint32_t exc_return);

_Noreturn void panic_fault_report(const uint32_t *frame, const uint32_t exc_return)
{
    static const char *const names[] = {
        "?", "?", "?", "HardFault", "MemManage", "BusFault", "UsageFault"
    };
    const uint32_t vect = SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk;

    __disable_irq();

    console_puts("\r\nPANIC: ");
    console_puts((vect < (sizeof names / sizeof names[0])) ? names[vect] : "fault");
    console_puts("\r\n");
    put_reg("PC",    frame[6]);
    put_reg("LR",    frame[5]);
    put_reg("xPSR",  frame[7]);
    put_reg("EXC_RETURN", exc_return);
    put_reg("CFSR",  SCB->CFSR);
    put_reg("HFSR",  SCB->HFSR);
    if ((SCB->CFSR & SCB_CFSR_MMARVALID_Msk) != 0U) {
        put_reg("MMFAR", SCB->MMFAR);
    }
    if ((SCB->CFSR & SCB_CFSR_BFARVALID_Msk) != 0U) {
        put_reg("BFAR",  SCB->BFAR);
    }

    halt();
}

/* The handlers select the stack that was in use when the fault happened
 * (bit 2 of EXC_RETURN) and pass the stacked frame to the C reporter. */
#define PANIC_FAULT_HANDLER(name)                     \
    __attribute__((naked)) void name(void)            \
    {                                                 \
        __asm volatile(                               \
            "tst   lr, #4              \n"            \
            "ite   eq                  \n"            \
            "mrseq r0, msp             \n"            \
            "mrsne r0, psp             \n"            \
            "mov   r1, lr              \n"            \
            "b     panic_fault_report  \n");          \
    }

void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);

PANIC_FAULT_HANDLER(HardFault_Handler)
PANIC_FAULT_HANDLER(MemManage_Handler)
PANIC_FAULT_HANDLER(BusFault_Handler)
PANIC_FAULT_HANDLER(UsageFault_Handler)

#endif /* !PANIC_NO_FAULT_HANDLERS */
