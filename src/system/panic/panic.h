#ifndef STM32G474RE_SYSTEM_PANIC_H
#define STM32G474RE_SYSTEM_PANIC_H

/**
 * @file panic.h
 * @brief Fatal-error reporting and Cortex-M fault handlers.
 *
 * panic() masks interrupts, prints a message on the console, breaks into an
 * attached debugger, and then either halts or resets:
 *
 * - By default the CPU halts in a low-power loop. If the independent
 *   watchdog is running it will reset the device.
 * - Build with `-D PANIC_RESET_ON_FAULT=1` to call NVIC_SystemReset()
 *   instead when no debugger is attached (recommended for deployed
 *   firmware).
 *
 * This module also defines HardFault_Handler, MemManage_Handler,
 * BusFault_Handler and UsageFault_Handler, which print the fault status
 * registers and the faulting PC before halting. Build with
 * `-D PANIC_NO_FAULT_HANDLERS=1` to provide your own handlers instead.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PANIC_RESET_ON_FAULT
#define PANIC_RESET_ON_FAULT 0
#endif

#ifndef PANIC_NO_FAULT_HANDLERS
#define PANIC_NO_FAULT_HANDLERS 0
#endif

/**
 * @brief Report a fatal error and stop.
 * @param msg Message to print, or NULL.
 */
_Noreturn void panic(const char *msg);

/**
 * @brief Report a fatal error with its source location and stop.
 * @param msg Message to print, or NULL.
 * @param file Source file name, or NULL.
 * @param line Source line number.
 */
_Noreturn void panic_at(const char *msg, const char *file, int line);

/**
 * @brief Enable the MemManage, BusFault and UsageFault exceptions and
 *        trapping of integer division by zero.
 * @note Without this, those faults escalate to HardFault. Call it early in
 *       main(); the dedicated handlers then report a more precise cause.
 */
void panic_enable_fault_exceptions(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_SYSTEM_PANIC_H */
