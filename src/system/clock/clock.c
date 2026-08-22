#include "clock.h"


// === Clock Source ===

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
    while ((RCC->CFGR & RCC_CFGR_SWS) != status);
}

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

void setSourceClockState(const ClockSource source, const EnableState state)
{
    setSourceClockStateAsync(source, state);

    switch (source) {
        case CLOCKSOURCE_HSI:
            if (state == STATE_ENABLE) {
                while (!(RCC->CR & RCC_CR_HSIRDY));
            } else {
                while (RCC->CR & RCC_CR_HSIRDY);
            }
            break;

        case CLOCKSOURCE_PLL:
            if (state == STATE_ENABLE) {
                while (!(RCC->CR & RCC_CR_PLLRDY));
            } else {
                while (RCC->CR & RCC_CR_PLLRDY);
            }
            break;

        default:
            panic("Invalid clock source.");
    }
}


// === PLL ===

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

void resetBusPrescalers(void)
{
    RCC->CFGR &= ~(
        RCC_CFGR_HPRE  |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2
    );
}


void flashSetLatencyAsync(const uint32_t latency)
{
    FLASH->ACR =
        (FLASH->ACR & ~FLASH_ACR_LATENCY) |
        latency;
}


void flashSetLatency(const uint32_t latency)
{
    flashSetLatencyAsync(latency);
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != latency) {}
}


// === Clock ===

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
            setRegulatorMode(REGULATORMODE_LDO);
            flashSetLatency(FLASH_ACR_LATENCY_0WS);
            resetBusPrescalers();
            break;

        case CLOCKSPEED_80MHZ:
            setRegulatorMode(REGULATORMODE_LDO);
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
            /* Range 1 boost mode requires HCLK to be divided by two while
             * the switch to the higher frequency takes place. */
            RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE) | RCC_CFGR_HPRE_DIV2;
            setRegulatorMode(REGULATORMODE_SMPS);
            flashSetLatency(FLASH_ACR_LATENCY_4WS);

            // HSI -> PLLM / PLLN -> PLLR
            configurePLL(
                3U,     /* PLLM */
                85U,    /* PLLN */
                0U      /* PLLR = /2 */
            );

            setSourceClockState(CLOCKSOURCE_PLL, STATE_ENABLE);
            switchClockSource(CLOCKSOURCE_PLL);
            
            /* HCLK is now clocked from PLL at half rate (HPRE = /2).
             * Restore the AHB/APB prescalers to /1 now that the switch
             * is complete, bringing HCLK up to the full 170 MHz. */
            resetBusPrescalers();
            break;

        default:
            panic("Unsupported clock speed");
    }

    // Synchronize the CMSIS SystemCoreClock variable with
    // the newly configured hardware clock tree.
    SystemCoreClockUpdate();
}
