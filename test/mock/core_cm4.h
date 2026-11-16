/*
 * Host replacement for CMSIS core_cm4.h, used by the unit tests only.
 *
 * The real STM32G474 device header (stm32g474xx.h) includes "core_cm4.h";
 * test/mock is searched first, so this file provides the core types,
 * intrinsics and core peripherals as plain host memory instead of
 * Cortex-M4 registers and instructions.
 */
#ifndef MOCK_CORE_CM4_H
#define MOCK_CORE_CM4_H

#include <stdint.h>

#define __I     volatile const
#define __O     volatile
#define __IO    volatile
#define __IM    volatile const
#define __OM    volatile
#define __IOM   volatile

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

/* --- Core register blocks (only the fields the library touches) ----------- */

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} SysTick_Type;

typedef struct {
    volatile uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;
    volatile uint32_t AIRCR;
    volatile uint32_t SCR;
    volatile uint32_t CCR;
    volatile uint8_t  SHP[12];
    volatile uint32_t SHCSR;
    volatile uint32_t CFSR;
    volatile uint32_t HFSR;
    volatile uint32_t DFSR;
    volatile uint32_t MMFAR;
    volatile uint32_t BFAR;
    volatile uint32_t AFSR;
} SCB_Type;

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t CYCCNT;
} DWT_Type;

typedef struct {
    volatile uint32_t DHCSR;
    volatile uint32_t DCRSR;
    volatile uint32_t DCRDR;
    volatile uint32_t DEMCR;
} CoreDebug_Type;

typedef struct {
    volatile union {
        volatile uint8_t  u8;
        volatile uint16_t u16;
        volatile uint32_t u32;
    } PORT[32];
    volatile uint32_t TER;
    volatile uint32_t TPR;
    volatile uint32_t TCR;
    volatile uint32_t LAR;
} ITM_Type;

/* --- Bit definitions ------------------------------------------------------- */

#define SysTick_CTRL_ENABLE_Msk     (1UL << 0)
#define SysTick_CTRL_TICKINT_Msk    (1UL << 1)
#define SysTick_CTRL_CLKSOURCE_Msk  (1UL << 2)
#define SysTick_LOAD_RELOAD_Msk     (0xFFFFFFUL)

#define SCB_ICSR_PENDSTSET_Msk      (1UL << 26)
#define SCB_ICSR_PENDSTCLR_Msk      (1UL << 25)
#define SCB_ICSR_VECTACTIVE_Msk     (0x1FFUL)
#define SCB_SHCSR_USGFAULTENA_Msk   (1UL << 18)
#define SCB_SHCSR_BUSFAULTENA_Msk   (1UL << 17)
#define SCB_SHCSR_MEMFAULTENA_Msk   (1UL << 16)
#define SCB_CCR_DIV_0_TRP_Msk       (1UL << 4)
#define SCB_CFSR_MMARVALID_Msk      (1UL << 7)
#define SCB_CFSR_BFARVALID_Msk      (1UL << 15)

#define DWT_CTRL_NOCYCCNT_Msk       (1UL << 25)
#define DWT_CTRL_CYCCNTENA_Msk      (1UL << 0)

#define CoreDebug_DHCSR_C_DEBUGEN_Msk (1UL << 0)
#define CoreDebug_DEMCR_TRCENA_Msk  (1UL << 24)

#define ITM_TCR_ITMENA_Msk          (1UL << 0)

/* --- Mock state ------------------------------------------------------------ */

extern SysTick_Type   mock_systick;
extern SCB_Type       mock_scb;
extern DWT_Type       mock_dwt;
extern CoreDebug_Type mock_coredebug;
extern ITM_Type       mock_itm;

extern uint32_t mock_primask;
extern uint32_t mock_ipsr;
extern uint32_t mock_nvic_enabled[128];
extern uint32_t mock_nvic_priority[128];
extern uint32_t mock_wfi_count;

DWT_Type *mock_dwt_access(void);

#define SysTick   (&mock_systick)
#define SCB       (&mock_scb)
#define DWT       (mock_dwt_access())
#define CoreDebug (&mock_coredebug)
#define ITM       (&mock_itm)

/* --- Intrinsics ------------------------------------------------------------ */

static inline uint32_t __get_PRIMASK(void) { return mock_primask; }
static inline void __disable_irq(void) { mock_primask = 1U; }
static inline void __enable_irq(void) { mock_primask = 0U; }
static inline uint32_t __get_IPSR(void) { return mock_ipsr; }
static inline void __WFI(void) { ++mock_wfi_count; }
static inline void __DSB(void) {}
static inline void __ISB(void) {}
static inline void __DMB(void) {}
#define __BKPT(value) ((void)(value))
#define __NOP() ((void)0)

/* --- NVIC / SysTick helpers -------------------------------------------------- */

static inline void NVIC_EnableIRQ(int irqn) { if (irqn >= 0) { mock_nvic_enabled[irqn] = 1U; } }
static inline void NVIC_DisableIRQ(int irqn) { if (irqn >= 0) { mock_nvic_enabled[irqn] = 0U; } }
static inline void NVIC_ClearPendingIRQ(int irqn) { (void)irqn; }
static inline void NVIC_SetPriority(int irqn, uint32_t priority)
{
    if (irqn >= 0) {
        mock_nvic_priority[irqn] = priority;
    }
}
void NVIC_SystemReset(void);

static inline uint32_t SysTick_Config(uint32_t ticks)
{
    if ((ticks - 1UL) > SysTick_LOAD_RELOAD_Msk) {
        return 1UL;
    }
    mock_systick.LOAD = ticks - 1UL;
    mock_systick.VAL = 0UL;
    mock_systick.CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
                        SysTick_CTRL_ENABLE_Msk;
    return 0UL;
}

#endif /* MOCK_CORE_CM4_H */
