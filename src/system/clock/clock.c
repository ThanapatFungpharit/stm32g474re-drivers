#include "clock.h"


// === Clock Source ===

/**
 * @brief Select the system clock source.
 *
 * Selects HSI or PLL as the SYSCLK source through the SW field
 * of RCC->CFGR and waits until the selected source is reflected
 * in the SWS status field.
 *
 * @param source Clock source to select.
 *
 * @note The selected clock source must already be configured and ready.
 */
void switchClockSource(const ClockSource source) {
    uint32_t status;

    switch (source) {
        case CLOCKSOURCE_HSI:
            status = RCC_CFGR_SWS_HSI;
            break;
        case CLOCKSOURCE_PLL:
            status = RCC_CFGR_SWS_PLL;
            break;
        default:
            panic("Invalid clock source");
    }

    RCC->CFGR =
        (RCC->CFGR & ~RCC_CFGR_SW) |
        source;
    while ((RCC->CFGR & RCC_CFGR_SWS) != status) {
    }
}


/**
 * @brief Enable or disable a clock source without waiting for readiness.
 *
 * Controls the HSI or PLL enable state through the RCC->CR register.
 * The function returns immediately without waiting for the clock source
 * to become ready or stop.
 *
 * @param source Clock source to control.
 * @param state Desired clock source state.
 *
 * @note Use setSourceClockState() when the caller must wait for the
 *       requested state to be confirmed by the corresponding ready flag.
 */
void setSourceClockStateAsync(
    const ClockSource source,
    const EnableState state)
{
    switch (source) {
        case CLOCKSOURCE_HSI:
            if (state == STATE_ENABLE)
                RCC->CR |= RCC_CR_HSION;
            else
                RCC->CR &= ~RCC_CR_HSION;
            break;

        case CLOCKSOURCE_PLL:
            if (state == STATE_ENABLE)
                RCC->CR |= RCC_CR_PLLON;
            else
                RCC->CR &= ~RCC_CR_PLLON;
            break;

        default:
            panic("Invalid clock source.");
    }
}


/**
 * @brief Enable or disable a clock source and wait for completion.
 *
 * Controls the HSI or PLL clock source and waits until the corresponding
 * RCC->CR ready status reflects the requested state.
 *
 * @param source Clock source to control.
 * @param state Desired clock source state.
 *
 * @note This function blocks until the requested clock state is reached.
 */
void setSourceClockState(const ClockSource source, const EnableState state)
{
    setSourceClockStateAsync(source, state);

    switch (source) {
        case CLOCKSOURCE_HSI:
            if (state == STATE_ENABLE) {
                while (!(RCC->CR & RCC_CR_HSIRDY)) {
                }
            } else {
                while (RCC->CR & RCC_CR_HSIRDY) {
                }
            }
            break;

        case CLOCKSOURCE_PLL:
            if (state == STATE_ENABLE) {
                while (!(RCC->CR & RCC_CR_PLLRDY)) {
                }
            } else {
                while (RCC->CR & RCC_CR_PLLRDY) {
                }
            }
            break;

        default:
            panic("Invalid clock source.");
    }
}


// === PLL ===

/**
 * @brief Configure the main PLL.
 *
 * Configures the PLL input source, multiplication factor, and output
 * division factor through RCC->PLLCFGR.
 *
 * @param m PLLM input divider value.
 * @param n PLLN multiplication value.
 * @param r PLLR output divider value.
 *
 * @note The PLL must be disabled before its configuration is changed.
 *       The supplied values must satisfy the STM32G474 PLL constraints.
 */
void configurePLL(
    const uint32_t m,
    const uint32_t n,
    const uint32_t r)
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
 * Configures the AHB, APB1, and APB2 prescalers in RCC->CFGR for
 * no clock division.
 *
 * @note The resulting bus clock frequencies must remain within the
 *       limits specified for the STM32G474 device.
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
 * @brief Set the Flash memory wait-state latency without waiting.
 *
 * Updates the LATENCY field of FLASH->ACR with the requested
 * Flash memory access latency and returns immediately.
 *
 * @param latency Flash latency value, such as FLASH_ACR_LATENCY_0WS
 *                or FLASH_ACR_LATENCY_4WS.
 *
 * @note This function does not wait for the requested latency value
 *       to be reflected in FLASH->ACR.
 */
