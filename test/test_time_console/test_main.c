/* Host tests: time base, timeouts, console formatting and output routing. */
#include <string.h>
#include <unity.h>

#include "drivers/common.h"
#include "system/console/console.h"
#include "system/time/time.h"

static char captured[128];
static size_t captured_len;

static size_t capture(void *ctx, const char *data, size_t len)
{
    (void)ctx;
    for (size_t i = 0U; (i < len) && (captured_len < (sizeof captured - 1U)); ++i) {
        captured[captured_len++] = data[i];
    }
    captured[captured_len] = '\0';
    return len;
}

void setUp(void)
{
    mock_reset();
    captured_len = 0U;
    captured[0] = '\0';
    console_set_output(NULL, NULL);
}

void tearDown(void) {}

/* --- Time ----------------------------------------------------------------------- */

static void test_us_to_cycles_rounds_up(void)
{
    TEST_ASSERT_EQUAL_UINT64(170U, time_us_to_cycles(1U, 170000000U));
    TEST_ASSERT_EQUAL_UINT64(16U, time_us_to_cycles(1U, 16000000U));
    TEST_ASSERT_EQUAL_UINT64(1U, time_us_to_cycles(1U, 32768U)); /* 0.03 cycles -> 1 */
    TEST_ASSERT_EQUAL_UINT64(0U, time_us_to_cycles(0U, 170000000U));
    /* Largest argument: no overflow (170 cycles per µs exactly). */
    TEST_ASSERT_EQUAL_UINT64(730144440150ULL, time_us_to_cycles(UINT32_MAX, 170000000U));
}

static void test_timeouts_expire_and_forever_does_not(void)
{
    SystemCoreClock = 1000000U; /* 1 cycle per µs; each check advances 100 cycles */
    TimeTimeout t;

    time_timeout_start_us(&t, 1000U);
    unsigned checks = 0U;
    while (!time_timeout_expired(&t)) {
        ++checks;
        TEST_ASSERT_TRUE(checks < 100U);
    }
    /* Each check advances the mock cycle counter by 100-200 cycles. */
    TEST_ASSERT_TRUE((checks >= 3U) && (checks <= 12U));

    time_timeout_start_us(&t, 0U);
    TEST_ASSERT_TRUE(time_timeout_expired(&t));

    time_timeout_start_ms(&t, UINT32_MAX);
    for (unsigned i = 0U; i < 10000U; ++i) {
        TEST_ASSERT_FALSE(time_timeout_expired(&t));
    }
}

static void test_timeout_survives_counter_wrap(void)
{
    SystemCoreClock = 1000000U;
    mock_dwt.CTRL = DWT_CTRL_CYCCNTENA_Msk;
    mock_dwt.CYCCNT = 0xFFFFFF00U;

    TimeTimeout t;
    time_timeout_start_us(&t, 1000U);
    unsigned checks = 0U;
    while (!time_timeout_expired(&t)) {
        ++checks;
    }
    /* Each check advances the mock cycle counter by 100-200 cycles. */
    TEST_ASSERT_TRUE((checks >= 3U) && (checks <= 12U));
}

static void test_millis_and_micros(void)
{
    SystemCoreClock = 170000000U;
    TEST_ASSERT_EQUAL_UINT64(0U, time_millis64()); /* before init */

    time_init();
    TEST_ASSERT_TRUE(time_is_initialized());
    TEST_ASSERT_EQUAL_UINT32(169999U, mock_systick.LOAD);

    for (int i = 0; i < 5; ++i) {
        SysTick_Handler();
    }
    mock_systick.VAL = 85000U - 1U; /* half-way through the period */
    TEST_ASSERT_EQUAL_UINT32(5U, time_millis());
    TEST_ASSERT_EQUAL_UINT32(5500U, time_micros());

    /* A tick that is pending (wrapped while masked) is counted. */
    mock_scb.ICSR = SCB_ICSR_PENDSTSET_Msk;
    mock_systick.VAL = 169000U;
    TEST_ASSERT_EQUAL_UINT32(6U, time_millis());
    TEST_ASSERT_EQUAL_UINT32(6005U, time_micros());
    TEST_ASSERT_EQUAL_UINT32(0U, mock_primask); /* interrupts restored */
}

static void test_clock_changed_is_monotonic_and_reprograms(void)
{
    SystemCoreClock = 16000000U;
    time_init();
    SysTick_Handler();
    mock_systick.VAL = 0U; /* end of the period */
    const uint64_t before = time_micros64();

    SystemCoreClock = 170000000U;
    time_clock_changed();
    mock_systick.VAL = 169999U;

    TEST_ASSERT_EQUAL_UINT32(169999U, mock_systick.LOAD);
    TEST_ASSERT_TRUE(time_micros64() >= before);
}

