#include "gpio.h"


/**
 * @brief Get the RCC clock-enable mask for a GPIO peripheral.
 *
 * Maps a GPIO peripheral instance to its corresponding AHB2
 * clock-enable bit in RCC->AHB2ENR.
 *
 * @param GPIOx GPIO peripheral instance (GPIOA ... GPIOG).
 *
 * @return Clock-enable bit mask, or 0 if GPIOx is not recognized.
 */
uint32_t getGPIOxClock(const GPIO_TypeDef *GPIOx) {
    switch ((uintptr_t)GPIOx) {
        case (uintptr_t)GPIOA:
            return RCC_AHB2ENR_GPIOAEN;
        case (uintptr_t)GPIOB:
            return RCC_AHB2ENR_GPIOBEN;
        case (uintptr_t)GPIOC:
            return RCC_AHB2ENR_GPIOCEN;
        case (uintptr_t)GPIOD:
            return RCC_AHB2ENR_GPIODEN;
        case (uintptr_t)GPIOE:
            return RCC_AHB2ENR_GPIOEEN;
        case (uintptr_t)GPIOF:
            return RCC_AHB2ENR_GPIOFEN;
        case (uintptr_t)GPIOG:
            return RCC_AHB2ENR_GPIOGEN;
        default:
            // unreachable if types are respected
            panic("Invalid GPIO port in getGPIOxClock()");
    }
}

/**
 * @brief Enable or disable the peripheral clock for a GPIO port.
 *
 * Looks up the AHB2 clock-enable bit for @p GPIOx via getGPIOxClock()
 * and sets or clears it in RCC->AHB2ENR.
 *
 * @param GPIOx GPIO peripheral instance (GPIOA ... GPIOG).
 * @param state Desired clock state:
 *        - STATE_ENABLE: enable the GPIO port's clock.
 *        - STATE_DISABLE: disable the GPIO port's clock.
 *
 * @note Reads back RCC->AHB2ENR after the write so the clock change
 *       is guaranteed to have propagated before the function returns.
 */
void setGPIOxClock(const GPIO_TypeDef *GPIOx, EnableState state) {
    const uint32_t clock = getGPIOxClock(GPIOx);
    switch (state) {
        case STATE_ENABLE:
            RCC->AHB2ENR |= clock;
            break;

        case STATE_DISABLE:
            RCC->AHB2ENR &= ~clock;
            break;

        default:
            // unreachable if types are respected
            panic("Invalid enable state in toggleGPIOxClock()");
    }

    (void)RCC->AHB2ENR;
}

/**
 * @brief Set a GPIO pin's operating mode.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param mode Mode to write to the MODER register.
 */
void setPinMode(GPIO_TypeDef *GPIOx, Pin pin, PinMode mode) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in setPinMode().");

    const uint32_t shift = pin << 1;
    const uint32_t mask  = 0x3U << shift;

    GPIOx->MODER =
        (GPIOx->MODER & ~mask) |
        ((uint32_t)mode << shift);
}

/**
 * @brief Set a GPIO pin's pull configuration.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param pull Pull configuration to write to PUPDR.
 */
void setPinPull(GPIO_TypeDef *GPIOx, Pin pin, PinPull pull) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in setPinPull().");

    const uint32_t shift = pin << 1;
    const uint32_t mask  = 0x3U << shift;

    GPIOx->PUPDR =
        (GPIOx->PUPDR & ~mask) |
        ((uint32_t)pull << shift);
}

/**
 * @brief Set a GPIO pin's output speed.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param slew Output speed to write to OSPEEDR.
 */
void setPinSlew(GPIO_TypeDef *GPIOx, Pin pin, PinSlew slew) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in setPinSlew().");

    const uint32_t shift = pin << 1;
    const uint32_t mask  = 0x3U << shift;

    GPIOx->OSPEEDR =
        (GPIOx->OSPEEDR & ~mask) |
        ((uint32_t)slew << shift);
}

/**
 * @brief Set a GPIO pin's output type.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param otype Push-pull or open-drain output type.
 */
void setPinOType(GPIO_TypeDef *GPIOx, Pin pin, PinOType otype) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in setPinOType().");

    switch (otype) {
        case PINOTYPE_OPENDRAIN:
            GPIOx->OTYPER |= (1U << pin);
            break;

        case PINOTYPE_PUSHPULL:
            GPIOx->OTYPER &= ~(1U << pin);
            break;
    }
}

/**
 * @brief Set or reset a GPIO output pin.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param value Output state to write through BSRR.
 */
void digitalWrite(GPIO_TypeDef *GPIOx,
                  Pin pin,
                  EnableState value) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in digitalWrite().");

    switch (value) {
        case STATE_ENABLE:
            GPIOx->BSRR = 1U << pin;
            break;

        case STATE_DISABLE:
            GPIOx->BSRR = 1U << (pin + 16U);
            break;
    }
}

/**
 * @brief Read a GPIO input pin.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @return STATE_ENABLE when the input is high; otherwise STATE_DISABLE.
 */
EnableState digitalRead(const GPIO_TypeDef *GPIOx, Pin pin) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in digitalRead().");

    return (GPIOx->IDR & (1U << pin))
           ? STATE_ENABLE
           : STATE_DISABLE;
}
