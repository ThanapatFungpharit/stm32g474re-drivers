//
// Created by user on 8/18/2026.
//

#ifndef STM32G474RE_DRIVERS_CONSOLE_H
#define STM32G474RE_DRIVERS_CONSOLE_H

#include "stm32g474xx.h"

#include <stdbool.h>
#include <stddef.h>


/**
 * @brief Check whether ITM stimulus port 0 can accept output.
 * @return true when tracing and port 0 are enabled and ready; otherwise false.
 */
bool console_is_ready(void);

/**
 * @brief Initialize ITM stimulus port 0 for console output.
 * @note Enables trace access and writes ITM control registers.
 */
void console_init(void);

/**
 * @brief Attempt to write one character through ITM port 0.
 * @param c Character to write.
 * @return true when the character was accepted; otherwise false.
 */
bool console_try_putc(char c);

/**
 * @brief Write one character through ITM port 0.
 * @param c Character to write.
 * @note Drops the character when ITM is unavailable.
 */
void console_putc(char c);

/**
 * @brief Write a null-terminated string through ITM port 0.
 * @param str String to write, or NULL.
 * @note Stops and drops remaining characters when ITM is unavailable.
 */
void console_puts(const char *str);

/**
 * @brief Write bytes through ITM port 0.
 * @param buffer Bytes to write, or NULL.
 * @param len Number of bytes to write.
 * @return Number of bytes accepted by ITM.
 */
size_t console_write(const char *buffer, size_t len);

#endif //STM32G474RE_DRIVERS_CONSOLE_H
