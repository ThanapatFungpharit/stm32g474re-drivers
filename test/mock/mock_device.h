/*
 * Force-included (-include) into every translation unit of the host test
 * build. Pulls in the real STM32G474 device header (with the mock core) and
 * then points every peripheral macro the library uses at a RAM instance, so
 * driver code runs unmodified against plain memory.
 */
#ifndef MOCK_DEVICE_H
#define MOCK_DEVICE_H

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"

/* X-macro list: (macro name, register-block type). */
#define MOCK_PERIPHERALS(X)                         \
    X(RCC, RCC_TypeDef)                             \
    X(PWR, PWR_TypeDef)                             \
    X(FLASH, FLASH_TypeDef)                         \
    X(GPIOA, GPIO_TypeDef) X(GPIOB, GPIO_TypeDef)   \
    X(GPIOC, GPIO_TypeDef) X(GPIOD, GPIO_TypeDef)   \
    X(GPIOE, GPIO_TypeDef) X(GPIOF, GPIO_TypeDef)   \
    X(GPIOG, GPIO_TypeDef)                          \
    X(ADC1, ADC_TypeDef) X(ADC2, ADC_TypeDef)       \
    X(ADC3, ADC_TypeDef) X(ADC4, ADC_TypeDef)       \
    X(ADC5, ADC_TypeDef)                            \
    X(ADC12_COMMON, ADC_Common_TypeDef)             \
    X(ADC345_COMMON, ADC_Common_TypeDef)            \
    X(DAC1, DAC_TypeDef) X(DAC2, DAC_TypeDef)       \
    X(DAC3, DAC_TypeDef) X(DAC4, DAC_TypeDef)       \
    X(TIM1, TIM_TypeDef) X(TIM2, TIM_TypeDef)       \
    X(TIM3, TIM_TypeDef) X(TIM4, TIM_TypeDef)       \
    X(TIM5, TIM_TypeDef) X(TIM6, TIM_TypeDef)       \
    X(TIM7, TIM_TypeDef) X(TIM8, TIM_TypeDef)       \
    X(TIM15, TIM_TypeDef) X(TIM16, TIM_TypeDef)     \
    X(TIM17, TIM_TypeDef) X(TIM20, TIM_TypeDef)     \
    X(USART1, USART_TypeDef) X(USART2, USART_TypeDef) \
    X(USART3, USART_TypeDef) X(UART4, USART_TypeDef)  \
    X(UART5, USART_TypeDef) X(LPUART1, USART_TypeDef) \
    X(EXTI, EXTI_TypeDef)                           \
    X(SYSCFG, SYSCFG_TypeDef)                       \
    X(IWDG, IWDG_TypeDef)                           \
    X(DBGMCU, DBGMCU_TypeDef)

#define MOCK_DECLARE(name, type) extern type mock_##name;
MOCK_PERIPHERALS(MOCK_DECLARE)
#undef MOCK_DECLARE

/* Redirect the CMSIS peripheral macros to the mock instances. */
#undef RCC
#define RCC (&mock_RCC)
#undef PWR
#define PWR (&mock_PWR)
#undef FLASH
#define FLASH (&mock_FLASH)
#undef GPIOA
#define GPIOA (&mock_GPIOA)
#undef GPIOB
#define GPIOB (&mock_GPIOB)
#undef GPIOC
#define GPIOC (&mock_GPIOC)
#undef GPIOD
#define GPIOD (&mock_GPIOD)
#undef GPIOE
#define GPIOE (&mock_GPIOE)
#undef GPIOF
#define GPIOF (&mock_GPIOF)
#undef GPIOG
#define GPIOG (&mock_GPIOG)
#undef ADC1
#define ADC1 (&mock_ADC1)
#undef ADC2
#define ADC2 (&mock_ADC2)
#undef ADC3
#define ADC3 (&mock_ADC3)
#undef ADC4
#define ADC4 (&mock_ADC4)
#undef ADC5
#define ADC5 (&mock_ADC5)
#undef ADC12_COMMON
#define ADC12_COMMON (&mock_ADC12_COMMON)
#undef ADC345_COMMON
#define ADC345_COMMON (&mock_ADC345_COMMON)
#undef DAC1
#define DAC1 (&mock_DAC1)
#undef DAC2
#define DAC2 (&mock_DAC2)
#undef DAC3
#define DAC3 (&mock_DAC3)
#undef DAC4
#define DAC4 (&mock_DAC4)
#undef TIM1
#define TIM1 (&mock_TIM1)
#undef TIM2
#define TIM2 (&mock_TIM2)
#undef TIM3
#define TIM3 (&mock_TIM3)
#undef TIM4
#define TIM4 (&mock_TIM4)
#undef TIM5
#define TIM5 (&mock_TIM5)
#undef TIM6
#define TIM6 (&mock_TIM6)
#undef TIM7
#define TIM7 (&mock_TIM7)
#undef TIM8
#define TIM8 (&mock_TIM8)
#undef TIM15
#define TIM15 (&mock_TIM15)
#undef TIM16
#define TIM16 (&mock_TIM16)
#undef TIM17
#define TIM17 (&mock_TIM17)
#undef TIM20
#define TIM20 (&mock_TIM20)
#undef USART1
#define USART1 (&mock_USART1)
#undef USART2
#define USART2 (&mock_USART2)
#undef USART3
#define USART3 (&mock_USART3)
#undef UART4
#define UART4 (&mock_UART4)
#undef UART5
#define UART5 (&mock_UART5)
#undef LPUART1
#define LPUART1 (&mock_LPUART1)
#undef EXTI
#define EXTI (&mock_EXTI)
#undef SYSCFG
#define SYSCFG (&mock_SYSCFG)
#undef IWDG
#define IWDG (&mock_IWDG)
#undef DBGMCU
#define DBGMCU (&mock_DBGMCU)

/* --- Test control --------------------------------------------------------- */

/** Cycles added to DWT->CYCCNT on every access (simulated passage of time). */
extern uint32_t mock_cycles_per_access;

/**
 * Called on every DWT access, i.e. inside every polling loop. Tests install
 * a function here to emulate hardware reacting to register writes (setting
 * ready flags, clearing busy bits, ...).
 */
extern void (*mock_hw_hook)(void);

/** Reset every mock register, the core state and the hooks to zero. */
void mock_reset(void);

/* panic() capture: run code that is expected to panic. */
extern jmp_buf mock_panic_jump;
extern bool    mock_panic_armed;
extern int     mock_panic_count;
extern char    mock_panic_message[128];

/** Evaluate @p stmt; evaluates to true if it called panic()/DRV_ASSERT. */
#define MOCK_PANICS(stmt)                                     \
    (__extension__({                                          \
        bool panicked_ = false;                               \
        mock_panic_armed = true;                              \
        if (setjmp(mock_panic_jump) == 0) {                   \
            stmt;                                             \
        } else {                                              \
            panicked_ = true;                                 \
        }                                                     \
        mock_panic_armed = false;                             \
        panicked_;                                            \
    }))

#endif /* MOCK_DEVICE_H */
