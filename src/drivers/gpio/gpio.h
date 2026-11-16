#ifndef STM32G474RE_DRIVERS_GPIO_H
#define STM32G474RE_DRIVERS_GPIO_H

/**
 * @file gpio.h
 * @brief General-purpose I/O ports GPIOA-GPIOG.
 *
 * Pins are addressed by port (GPIOA...GPIOG from the CMSIS header) and pin
 * number 0-15. On the STM32G474RE (LQFP64) the bonded pins are PA0-15,
 * PB0-15, PC0-15, PD2, PF0-1 and PG10 (NRST); the other ports exist but
 * have no package pins.
 *
 * After reset every pin is in analog mode except the debug pins
 * PA13 (SWDIO), PA14 (SWCLK), PA15, PB3 (SWO) and PB4. Reconfiguring PA13 or
 * PA14 disconnects the debugger.
 *
 * Configuration functions are safe to call from interrupt handlers: their
 * read-modify-write sequences run with interrupts masked. gpio_write() and
 * gpio_toggle() use the atomic BSRR register.
 */

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Highest pin number in a port. */
#define GPIO_PIN_MAX 15U

/** Pin mode (MODER). */
typedef enum {
    GPIO_MODE_INPUT  = 0U, /**< Digital input. */
    GPIO_MODE_OUTPUT = 1U, /**< General-purpose output. */
    GPIO_MODE_AF     = 2U, /**< Alternate function (peripheral controls the pin). */
    GPIO_MODE_ANALOG = 3U, /**< Analog (ADC/DAC/COMP/OPAMP); reset state, lowest power. */
} GpioMode;

/** Internal pull resistor (PUPDR). */
typedef enum {
    GPIO_PULL_NONE = 0U,
    GPIO_PULL_UP   = 1U,
    GPIO_PULL_DOWN = 2U,
} GpioPull;

/** Output driver type (OTYPER). */
typedef enum {
    GPIO_OTYPE_PUSH_PULL  = 0U,
    GPIO_OTYPE_OPEN_DRAIN = 1U,
} GpioOutputType;

/** Output slew-rate / speed (OSPEEDR). Use the lowest speed that works to limit EMI. */
typedef enum {
    GPIO_SPEED_LOW       = 0U,
    GPIO_SPEED_MEDIUM    = 1U,
    GPIO_SPEED_HIGH      = 2U,
    GPIO_SPEED_VERY_HIGH = 3U,
} GpioSpeed;

/** Complete pin configuration for gpio_init(). */
typedef struct {
    GpioMode       mode;
    GpioPull       pull;
    GpioOutputType otype;  /**< Used in output and AF modes. */
    GpioSpeed      speed;  /**< Used in output and AF modes. */
    uint8_t        af;     /**< Alternate function 0-15, used in AF mode. */
} GpioConfig;

/**
 * @brief Enable or disable the bus clock of a GPIO port.
 * @return DRV_OK, or DRV_ERR_PARAM for an invalid port.
 */
DrvStatus gpio_set_clock(GPIO_TypeDef *port, bool enable);

/**
 * @brief Fully configure one pin, enabling the port clock if needed.
 * @param port GPIO port.
 * @param pin Pin number 0-15.
 * @param config Desired configuration.
 * @return DRV_OK, or DRV_ERR_PARAM for an invalid port, pin or field.
 * @note Registers are written in a glitch-free order: output type, speed,
 *       pull and alternate function first, the mode last.
 */
DrvStatus gpio_init(GPIO_TypeDef *port, uint8_t pin, const GpioConfig *config);

/**
 * @brief Configure a push-pull output, low speed, no pull, driven to @p level.
 * @note The output level is written before the pin becomes an output, so it
 *       does not glitch.
 */
DrvStatus gpio_init_output(GPIO_TypeDef *port, uint8_t pin, bool level);

/** @brief Configure a digital input with the given pull resistor. */
DrvStatus gpio_init_input(GPIO_TypeDef *port, uint8_t pin, GpioPull pull);

/** @brief Configure a pin in analog mode (no pull), for ADC or DAC use. */
DrvStatus gpio_init_analog(GPIO_TypeDef *port, uint8_t pin);

/**
 * @brief Configure a pin for an alternate function.
 * @param af Alternate function number 0-15 (see the datasheet AF table).
 * @param otype Push-pull or open-drain (I2C needs open-drain).
 * @param pull Pull resistor.
 * @param speed Output speed.
 */
DrvStatus gpio_init_af(GPIO_TypeDef *port, uint8_t pin, uint8_t af,
                       GpioOutputType otype, GpioPull pull, GpioSpeed speed);

/** @brief Change only the mode of a pin. */
DrvStatus gpio_set_mode(GPIO_TypeDef *port, uint8_t pin, GpioMode mode);

/** @brief Change only the pull resistor of a pin. */
DrvStatus gpio_set_pull(GPIO_TypeDef *port, uint8_t pin, GpioPull pull);

/** @brief Change only the output type of a pin. */
DrvStatus gpio_set_output_type(GPIO_TypeDef *port, uint8_t pin, GpioOutputType otype);

/** @brief Change only the output speed of a pin. */
DrvStatus gpio_set_speed(GPIO_TypeDef *port, uint8_t pin, GpioSpeed speed);

/** @brief Change only the alternate function number (0-15) of a pin. */
DrvStatus gpio_set_af(GPIO_TypeDef *port, uint8_t pin, uint8_t af);

/**
 * @brief Drive an output pin high (true) or low (false).
 * @note Atomic (BSRR). Asserts that @p pin is 0-15.
 */
void gpio_write(GPIO_TypeDef *port, uint8_t pin, bool level);

/**
 * @brief Invert an output pin.
 * @note The BSRR write is atomic, but an interrupt that changes the same
 *       pin between the ODR read and the write may be overridden.
 */
void gpio_toggle(GPIO_TypeDef *port, uint8_t pin);

/** @brief Read the input level of a pin (true = high). */
bool gpio_read(const GPIO_TypeDef *port, uint8_t pin);

/**
 * @brief Return the zero-based index of a port (GPIOA = 0 ... GPIOG = 6).
 * @return Index, or -1 for an invalid port.
 */
int32_t gpio_port_index(const GPIO_TypeDef *port);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_GPIO_H */