void flashSetLatencyAsync(const uint32_t latency)
{
    FLASH->ACR =
        (FLASH->ACR & ~FLASH_ACR_LATENCY) |
        latency;
}


/**
 * @brief Set the Flash memory wait-state latency.
 *
 * Updates the LATENCY field of FLASH->ACR and waits until the requested
 * value is reflected in the register.
 *
 * @param latency Flash latency value, such as FLASH_ACR_LATENCY_0WS
 *                or FLASH_ACR_LATENCY_4WS.
 */
void flashSetLatency(const uint32_t latency)
{
    flashSetLatencyAsync(latency);
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != latency) {}
}


// === Clock ===

/**
 * @brief Configure the MCU system clock.
 *
 * Configures the power supply, Flash latency, bus prescalers, PLL,
 * and system clock source for one of the predefined system clock
 * speeds supported by the driver.
 *
 * @param speed Target system clock speed.
 *
 * @note Supported speeds are CLOCKSPEED_16MHZ, CLOCKSPEED_80MHZ,
 *       and CLOCKSPEED_170MHZ.
 *
 * @note The function blocks while clock sources are enabled, disabled,
 *       or switched and updates the CMSIS SystemCoreClock variable
 *       after the hardware clock configuration is complete.
 *
 * @note An unsupported clock speed causes the function to enter panic().
 */
void setClockSpeed(const ClockSpeed speed)
{
    // Enable the PWR peripheral clock.
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;

    // Configure voltage scaling.
    setVoltageScaling(VOLTAGE_SCALE_RANGE_1);

    // Enable HSI and wait until it is stable.
    setSourceClockState(CLOCKSOURCE_HSI, STATE_ENABLE);

    // HSI provides a safe clock source while changing the PLL.
    switchClockSource(CLOCKSOURCE_HSI);

    // PLL configuration can only be changed while PLL is disabled.
    setSourceClockState(CLOCKSOURCE_PLL, STATE_DISABLE);

    switch (speed) {
        case CLOCKSPEED_16MHZ:
            setPowerSupply(POWERSUPPLY_LDO);
            flashSetLatency(FLASH_ACR_LATENCY_0WS);
            resetBusPrescalers();
            break;

        case CLOCKSPEED_80MHZ:
            setPowerSupply(POWERSUPPLY_LDO);
            flashSetLatency(FLASH_ACR_LATENCY_4WS);

            // HSI -> PLLM / PLLN -> PLLR
            configurePLL(
                1U,     /* PLLM */
                20U,    /* PLLN */
                0U      /* PLLR = /2 */
            );

            resetBusPrescalers();
            setSourceClockState(CLOCKSOURCE_PLL, STATE_ENABLE);
            switchClockSource(CLOCKSOURCE_PLL);
            break;

        case CLOCKSPEED_170MHZ:
            /* Range 1 boost mode requires HCLK to be divided by two first. */
            RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE) | RCC_CFGR_HPRE_DIV2;
            setPowerSupply(POWERSUPPLY_SMPS);
            flashSetLatency(FLASH_ACR_LATENCY_4WS);

            // HSI -> PLLM / PLLN -> PLLR
            configurePLL(
                3U,     /* PLLM */
                85U,    /* PLLN */
                0U      /* PLLR = /2 */
            );

            resetBusPrescalers();
            setSourceClockState(CLOCKSOURCE_PLL, STATE_ENABLE);
            switchClockSource(CLOCKSOURCE_PLL);
            break;

        default:
            panic("Unsupported clock speed");
    }

    // Synchronize the CMSIS SystemCoreClock variable with
    // the newly configured hardware clock tree.
    SystemCoreClockUpdate();
}
