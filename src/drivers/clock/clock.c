#include "clock.h"


// === GPIOx clocks ===

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
unsigned long getGPIOxClock(const GPIO_TypeDef *GPIOx) {
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
            return 0;
    }
}

/**
 * @brief Enable the clock for a GPIO peripheral.
 *
 * Sets the corresponding GPIO clock-enable bit in RCC->AHB2ENR.
 * An additional read ensures the register write has completed before
 * the function returns.
 *
 * @param GPIOx GPIO peripheral whose clock should be enabled.
 *
 * @note If GPIOx is not recognized, the function does nothing.
 */
void enableGPIOxClock(const GPIO_TypeDef *GPIOx) {
    unsigned long clock = getGPIOxClock(GPIOx);
    if (clock == 0) return;

    RCC->AHB2ENR |= clock;

    // Ensure the peripheral clock write has completed.
    (void)RCC->AHB2ENR;
}

/**
 * @brief Disable the clock for a GPIO peripheral.
 *
 * Clears the corresponding GPIO clock-enable bit in RCC->AHB2ENR.
 * An additional read ensures the register write has completed before
 * the function returns.
 *
 * @param GPIOx GPIO peripheral whose clock should be disabled.
 *
 * @note If GPIOx is not recognized, the function does nothing.
 */
void disableGPIOxClock(const GPIO_TypeDef *GPIOx) {
    unsigned long clock = getGPIOxClock(GPIOx);
    if (clock == 0) return;

    RCC->AHB2ENR &= ~clock;

    // Ensure the peripheral clock write has completed.
    (void)RCC->AHB2ENR;
}


// === Clock Source ===

/**
 * @brief Select the system clock source.
 *
 * Updates the SW field in RCC->CFGR while preserving all other
 * configuration bits.
 *
 * @param source Clock source to select.
 *
 * @note The selected source must already be configured and ready.
 *       This function does not wait for the switch to complete.
 */
void switchClockSource(ClockSource source) {
    RCC->CFGR =
        (RCC->CFGR & ~RCC_CFGR_SW) |
        source;
}


// === PLL ===

/**
 * @brief Enable or disable the main PLL.
 *
 * @param state PLL state:
 *        - ENABLE: enable the PLL.
 *        - DISABLE: disable the PLL.
 */
void setPLLState(EnableState state) {
    switch (state) {
        case ENABLE:
            RCC->CR |= RCC_CR_PLLON;
            break;

        case DISABLE:
            RCC->CR &= ~RCC_CR_PLLON;
    }
}

/**
 * @brief Configure the main PLL.
 *
 * Configures the PLL using HSI as its input clock.
 *
 * @param m PLLM divider value.
 * @param n PLLN multiplication value.
 * @param r PLLR output divider value.
 *
 * @note The PLL should be disabled before changing its configuration.
 *       The caller is responsible for ensuring that the supplied
 *       values are valid for the target STM32 device.
 */
void configurePLL(uint32_t m, uint32_t n, uint32_t r)
{
    RCC->PLLCFGR =
        RCC_PLLCFGR_PLLSRC_HSI |
        (m << RCC_PLLCFGR_PLLM_Pos) |
        (n << RCC_PLLCFGR_PLLN_Pos) |
        (r << RCC_PLLCFGR_PLLR_Pos) |
        RCC_PLLCFGR_PLLREN;
}


// === Utils ===

/**
 * @brief Reset the AHB and APB bus prescalers.
 *
 * Configures the AHB, APB1, and APB2 prescalers for no division.
 * This results in the bus clocks running at the system clock
 * frequency, subject to the device's clock-tree constraints.
 */
void resetBusPrescalers(void)
{
    RCC->CFGR &= ~(
        RCC_CFGR_HPRE  |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2
    );
}

/**
 * @brief Set the Flash memory wait-state latency.
 *
 * Updates only the LATENCY field of FLASH->ACR and waits until
 * the requested value is reflected in the register.
 *
 * @param latency Flash latency value, such as FLASH_ACR_LATENCY_0WS
 *                or FLASH_ACR_LATENCY_4WS.
 */
void flashSetLatency(uint32_t latency)
{
    FLASH->ACR =
        (FLASH->ACR & ~FLASH_ACR_LATENCY) |
        latency;

    while ((FLASH->ACR & FLASH_ACR_LATENCY) != latency) {}
}


// === Clock ===

/**
 * @brief Configure the MCU system clock.
 *
 * Changes the system clock configuration to one of the predefined
 * clock speeds supported by this driver.
 *
 * The function:
 *  1. Enables the PWR peripheral clock.
 *  2. Configures the voltage scaling.
 *  3. Enables HSI and waits until it is ready.
 *  4. Temporarily switches the system clock to HSI.
 *  5. Disables the PLL before reconfiguration.
 *  6. Configures the requested clock speed.
 *  7. Updates the CMSIS SystemCoreClock variable.
 *
 * Supported speeds:
 *  - CLOCK_16MHZ  : HSI directly.
 *  - CLOCK_80MHZ  : PLL from HSI.
 *  - CLOCK_170MHZ : PLL from HSI.
 *
 * @param speed Target system clock speed.
 *
 * @note An unsupported ClockSpeed causes the function to enter an
 *       infinite loop.
 *
 * @note This function assumes the clock configuration values are
 *       valid for the target STM32G474 device.
 */
void setClockSpeed(ClockSpeed speed)
{
    // Enable the PWR peripheral clock.
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;

    // Configure voltage scaling.
    PWR->CR1 =
        (PWR->CR1 & ~PWR_CR1_VOS) |
        PWR_CR1_VOS_0;

    // Enable HSI and wait until it is stable.
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {
    }

    // HSI provides a safe clock source while changing the PLL.
    switchClockSource(CLOCKSOURCE_HSI);

    // PLL configuration can only be changed while PLL is disabled.
    setPLLState(DISABLE);

    switch (speed) {
        case CLOCK_16MHZ:
            setPowerSupply(POWERSUPPLY_LDO);
            flashSetLatency(FLASH_ACR_LATENCY_0WS);
            break;

        case CLOCK_80MHZ:
            setPowerSupply(POWERSUPPLY_LDO);
            flashSetLatency(FLASH_ACR_LATENCY_4WS);

            // HSI -> PLLM / PLLN -> PLLR
            configurePLL(
                1U,     /* PLLM */
                20U,    /* PLLN */
                0U      /* PLLR = /2 */
            );

            resetBusPrescalers();
            setPLLState(ENABLE);
            switchClockSource(CLOCKSOURCE_PLL);
            break;

        case CLOCK_170MHZ:
            setPowerSupply(POWERSUPPLY_SMPS);
            flashSetLatency(FLASH_ACR_LATENCY_4WS);

            // HSI -> PLLM / PLLN -> PLLR
            configurePLL(
                3U,     /* PLLM */
                85U,    /* PLLN */
                0U      /* PLLR = /2 */
            );

            resetBusPrescalers();
            setPLLState(ENABLE);
            switchClockSource(CLOCKSOURCE_PLL);
            break;

        default:
            // Unsupported clock speed.
            while (1) {}
    }

    // Synchronize the CMSIS SystemCoreClock variable with
    // the newly configured hardware clock tree.
    SystemCoreClockUpdate();
}
