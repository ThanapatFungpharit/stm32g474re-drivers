#include "exti.h"

#include "drivers/gpio/gpio.h"
#include "drivers/internal/drv_internal.h"


static const DrvClockGate SYSCFG_GATE = { DRV_BUS_APB2, RCC_APB2ENR_SYSCFGEN };


IRQn_Type exti_irqn(const uint8_t line)
{
    DRV_ASSERT(line <= 15U);

    switch (line) {
        case 0U: return EXTI0_IRQn;
        case 1U: return EXTI1_IRQn;
        case 2U: return EXTI2_IRQn;
        case 3U: return EXTI3_IRQn;
        case 4U: return EXTI4_IRQn;
        default: return (line <= 9U) ? EXTI9_5_IRQn : EXTI15_10_IRQn;
    }
}

DrvStatus exti_enable_gpio(GPIO_TypeDef *port, const uint8_t pin,
                           const ExtiTrigger trigger, const uint32_t priority)
{
    const int32_t port_index = gpio_port_index(port);

    if ((port_index < 0) || (pin > 15U) || (priority > DRV_NVIC_PRIORITY_MAX) ||
        (trigger < EXTI_TRIGGER_RISING) || (trigger > EXTI_TRIGGER_BOTH)) {
        return DRV_ERR_PARAM;
    }

    const uint32_t bit = 1UL << pin;

    if (!drv_clock_gate_is_enabled(SYSCFG_GATE)) {
        drv_clock_gate_set(SYSCFG_GATE, true);
    }

    /* Mask the line while it is being re-routed. */
    drv_reg_modify(&EXTI->IMR1, bit, 0U);

    const uint32_t shift = ((uint32_t)pin & 3U) * 4U;
    drv_reg_modify(&SYSCFG->EXTICR[pin >> 2], 0xFUL << shift,
                   (uint32_t)port_index << shift);

    drv_reg_modify(&EXTI->RTSR1, bit, ((trigger & EXTI_TRIGGER_RISING) != 0U) ? bit : 0U);
    drv_reg_modify(&EXTI->FTSR1, bit, ((trigger & EXTI_TRIGGER_FALLING) != 0U) ? bit : 0U);

    EXTI->PR1 = bit; /* write 1 to clear */
    drv_reg_modify(&EXTI->IMR1, 0U, bit);

    drv_nvic_enable(exti_irqn(pin), priority);
    return DRV_OK;
}

DrvStatus exti_disable(const uint8_t line)
{
    if (line > 15U) {
        return DRV_ERR_PARAM;
    }

    const uint32_t bit = 1UL << line;
    drv_reg_modify(&EXTI->IMR1, bit, 0U);
    drv_reg_modify(&EXTI->RTSR1, bit, 0U);
    drv_reg_modify(&EXTI->FTSR1, bit, 0U);
    EXTI->PR1 = bit;
    return DRV_OK;
}

bool exti_take_pending(const uint8_t line)
{
    DRV_ASSERT(line <= 15U);

    const uint32_t bit = 1UL << line;
    if ((EXTI->PR1 & bit) == 0U) {
        return false;
    }
    EXTI->PR1 = bit;
    return true;
}
