//
// Created by user on 8/18/2026.
//

#ifndef STM32G474RE_DRIVERS_PANIC_H
#define STM32G474RE_DRIVERS_PANIC_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Report a panic message and halt execution. @param str Message to report, or NULL. */
_Noreturn void panic(const char *str);

#ifdef __cplusplus
}
#endif

#endif //STM32G474RE_DRIVERS_PANIC_H
