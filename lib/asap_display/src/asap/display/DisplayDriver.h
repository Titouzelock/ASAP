#pragma once

#include "asap/display/DisplayTypes.h"

#ifdef ARDUINO
#include "asap/display/DetectorDisplay.h"
#else
#include "asap/display/NativeDisplay.h"

namespace asap::display
{
using DetectorDisplay = NativeDisplay;
}

#endif  // ARDUINO