static void test_delay_ms_sleeps_only_when_interrupts_can_run(void)
{
    SystemCoreClock = 1000000U;

    /* Before time_init(): busy wait, no WFI. */
    time_delay_ms(1U);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_wfi_count);

    /* With interrupts masked after init: also busy wait. */
    time_init();
    mock_primask = 1U;
    time_delay_ms(1U);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_wfi_count);
    mock_primask = 0U;
}

/* --- Console -------------------------------------------------------------------- */

static void test_number_formatting(void)
{
    char buf[12];

    TEST_ASSERT_EQUAL_UINT32(1U, console_format_udec(0U, buf));
    TEST_ASSERT_EQUAL_STRING("0", buf);
    TEST_ASSERT_EQUAL_UINT32(10U, console_format_udec(4294967295U, buf));
    TEST_ASSERT_EQUAL_STRING("4294967295", buf);

    TEST_ASSERT_EQUAL_UINT32(8U, console_format_hex(0xDEADBEEFU, 2U, buf));
    TEST_ASSERT_EQUAL_STRING("DEADBEEF", buf);
    TEST_ASSERT_EQUAL_UINT32(4U, console_format_hex(0x2AU, 4U, buf));
    TEST_ASSERT_EQUAL_STRING("002A", buf);
    TEST_ASSERT_EQUAL_UINT32(1U, console_format_hex(0U, 0U, buf));
    TEST_ASSERT_EQUAL_STRING("0", buf);
    TEST_ASSERT_EQUAL_UINT32(8U, console_format_hex(1U, 99U, buf));
}

static void test_output_redirection(void)
{
    console_set_output(capture, NULL);
    console_puts("v=");
    console_put_dec(-42);
    console_putc(' ');
    console_put_dec(INT32_MIN);
    console_puts(" 0x");
    console_put_hex(0xABU, 4U);
    console_puts(NULL);
    TEST_ASSERT_EQUAL_STRING("v=-42 -2147483648 0x00AB", captured);
    TEST_ASSERT_TRUE(console_is_ready());
}

static void test_itm_without_debugger_drops_output(void)
{
    console_init(); /* C_DEBUGEN clear: ITM stays off */
    TEST_ASSERT_EQUAL_HEX32(0U, mock_itm.TCR);
    TEST_ASSERT_FALSE(console_is_ready());
    TEST_ASSERT_EQUAL_UINT32(0U, console_write("abc", 3U));
}

static void test_itm_with_debugger(void)
{
    mock_coredebug.DHCSR = CoreDebug_DHCSR_C_DEBUGEN_Msk;
    console_init();
    TEST_ASSERT_BITS(ITM_TCR_ITMENA_Msk, ITM_TCR_ITMENA_Msk, mock_itm.TCR);

    mock_itm.PORT[0].u32 = 1U; /* FIFO ready */
    TEST_ASSERT_EQUAL_UINT32(3U, console_write("xyz", 3U));
    TEST_ASSERT_EQUAL_UINT8('z', mock_itm.PORT[0].u8);

    /* FIFO never drains: the write gives up instead of hanging. */
    mock_itm.PORT[0].u32 = 0U;
    TEST_ASSERT_EQUAL_UINT32(0U, console_write("q", 1U));
    TEST_ASSERT_EQUAL_UINT32(0U, console_write("q", 1U)); /* stalled: immediate */
}

static void test_status_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("DRV_OK", drv_status_str(DRV_OK));
    TEST_ASSERT_EQUAL_STRING("DRV_ERR_TIMEOUT", drv_status_str(DRV_ERR_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("DRV_ERR_OVERRUN", drv_status_str(DRV_ERR_OVERRUN));
    TEST_ASSERT_EQUAL_STRING("DRV_ERR_?", drv_status_str((DrvStatus)99));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_us_to_cycles_rounds_up);
    RUN_TEST(test_timeouts_expire_and_forever_does_not);
    RUN_TEST(test_timeout_survives_counter_wrap);
    RUN_TEST(test_millis_and_micros);
    RUN_TEST(test_clock_changed_is_monotonic_and_reprograms);
    RUN_TEST(test_delay_ms_sleeps_only_when_interrupts_can_run);
    RUN_TEST(test_number_formatting);
    RUN_TEST(test_output_redirection);
    RUN_TEST(test_itm_without_debugger_drops_output);
    RUN_TEST(test_itm_with_debugger);
    RUN_TEST(test_status_strings);
    return UNITY_END();
}
