/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BUT_CLICK_Pin GPIO_PIN_13
#define BUT_CLICK_GPIO_Port GPIOC
#define PWM_AUDIO_Pin GPIO_PIN_0
#define PWM_AUDIO_GPIO_Port GPIOA
#define SSD_RESET_Pin GPIO_PIN_2
#define SSD_RESET_GPIO_Port GPIOA
#define SSD_CS_Pin GPIO_PIN_3
#define SSD_CS_GPIO_Port GPIOA
#define SSD_DC_Pin GPIO_PIN_4
#define SSD_DC_GPIO_Port GPIOA
#define SSD_CLK_Pin GPIO_PIN_5
#define SSD_CLK_GPIO_Port GPIOA
#define SSD_MOSI_Pin GPIO_PIN_7
#define SSD_MOSI_GPIO_Port GPIOA
#define BUT_UP_Pin GPIO_PIN_0
#define BUT_UP_GPIO_Port GPIOB
#define BUT_RIGHT_Pin GPIO_PIN_1
#define BUT_RIGHT_GPIO_Port GPIOB
#define CC_IRQ0_Pin GPIO_PIN_10
#define CC_IRQ0_GPIO_Port GPIOB
#define CC_IRQ2_Pin GPIO_PIN_11
#define CC_IRQ2_GPIO_Port GPIOB
#define CC_CS_Pin GPIO_PIN_12
#define CC_CS_GPIO_Port GPIOB
#define CC_SCK_Pin GPIO_PIN_13
#define CC_SCK_GPIO_Port GPIOB
#define CC_MISO_Pin GPIO_PIN_14
#define CC_MISO_GPIO_Port GPIOB
#define CC_MOSI_Pin GPIO_PIN_15
#define CC_MOSI_GPIO_Port GPIOB
#define DIP8_Pin GPIO_PIN_11
#define DIP8_GPIO_Port GPIOA
#define DIP7_Pin GPIO_PIN_12
#define DIP7_GPIO_Port GPIOA
#define DIP6_Pin GPIO_PIN_15
#define DIP6_GPIO_Port GPIOA
#define DIP5_Pin GPIO_PIN_3
#define DIP5_GPIO_Port GPIOB
#define DIP4_Pin GPIO_PIN_4
#define DIP4_GPIO_Port GPIOB
#define DIP3_Pin GPIO_PIN_5
#define DIP3_GPIO_Port GPIOB
#define DIP2_Pin GPIO_PIN_6
#define DIP2_GPIO_Port GPIOB
#define DIP1_Pin GPIO_PIN_7
#define DIP1_GPIO_Port GPIOB
#define BUT_DOWN_Pin GPIO_PIN_8
#define BUT_DOWN_GPIO_Port GPIOB
#define BUT_LEFT_Pin GPIO_PIN_9
#define BUT_LEFT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
