#include "nucleo_g474re.h"

#include "drivers/gpio/gpio.h"


DrvStatus board_led_init(void)
{
    return gpio_init_output(BOARD_LED_PORT, BOARD_LED_PIN, false);
}

void board_led_set(const bool on)
{
    gpio_write(BOARD_LED_PORT, BOARD_LED_PIN, on);
}

void board_led_toggle(void)
{
    gpio_toggle(BOARD_LED_PORT, BOARD_LED_PIN);
}

DrvStatus board_button_init(void)
{
    return gpio_init_input(BOARD_BUTTON_PORT, BOARD_BUTTON_PIN, GPIO_PULL_DOWN);
}

bool board_button_pressed(void)
{
    return gpio_read(BOARD_BUTTON_PORT, BOARD_BUTTON_PIN);
}

DrvStatus board_vcp_init(UartHandle *uart, const uint32_t baud_rate)
{
    DrvStatus status = gpio_init_af(BOARD_VCP_TX_PORT, BOARD_VCP_TX_PIN, BOARD_VCP_AF,
                                    GPIO_OTYPE_PUSH_PULL, GPIO_PULL_NONE, GPIO_SPEED_LOW);
    if (status != DRV_OK) {
        return status;
    }

    /* Pull-up keeps the receiver idle (high) if the ST-LINK side floats. */
    status = gpio_init_af(BOARD_VCP_RX_PORT, BOARD_VCP_RX_PIN, BOARD_VCP_AF,
                          GPIO_OTYPE_PUSH_PULL, GPIO_PULL_UP, GPIO_SPEED_LOW);
    if (status != DRV_OK) {
        return status;
    }

    const UartConfig config = uart_default_config(baud_rate);
    return uart_init(uart, BOARD_VCP_UART, &config);
}
