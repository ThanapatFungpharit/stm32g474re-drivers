#include "gpio.h"


/**
 * @brief Get the RCC clock-enable mask for a GPIO peripheral.
 * @param GPIOx GPIO peripheral instance.
 * @return Corresponding RCC AHB2 clock-enable mask.
 */
static uint32_t getGPIOxClockEnableBit(const GPIO_TypeDef *GPIOx) {
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
            panic("Invalid GPIO port in getGPIOxClockEnableBit()");
    }
}


void setGPIOxClock(const GPIO_TypeDef *GPIOx, const EnableState state) {
    const uint32_t clock = getGPIOxClockEnableBit(GPIOx);
    switch (state) {
        case STATE_ENABLE:
            RCC->AHB2ENR |= clock;
            break;

        case STATE_DISABLE:
            RCC->AHB2ENR &= ~clock;
            break;

        default:
            // unreachable if types are respected
            panic("Invalid enable state in setGPIOxClock()");
    }

    (void)RCC->AHB2ENR;
}


void setPinMode(GPIO_TypeDef *GPIOx, const Pin pin, const PinMode mode) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in setPinMode().");

    const uint32_t shift = pin << 1;
    const uint32_t mask  = 0x3U << shift;

    GPIOx->MODER =
        (GPIOx->MODER & ~mask) |
        ((uint32_t)mode << shift);
}


void setAlternateFunction(GPIO_TypeDef *GPIOx, const uint8_t pin, const AlternateFunction af) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in setPinAF().");

    const uint32_t reg   = pin >> 3;
    const uint32_t shift = (pin & 7U) << 2;
    const uint32_t mask  = 0xFU << shift;

    GPIOx->AFR[reg] =
        (GPIOx->AFR[reg] & ~mask) |
        ((uint32_t)af << shift);
}
void setPinPull(GPIO_TypeDef *GPIOx, const Pin pin, const PinPull pull) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in setPinPull().");

    const uint32_t shift = pin << 1;
    const uint32_t mask  = 0x3U << shift;

    GPIOx->PUPDR =
        (GPIOx->PUPDR & ~mask) |
        ((uint32_t)pull << shift);
}

void setPinSlew(GPIO_TypeDef *GPIOx, const Pin pin, const PinSlew slew) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in setPinSlew().");

    const uint32_t shift = pin << 1;
    const uint32_t mask  = 0x3U << shift;

    GPIOx->OSPEEDR =
        (GPIOx->OSPEEDR & ~mask) |
        ((uint32_t)slew << shift);
}

void setPinOType(GPIO_TypeDef *GPIOx, const Pin pin, const PinOType otype) {
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

void digitalWrite(
    GPIO_TypeDef *GPIOx,
    const Pin pin,
    const EnableState value
    )
{
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

EnableState digitalRead(const GPIO_TypeDef *GPIOx, const Pin pin) {
    if (pin != (pin & 0x0FU))
        panic("Invalid pin in digitalRead().");

    return (GPIOx->IDR & (1U << pin))
           ? STATE_ENABLE
           : STATE_DISABLE;
}
