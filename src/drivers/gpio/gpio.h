#ifndef NUCLEO_G474RE_DRIVER_GPIO_H
#define NUCLEO_G474RE_DRIVER_GPIO_H

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "system/panic/panic.h"


/** @brief Get a GPIO peripheral's RCC clock mask. @param GPIOx GPIO port. @return RCC clock-enable mask. */
uint32_t getGPIOxClock(const GPIO_TypeDef *GPIOx);

/** @brief Set a GPIO peripheral clock state. @param GPIOx GPIO port. @param state Desired clock state. */
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
/** @brief Set a GPIO pin's operating mode. @param GPIOx GPIO port. @param pin Pin number. @param mode Pin mode. */
void setPinMode(GPIO_TypeDef *GPIOx, Pin pin, PinMode mode);

/** @brief Set a GPIO pin's pull configuration. @param GPIOx GPIO port. @param pin Pin number. @param pull Pull configuration. */
void setPinPull(GPIO_TypeDef *GPIOx, Pin pin, PinPull pull);

/** @brief Set a GPIO pin's output speed. @param GPIOx GPIO port. @param pin Pin number. @param slew Output speed. */
void setPinSlew(GPIO_TypeDef *GPIOx, Pin pin, PinSlew slew);

/** @brief Set a GPIO pin's output type. @param GPIOx GPIO port. @param pin Pin number. @param otype Output type. */
void setPinOType(GPIO_TypeDef *GPIOx, uint8_t pin, PinOType otype);

/** @brief Set or reset a GPIO output pin. @param GPIOx GPIO port. @param pin Pin number. @param value Output state. */
void digitalWrite(GPIO_TypeDef *GPIOx,
                  uint8_t pin,
                  EnableState value);

/** @brief Read a GPIO input pin. @param GPIOx GPIO port. @param pin Pin number. @return Pin state. */
EnableState digitalRead(const GPIO_TypeDef *GPIOx, Pin pin);

#endif //NUCLEO_G474RE_DRIVER_GPIO_H
