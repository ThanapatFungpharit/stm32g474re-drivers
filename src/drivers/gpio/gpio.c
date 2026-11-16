#include "gpio.h"

#include "drivers/internal/drv_internal.h"


int32_t gpio_port_index(const GPIO_TypeDef *port)
{
    if (port == GPIOA) return 0;
    if (port == GPIOB) return 1;
    if (port == GPIOC) return 2;
    if (port == GPIOD) return 3;
    if (port == GPIOE) return 4;
    if (port == GPIOF) return 5;
    if (port == GPIOG) return 6;
    return -1;
}

static bool valid_pin(const GPIO_TypeDef *port, const uint8_t pin)
{
    return (gpio_port_index(port) >= 0) && (pin <= GPIO_PIN_MAX);
}

static DrvClockGate port_gate(const GPIO_TypeDef *port)
{
    /* GPIOxEN bits are 0..6 in port order. */
    const DrvClockGate gate = { DRV_BUS_AHB2, 1UL << (uint32_t)gpio_port_index(port) };
    return gate;
}

DrvStatus gpio_set_clock(GPIO_TypeDef *port, const bool enable)
{
    if (gpio_port_index(port) < 0) {
        return DRV_ERR_PARAM;
    }
    drv_clock_gate_set(port_gate(port), enable);
    return DRV_OK;
}

/** @brief Write a 2-bit field for @p pin into a per-pin 2-bit register. */
static void write_field2(volatile uint32_t *reg, const uint8_t pin, const uint32_t value)
{
    const uint32_t shift = (uint32_t)pin * 2U;
    drv_reg_modify(reg, 0x3UL << shift, (value & 0x3UL) << shift);
}

DrvStatus gpio_set_mode(GPIO_TypeDef *port, const uint8_t pin, const GpioMode mode)
{
    if (!valid_pin(port, pin) || ((uint32_t)mode > 3U)) {
        return DRV_ERR_PARAM;
    }
    write_field2(&port->MODER, pin, (uint32_t)mode);
    return DRV_OK;
}

DrvStatus gpio_set_pull(GPIO_TypeDef *port, const uint8_t pin, const GpioPull pull)
{
    if (!valid_pin(port, pin) || ((uint32_t)pull > 2U)) {
        return DRV_ERR_PARAM;
    }
    write_field2(&port->PUPDR, pin, (uint32_t)pull);
    return DRV_OK;
}

DrvStatus gpio_set_speed(GPIO_TypeDef *port, const uint8_t pin, const GpioSpeed speed)
{
    if (!valid_pin(port, pin) || ((uint32_t)speed > 3U)) {
        return DRV_ERR_PARAM;
    }
    write_field2(&port->OSPEEDR, pin, (uint32_t)speed);
    return DRV_OK;
}

DrvStatus gpio_set_output_type(GPIO_TypeDef *port, const uint8_t pin,
                               const GpioOutputType otype)
{
    if (!valid_pin(port, pin) || ((uint32_t)otype > 1U)) {
        return DRV_ERR_PARAM;
    }
    const uint32_t bit = 1UL << pin;
    drv_reg_modify(&port->OTYPER, bit, (otype == GPIO_OTYPE_OPEN_DRAIN) ? bit : 0U);
    return DRV_OK;
}

DrvStatus gpio_set_af(GPIO_TypeDef *port, const uint8_t pin, const uint8_t af)
{
    if (!valid_pin(port, pin) || (af > 15U)) {
        return DRV_ERR_PARAM;
    }
    const uint32_t shift = ((uint32_t)pin & 7U) * 4U;
    drv_reg_modify(&port->AFR[pin >> 3], 0xFUL << shift, (uint32_t)af << shift);
    return DRV_OK;
}

DrvStatus gpio_init(GPIO_TypeDef *port, const uint8_t pin, const GpioConfig *config)
{
    if ((config == NULL) || !valid_pin(port, pin) ||
        ((uint32_t)config->mode > 3U) || ((uint32_t)config->pull > 2U) ||
        ((uint32_t)config->otype > 1U) || ((uint32_t)config->speed > 3U) ||
        (config->af > 15U)) {
        return DRV_ERR_PARAM;
    }

    if (!drv_clock_gate_is_enabled(port_gate(port))) {
        drv_clock_gate_set(port_gate(port), true);
    }

    /* Arguments are validated, so the setters below cannot fail. */
    (void)gpio_set_output_type(port, pin, config->otype);
    (void)gpio_set_speed(port, pin, config->speed);
    (void)gpio_set_pull(port, pin, config->pull);
    if (config->mode == GPIO_MODE_AF) {
        (void)gpio_set_af(port, pin, config->af);
    }
    (void)gpio_set_mode(port, pin, config->mode);
    return DRV_OK;
}

DrvStatus gpio_init_output(GPIO_TypeDef *port, const uint8_t pin, const bool level)
{
    if (!valid_pin(port, pin)) {
        return DRV_ERR_PARAM;
    }
    const GpioConfig config = {
        .mode = GPIO_MODE_OUTPUT, .pull = GPIO_PULL_NONE,
        .otype = GPIO_OTYPE_PUSH_PULL, .speed = GPIO_SPEED_LOW, .af = 0U,
    };

    if (!drv_clock_gate_is_enabled(port_gate(port))) {
        drv_clock_gate_set(port_gate(port), true);
    }
    gpio_write(port, pin, level); /* preset ODR before enabling the driver */
    return gpio_init(port, pin, &config);
}

DrvStatus gpio_init_input(GPIO_TypeDef *port, const uint8_t pin, const GpioPull pull)
{
    const GpioConfig config = {
        .mode = GPIO_MODE_INPUT, .pull = pull,
        .otype = GPIO_OTYPE_PUSH_PULL, .speed = GPIO_SPEED_LOW, .af = 0U,
    };
    return gpio_init(port, pin, &config);
}

DrvStatus gpio_init_analog(GPIO_TypeDef *port, const uint8_t pin)
{
    const GpioConfig config = {
        .mode = GPIO_MODE_ANALOG, .pull = GPIO_PULL_NONE,
        .otype = GPIO_OTYPE_PUSH_PULL, .speed = GPIO_SPEED_LOW, .af = 0U,
    };
    return gpio_init(port, pin, &config);
}

DrvStatus gpio_init_af(GPIO_TypeDef *port, const uint8_t pin, const uint8_t af,
                       const GpioOutputType otype, const GpioPull pull,
                       const GpioSpeed speed)
{
    const GpioConfig config = {
        .mode = GPIO_MODE_AF, .pull = pull, .otype = otype, .speed = speed, .af = af,
    };
    return gpio_init(port, pin, &config);
}

void gpio_write(GPIO_TypeDef *port, const uint8_t pin, const bool level)
{
    DRV_ASSERT(pin <= GPIO_PIN_MAX);
    port->BSRR = level ? (1UL << pin) : (1UL << (pin + 16U));
}

void gpio_toggle(GPIO_TypeDef *port, const uint8_t pin)
{
    DRV_ASSERT(pin <= GPIO_PIN_MAX);
    const uint32_t bit = 1UL << pin;
    port->BSRR = ((port->ODR & bit) != 0U) ? (bit << 16U) : bit;
}

bool gpio_read(const GPIO_TypeDef *port, const uint8_t pin)
{
    DRV_ASSERT(pin <= GPIO_PIN_MAX);
    return (port->IDR & (1UL << pin)) != 0U;
}
