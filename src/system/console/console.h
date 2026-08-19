//
// Created by user on 8/18/2026.
//

#ifndef STM32G474RE_DRIVERS_CONSOLE_H
#define STM32G474RE_DRIVERS_CONSOLE_H

#include "stm32g474xx.h"

#include <stdbool.h>
#include <stddef.h>


/**
 * @brief Check whether ITM stimulus port 0 is enabled and can accept a byte.
 * @return true when the port is enabled and ready; otherwise false.
 * This check is non-blocking, so it is also safe to use from error paths.
 */
bool console_is_ready(void);

/** @brief Initialize ITM stimulus port 0 for console output. */
void console_init(void);

/**
 * @brief Attempt to send one byte through ITM stimulus port 0 without waiting.
 * @param c Character to send.
 * @return true when the character is accepted; otherwise false.
 * Returns false if tracing is disabled or the port is busy.
 */
bool console_try_putc(char c);

/** @brief Send one character through ITM port 0. @param c Character to send. */
void console_putc(char c);
/** @brief Send a null-terminated string through ITM port 0. @param str String to send. */
void console_puts(const char *str);

/**
 * @brief Write bytes through ITM port 0 without blocking.
 * @param buffer Bytes to write, or NULL.
 * @param len Maximum number of bytes to write.
 * @return Number of bytes accepted.
 * A null buffer is treated as an empty buffer.
 */
size_t console_write(const char *buffer, size_t len);

#endif //STM32G474RE_DRIVERS_CONSOLE_H
