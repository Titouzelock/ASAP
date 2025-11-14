#include "stm32f1xx_hal.h"
#include "main.h"

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();

  for (;;)
  {
    HAL_Delay(1500);
    // Placeholder: anomaly behavior to be implemented.
  }
}
