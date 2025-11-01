#pragma once

#include <stdint.h>

namespace asap::input {

// High-level joystick actions consumed by UI/menu code.
// Click should represent a press-then-release edge.
enum class JoyAction : uint8_t {
  Neutral = 0,
  Left,
  Right,
  Up,
  Down,
  Click,
};

}  // namespace asap::input

