// Copyright 2023 Katherine Whitlock
//
// Author: Katherine Whitlock (kate@skylinesynths.nyc)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// LED helper functions

#pragma once
#include <stdint.h>
#include <avr/io.h>
#include "hardware_config.h"

namespace clkr {
enum LEDs {
  LED_CLOCK, // PD5
  LED_PAUSE  // PD6
};

enum LedBrightness {
  BRIGHTNESS_NONE = 0x00,
  BRIGHTNESS_HALF = 0x7F,
  BRIGHTNESS_FULL = 0xFF
};

inline void PWMOff(LEDs led) {
  switch (led) {
  case LED_PAUSE:
    TCCR0A &= ~_BV(COM0A1);
    break;
  case LED_CLOCK:
    TCCR0A &= ~_BV(COM0B1);
    break;
  }
}

inline void PWMOn(LEDs led) {
  switch (led) {
  case LED_PAUSE:
    TCCR0A |= _BV(COM0A1);
    break;
  case LED_CLOCK:
    TCCR0A |= _BV(COM0B1);
    break;
  }
}

inline void LedSetBrightness(LEDs led, uint8_t brightness) {
  if (brightness <= 0) {
    PWMOff(led);
  } else {
    PWMOn(led);
    switch (led) {
    case LED_PAUSE:
      OCR0A = brightness;
      break;
    case LED_CLOCK:
      OCR0B = brightness;
      break;
    }
  }
}

void LedDance();
void FadeLeds();
}