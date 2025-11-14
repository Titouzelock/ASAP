#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

// Forward declarations for HAL handles defined in Core/Src/main.c.
// Exposed here so application code (e.g., src/main_detector.cpp) can use
// timers/UARTs configured by CubeMX without re-declaring them.
extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart1;

void Error_Handler(void);
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART1_UART_Init(void);
void MX_TIM2_Init(void);

// GPIO pin mappings generated from ASAP.ioc.
// These labels are the single source of truth for hardware wiring: CubeMX,
// the HAL init code, and the C++ application all agree on these pins.

// OLED (SSD1322)
#define SSD_RESET_Pin       GPIO_PIN_2
#define SSD_RESET_GPIO_Port GPIOA
#define SSD_CS_Pin          GPIO_PIN_3
#define SSD_CS_GPIO_Port    GPIOA
#define SSD_DC_Pin          GPIO_PIN_4
#define SSD_DC_GPIO_Port    GPIOA
#define SSD_CLK_Pin         GPIO_PIN_5
#define SSD_CLK_GPIO_Port   GPIOA
#define SSD_MOSI_Pin        GPIO_PIN_7
#define SSD_MOSI_GPIO_Port  GPIOA

// Buttons (digital, active low with pull-ups)
#define BUT_CLICK_Pin       GPIO_PIN_13
#define BUT_CLICK_GPIO_Port GPIOC
#define BUT_UP_Pin          GPIO_PIN_0
#define BUT_UP_GPIO_Port    GPIOB
#define BUT_RIGHT_Pin       GPIO_PIN_1
#define BUT_RIGHT_GPIO_Port GPIOB
#define BUT_DOWN_Pin        GPIO_PIN_8
#define BUT_DOWN_GPIO_Port  GPIOB
#define BUT_LEFT_Pin        GPIO_PIN_9
#define BUT_LEFT_GPIO_Port  GPIOB

// DIP switches (8-position DIP used for configuration / IDs)
#define DIP8_Pin            GPIO_PIN_11
#define DIP8_GPIO_Port      GPIOA
#define DIP7_Pin            GPIO_PIN_12
#define DIP7_GPIO_Port      GPIOA
#define DIP6_Pin            GPIO_PIN_15
#define DIP6_GPIO_Port      GPIOA
#define DIP5_Pin            GPIO_PIN_3
#define DIP5_GPIO_Port      GPIOB
#define DIP4_Pin            GPIO_PIN_4
#define DIP4_GPIO_Port      GPIOB
#define DIP3_Pin            GPIO_PIN_5
#define DIP3_GPIO_Port      GPIOB
#define DIP2_Pin            GPIO_PIN_6
#define DIP2_GPIO_Port      GPIOB
#define DIP1_Pin            GPIO_PIN_7
#define DIP1_GPIO_Port      GPIOB

// CC1101 RF module (SPI + IRQs)
#define CC_CS_Pin           GPIO_PIN_12
#define CC_CS_GPIO_Port     GPIOB
#define CC_SCK_Pin          GPIO_PIN_13
#define CC_SCK_GPIO_Port    GPIOB
#define CC_MISO_Pin         GPIO_PIN_14
#define CC_MISO_GPIO_Port   GPIOB
#define CC_MOSI_Pin         GPIO_PIN_15
#define CC_MOSI_GPIO_Port   GPIOB
#define CC_IRQ0_Pin         GPIO_PIN_10
#define CC_IRQ0_GPIO_Port   GPIOB
#define CC_IRQ2_Pin         GPIO_PIN_11
#define CC_IRQ2_GPIO_Port   GPIOB

// PWM audio (TIM2 CH1 on PA0)
#define PWM_AUDIO_Pin       GPIO_PIN_0
#define PWM_AUDIO_GPIO_Port GPIOA

#ifdef __cplusplus
}
#endif
