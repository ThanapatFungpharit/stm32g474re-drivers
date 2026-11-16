/* Host tests: GPIO and EXTI register programming. */
#include <unity.h>

#include "drivers/exti/exti.h"
#include "drivers/gpio/gpio.h"

void setUp(void) { mock_reset(); }
void tearDown(void) {}

static void test_init_output_programs_all_fields_and_clock(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, gpio_init_output(GPIOA, 5U, true));

    TEST_ASSERT_BITS(RCC_AHB2ENR_GPIOAEN, RCC_AHB2ENR_GPIOAEN, mock_RCC.AHB2ENR);
    TEST_ASSERT_EQUAL_HEX32(0x1UL << 10, mock_GPIOA.MODER & (0x3UL << 10));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_GPIOA.OTYPER & (1UL << 5));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_GPIOA.PUPDR & (0x3UL << 10));
    /* Level preset through BSRR before the pin became an output. */
    TEST_ASSERT_EQUAL_HEX32(1UL << 5, mock_GPIOA.BSRR);
}

static void test_init_keeps_other_pins(void)
{
    mock_GPIOB.MODER = 0xFFFFFFFFUL; /* reset state: all analog */
    TEST_ASSERT_EQUAL(DRV_OK, gpio_init_input(GPIOB, 7U, GPIO_PULL_UP));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFFUL & ~(0x3UL << 14), mock_GPIOB.MODER);
    TEST_ASSERT_EQUAL_HEX32(0x1UL << 14, mock_GPIOB.PUPDR);
}

static void test_alternate_function_uses_high_register_for_pins_8_to_15(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, gpio_init_af(GPIOC, 10U, 5U, GPIO_OTYPE_OPEN_DRAIN,
                                           GPIO_PULL_UP, GPIO_SPEED_VERY_HIGH));
    TEST_ASSERT_EQUAL_HEX32(5UL << 8, mock_GPIOC.AFR[1]);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_GPIOC.AFR[0]);
    TEST_ASSERT_EQUAL_HEX32(0x2UL << 20, mock_GPIOC.MODER);
    TEST_ASSERT_EQUAL_HEX32(1UL << 10, mock_GPIOC.OTYPER);
    TEST_ASSERT_EQUAL_HEX32(0x3UL << 20, mock_GPIOC.OSPEEDR);

    TEST_ASSERT_EQUAL(DRV_OK, gpio_set_af(GPIOC, 0U, 15U));
    TEST_ASSERT_EQUAL_HEX32(0xFUL, mock_GPIOC.AFR[0]);
}

static void test_invalid_arguments_are_rejected_without_side_effects(void)
{
    const GpioConfig bad_af = { GPIO_MODE_AF, GPIO_PULL_NONE, GPIO_OTYPE_PUSH_PULL,
                                GPIO_SPEED_LOW, 16U };

    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, gpio_init_output(GPIOA, 16U, false));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, gpio_init_input((GPIO_TypeDef *)&mock_TIM1, 0U, GPIO_PULL_NONE));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, gpio_init(GPIOA, 1U, &bad_af));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, gpio_init(GPIOA, 1U, NULL));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, gpio_set_mode(GPIOA, 1U, (GpioMode)4));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, gpio_set_pull(GPIOA, 1U, (GpioPull)3));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, gpio_set_clock(NULL, true));

    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.AHB2ENR);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_GPIOA.MODER);
}

static void test_write_read_toggle(void)
{
    gpio_write(GPIOB, 3U, false);
    TEST_ASSERT_EQUAL_HEX32(1UL << 19, mock_GPIOB.BSRR);

    mock_GPIOB.ODR = 1UL << 3;
    gpio_toggle(GPIOB, 3U);
    TEST_ASSERT_EQUAL_HEX32(1UL << 19, mock_GPIOB.BSRR);
    mock_GPIOB.ODR = 0U;
    gpio_toggle(GPIOB, 3U);
    TEST_ASSERT_EQUAL_HEX32(1UL << 3, mock_GPIOB.BSRR);

    mock_GPIOB.IDR = 1UL << 15;
    TEST_ASSERT_TRUE(gpio_read(GPIOB, 15U));
    TEST_ASSERT_FALSE(gpio_read(GPIOB, 14U));
}

