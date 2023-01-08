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
// Running average helper class

#pragma once
#include "avrlib/base.h"
#include <stdint.h>

/**
 * @brief Rate-dependent hysterisis system using
 *        running averages in a circular buffer
 *
 * @tparam length The number of values to average (here max 255), aka the amount
 *                of smoothing
 */
template <uint8_t length> class RunningAverage {
  uint8_t history[length] = {0};
  decltype(length) history_idx = 0;
  uint16_t total = 0;

public:
  RunningAverage() {}
  ~RunningAverage(){}
  
  /**
   * @brief Add a new value to our running average
   * 
   * @param new_value the value to add
   */
  void push(uint8_t new_value) {
    this->total -= history[history_idx]; // subtract the oldest value
    history[history_idx] = new_value;   // place the new value
    this->total += history[history_idx]; // calculate the new total

    // circular index
    history_idx += 1;
    if (history_idx >= length) {
      history_idx = 0;
    }
  }

  /**
   * @brief Fetch the current running average
   * 
   * guarantee: despite working with a uint16_t, the average should never
   * be able to go above the maximum width of the input value (uint8_t) */
  uint8_t get() { return static_cast<uint8_t>(this->total / length); }

  inline uint8_t push_and_get(uint8_t new_value) {
    this->push(new_value);
    return this->get();
  }
  DISALLOW_COPY_AND_ASSIGN(RunningAverage);
};
