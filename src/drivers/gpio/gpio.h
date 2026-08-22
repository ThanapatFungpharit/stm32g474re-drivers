#ifndef NUCLEO_G474RE_DRIVER_GPIO_H
#define NUCLEO_G474RE_DRIVER_GPIO_H

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "system/panic/panic.h"


/**
 * @brief Enable or disable the peripheral clock for a GPIO port.
 * @param GPIOx GPIO peripheral instance.
 * @param state Desired clock state.
 * @note Modifies RCC->AHB2ENR and reads it back after the write.
 */
void setGPIOxClock(const GPIO_TypeDef *GPIOx, EnableState state);

// == types ==

// Pin0...Pin15
typedef uint8_t Pin;

// AF0...AF15
typedef uint8_t AlternateFunction;


// == pin config ==
typedef enum {
    PINMODE_DIGITAL_INPUT  = 0b00,
    PINMODE_DIGITAL_OUTPUT = 0b01,
    PINMODE_AF             = 0b10,
    PINMODE_ANALOG         = 0b11,
} PinMode;

typedef enum {
    PINPULL_NONE = 0b00,
    PINPULL_UP   = 0b01,
    PINPULL_DOWN = 0b10,
} PinPull;

typedef enum {
    PINOTYPE_PUSHPULL  = 0,
    PINOTYPE_OPENDRAIN = 1,
} PinOType;

typedef enum {
    PINSLEW_LOW    = 0b00,
    PINSLEW_MEDIUM = 0b01,
    PINSLEW_HIGH   = 0b10,
    PINSLEW_VERY_HIGH = 0b11,
} PinSlew;


// == api ==
/**
 * @brief Set a GPIO pin's operating mode.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param mode Mode to write to the MODER register.
 * @note Calls panic() when pin is outside the range 0 to 15.
 */
void setPinMode(GPIO_TypeDef *GPIOx, Pin pin, PinMode mode);

/**
 * @brief Set a GPIO pin's alternate function.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param af Alternate-function value to write to AFR.
 * @note Calls panic() when pin is outside the range 0 to 15.
 */
void setAlternateFunction(GPIO_TypeDef *GPIOx, uint8_t pin, AlternateFunction af);

/**
 * @brief Set a GPIO pin's pull configuration.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param pull Pull configuration to write to PUPDR.
 * @note Calls panic() when pin is outside the range 0 to 15.
 */
void setPinPull(GPIO_TypeDef *GPIOx, Pin pin, PinPull pull);

/**
 * @brief Set a GPIO pin's output speed.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param slew Output speed to write to OSPEEDR.
 * @note Calls panic() when pin is outside the range 0 to 15.
 */
void setPinSlew(GPIO_TypeDef *GPIOx, Pin pin, PinSlew slew);

/**
 * @brief Set a GPIO pin's output type.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param otype Push-pull or open-drain output type.
 * @note Calls panic() when pin is outside the range 0 to 15.
 */
void setPinOType(GPIO_TypeDef *GPIOx, uint8_t pin, PinOType otype);

/**
 * @brief Set or reset a GPIO output pin.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @param value Output state to write through BSRR.
 * @note Calls panic() when pin is outside the range 0 to 15.
 */
void digitalWrite(GPIO_TypeDef *GPIOx,
                  uint8_t pin,
                  EnableState value);

/**
 * @brief Read a GPIO input pin.
 * @param GPIOx GPIO port.
 * @param pin Pin number.
 * @return STATE_ENABLE when the input is high; otherwise STATE_DISABLE.
 * @note Calls panic() when pin is outside the range 0 to 15.
 */
EnableState digitalRead(const GPIO_TypeDef *GPIOx, Pin pin);

#endif //NUCLEO_G474RE_DRIVER_GPIO_H
