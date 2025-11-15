#include "stm32f1xx_hal.h"
#include "main.h"

// Detector entrypoint for the STM32Cube (HAL) build.
// This file replaces the old Arduino-style setup()/loop() with:
// - HAL init and CubeMX-generated system/peripheral setup
// - A simple heartbeat loop that drives the UI controller
// - Button sampling mapped to JoyAction so native and embedded UI stay in sync

#include <asap/display/DetectorDisplay.h>  // SSD1322 display driver abstraction
#include <asap/input/Joystick.h>          // High-level JoyAction enum
#include <asap/ui/UIController.h>         // UI state machine
#include <asap/audio/AudioEngine.h>       // MCU audio engine (geiger + beeps)

extern "C"
{
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART1_UART_Init(void);
void MX_TIM2_Init(void);
void MX_TIM3_Init(void);

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
}

using asap::display::DetectorDisplay;
using asap::display::DisplayPins;

namespace
{

// How often the UI controller is ticked.
constexpr uint32_t kHeartbeatIntervalMs = 250;

// SPI / GPIO wiring for the SSD1322 module on the detector PCB.
// Pins follow ASAP.ioc labels and Core/Src/main.c GPIO init:
// PA2 -> SSD_RESET, PA3 -> SSD_CS, PA4 -> SSD_DC.
constexpr DisplayPins kDisplayPins = {
    /*chipSelect*/ SSD_CS_Pin,
    /*dataCommand*/ SSD_DC_Pin,
    /*reset*/ SSD_RESET_Pin,
};

// Global display + UI controller. The UIController only depends on the
// abstract DetectorDisplay API, so the same state machine runs on native
// and embedded builds.
DetectorDisplay detectorDisplay(kDisplayPins);
asap::ui::UIController ui(detectorDisplay);
uint32_t lastHeartbeat = 0;  // last time the heartbeat ran


// Sample buttons (UP/DOWN/LEFT/RIGHT/CLICK) and map to JoyAction.
// Buttons are configured as GPIO inputs with pull-ups in MX_GPIO_Init.
asap::input::JoyAction sampleButtons()
{
  if (HAL_GPIO_ReadPin(BUT_LEFT_GPIO_Port, BUT_LEFT_Pin) == GPIO_PIN_RESET)
  {
    return asap::input::JoyAction::Left;
  }
  if (HAL_GPIO_ReadPin(BUT_RIGHT_GPIO_Port, BUT_RIGHT_Pin) == GPIO_PIN_RESET)
  {
    return asap::input::JoyAction::Right;
  }
  if (HAL_GPIO_ReadPin(BUT_UP_GPIO_Port, BUT_UP_Pin) == GPIO_PIN_RESET)
  {
    return asap::input::JoyAction::Up;
  }
  if (HAL_GPIO_ReadPin(BUT_DOWN_GPIO_Port, BUT_DOWN_Pin) == GPIO_PIN_RESET)
  {
    return asap::input::JoyAction::Down;
  }
  if (HAL_GPIO_ReadPin(BUT_CLICK_GPIO_Port, BUT_CLICK_Pin) == GPIO_PIN_RESET)
  {
    return asap::input::JoyAction::Click;
  }
  return asap::input::JoyAction::Neutral;
}

}  // namespace

// HAL-based application entry point.
// HAL-based application entry point.
int main(void)
{
  // Bring up the HAL, system clock and all GPIO/UART/TIM peripherals
  // configured in CubeMX. The MX_* functions are defined in Core/Src/main.c.
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();

  // Initialize the MCU-style audio engine and start the timers used for audio
  // PWM (TIM2) and the 8 kHz sample clock (TIM3).
  asap_audio_init();
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_Base_Start_IT(&htim3);

  if (detectorDisplay.begin())
  {
    detectorDisplay.drawBootScreen(ASAP_VERSION);
  }

  lastHeartbeat = HAL_GetTick();

  // Main heartbeat loop: periodically sample input and tick the UI.
  while (true)
  {
    const uint32_t now = HAL_GetTick();
    if (now - lastHeartbeat >= kHeartbeatIntervalMs)
    {
      lastHeartbeat = now;

      // Center button is used for the long-press "enter menu" gesture.
      const bool centerDown =
          (HAL_GPIO_ReadPin(BUT_CLICK_GPIO_Port, BUT_CLICK_Pin) == GPIO_PIN_RESET);
      const asap::input::JoyAction action = sampleButtons();
      ui.onTick(now, {centerDown, action});
    }
  }
}