static void test_fast_path_asserts_on_bad_pin(void)
{
    TEST_ASSERT_TRUE(MOCK_PANICS(gpio_write(GPIOA, 16U, true)));
    TEST_ASSERT_TRUE(MOCK_PANICS((void)gpio_read(GPIOA, 200U)));
    TEST_ASSERT_FALSE(MOCK_PANICS(gpio_write(GPIOA, 15U, true)));
}

static void test_port_index(void)
{
    TEST_ASSERT_EQUAL_INT32(0, gpio_port_index(GPIOA));
    TEST_ASSERT_EQUAL_INT32(6, gpio_port_index(GPIOG));
    TEST_ASSERT_EQUAL_INT32(-1, gpio_port_index(NULL));
}

static void test_exti_routes_pc13_rising_edge(void)
{
    mock_EXTI.FTSR1 = 1UL << 13; /* stale configuration must be cleared */

    TEST_ASSERT_EQUAL(DRV_OK, exti_enable_gpio(GPIOC, 13U, EXTI_TRIGGER_RISING, 5U));

    TEST_ASSERT_BITS(RCC_APB2ENR_SYSCFGEN, RCC_APB2ENR_SYSCFGEN, mock_RCC.APB2ENR);
    TEST_ASSERT_EQUAL_HEX32(2UL << 4, mock_SYSCFG.EXTICR[3]); /* port C = 2, line 13 */
    TEST_ASSERT_EQUAL_HEX32(1UL << 13, mock_EXTI.RTSR1);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_EXTI.FTSR1);
    TEST_ASSERT_EQUAL_HEX32(1UL << 13, mock_EXTI.IMR1);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_nvic_enabled[EXTI15_10_IRQn]);
    TEST_ASSERT_EQUAL_UINT32(5U, mock_nvic_priority[EXTI15_10_IRQn]);
}

static void test_exti_both_edges_and_irq_mapping(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, exti_enable_gpio(GPIOB, 6U, EXTI_TRIGGER_BOTH, 15U));
    TEST_ASSERT_EQUAL_HEX32(1UL << 6, mock_EXTI.RTSR1);
    TEST_ASSERT_EQUAL_HEX32(1UL << 6, mock_EXTI.FTSR1);
    TEST_ASSERT_EQUAL_HEX32(1UL << 8, mock_SYSCFG.EXTICR[1]);

    TEST_ASSERT_EQUAL(EXTI0_IRQn, exti_irqn(0U));
    TEST_ASSERT_EQUAL(EXTI4_IRQn, exti_irqn(4U));
    TEST_ASSERT_EQUAL(EXTI9_5_IRQn, exti_irqn(5U));
    TEST_ASSERT_EQUAL(EXTI9_5_IRQn, exti_irqn(9U));
    TEST_ASSERT_EQUAL(EXTI15_10_IRQn, exti_irqn(10U));

    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, exti_enable_gpio(GPIOB, 16U, EXTI_TRIGGER_BOTH, 0U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, exti_enable_gpio(GPIOB, 1U, (ExtiTrigger)0, 0U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, exti_enable_gpio(GPIOB, 1U, EXTI_TRIGGER_BOTH, 16U));
}

static void test_exti_take_pending(void)
{
    TEST_ASSERT_FALSE(exti_take_pending(3U));
    mock_EXTI.PR1 = 1UL << 3;
    TEST_ASSERT_TRUE(exti_take_pending(3U));
    TEST_ASSERT_EQUAL_HEX32(1UL << 3, mock_EXTI.PR1); /* write-1-to-clear */
    TEST_ASSERT_EQUAL(DRV_OK, exti_disable(3U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, exti_disable(16U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_output_programs_all_fields_and_clock);
    RUN_TEST(test_init_keeps_other_pins);
    RUN_TEST(test_alternate_function_uses_high_register_for_pins_8_to_15);
    RUN_TEST(test_invalid_arguments_are_rejected_without_side_effects);
    RUN_TEST(test_write_read_toggle);
    RUN_TEST(test_fast_path_asserts_on_bad_pin);
    RUN_TEST(test_port_index);
    RUN_TEST(test_exti_routes_pc13_rising_edge);
    RUN_TEST(test_exti_both_edges_and_irq_mapping);
    RUN_TEST(test_exti_take_pending);
    return UNITY_END();
}
