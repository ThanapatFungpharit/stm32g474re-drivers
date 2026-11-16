#include "mock_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system/panic/panic.h"

#define MOCK_DEFINE(name, type) type mock_##name;
MOCK_PERIPHERALS(MOCK_DEFINE)
#undef MOCK_DEFINE

SysTick_Type   mock_systick;
SCB_Type       mock_scb;
DWT_Type       mock_dwt;
CoreDebug_Type mock_coredebug;
ITM_Type       mock_itm;

uint32_t mock_primask;
uint32_t mock_ipsr;
uint32_t mock_nvic_enabled[128];
uint32_t mock_nvic_priority[128];
uint32_t mock_wfi_count;

uint32_t mock_cycles_per_access = 100U;
void (*mock_hw_hook)(void);

/* CMSIS system variables normally provided by system_stm32g4xx.c. */
uint32_t SystemCoreClock = 16000000U;
const uint8_t AHBPrescTable[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9 };
const uint8_t APBPrescTable[8] = { 0, 0, 0, 0, 1, 2, 3, 4 };

void SystemInit(void) {}
void SystemCoreClockUpdate(void) {}

jmp_buf mock_panic_jump;
bool    mock_panic_armed;
int     mock_panic_count;
char    mock_panic_message[128];

DWT_Type *mock_dwt_access(void)
{
    if ((mock_dwt.CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U) {
        mock_dwt.CYCCNT += mock_cycles_per_access;
    }
    if (mock_hw_hook != NULL) {
        mock_hw_hook();
    }
    return &mock_dwt;
}

void NVIC_SystemReset(void)
{
    abort();
}

void mock_reset(void)
{
#define MOCK_CLEAR(name, type) memset((void *)&mock_##name, 0, sizeof mock_##name);
    MOCK_PERIPHERALS(MOCK_CLEAR)
#undef MOCK_CLEAR
    memset((void *)&mock_systick, 0, sizeof mock_systick);
    memset((void *)&mock_scb, 0, sizeof mock_scb);
    memset((void *)&mock_dwt, 0, sizeof mock_dwt);
    memset((void *)&mock_coredebug, 0, sizeof mock_coredebug);
    memset((void *)&mock_itm, 0, sizeof mock_itm);
    memset(mock_nvic_enabled, 0, sizeof mock_nvic_enabled);
    memset(mock_nvic_priority, 0, sizeof mock_nvic_priority);
    mock_primask = 0U;
    mock_ipsr = 0U;
    mock_wfi_count = 0U;
    mock_cycles_per_access = 100U;
    mock_hw_hook = NULL;
    mock_panic_count = 0;
    mock_panic_message[0] = '\0';
    SystemCoreClock = 16000000U;

    /* Reset values that matter to the library. */
    mock_RCC.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
    mock_RCC.CFGR = RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI;
    mock_PWR.CR1 = PWR_CR1_VOS_0;   /* Range 1 */
    mock_PWR.CR5 = PWR_CR5_R1MODE;  /* normal mode */
    mock_FLASH.ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN;
}

_Noreturn void panic_at(const char *msg, const char *file, int line)
{
    (void)file;
    (void)line;
    ++mock_panic_count;
    snprintf(mock_panic_message, sizeof mock_panic_message, "%s", (msg != NULL) ? msg : "");
    if (mock_panic_armed) {
        longjmp(mock_panic_jump, 1);
    }
    fprintf(stderr, "unexpected panic: %s\n", mock_panic_message);
    abort();
}

_Noreturn void panic(const char *msg)
{
    panic_at(msg, NULL, 0);
}

void panic_enable_fault_exceptions(void) {}
