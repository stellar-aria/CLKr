// Copyright 2011 Emilie Gillet, 2023 Katherine Whitlock
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//         Katherine Whitlock (kate@skylinesynths.nyc)
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
// Global clock. This works as a 31-bit phase increment counter.

#pragma once
#include "avrlib/base.h"

namespace clkr {

// The resolution of the clock
// output in FAST mode
enum ClockResolution {
  CLOCK_RESOLUTION_4_PPQN,
  CLOCK_RESOLUTION_8_PPQN,
  CLOCK_RESOLUTION_24_PPQN,
  CLOCK_RESOLUTION_LAST
};

// EEPROM-stored settings
struct Options {
  ClockResolution clock_resolution;
  bool tap_tempo;
  bool locked;
  bool legacy_mode;

  // Pack the settings to be stored in EEPROM
  uint8_t pack() const {
    uint8_t byte = clock_resolution;
    if (tap_tempo) {
      byte |= 0x08;
    }
    if (locked) {
      byte |= 0x10;
    }
    if (legacy_mode) {
      byte |= 0x20;
    }
    return byte;
  }

  // Unpack the EEPROM options into the current settings
  void unpack(uint8_t byte) {
    tap_tempo = byte & 0x08;
    locked = byte & 0x10;
    legacy_mode = byte & 0x20;
    clock_resolution = static_cast<ClockResolution>(byte & 0x7);
    if (clock_resolution >= CLOCK_RESOLUTION_24_PPQN) {
      clock_resolution = CLOCK_RESOLUTION_24_PPQN;
    }
  }
};

const uint8_t kPulsesPerBeat = 24; // 24 pulses per quarter note

class Clock {
public:
  Clock() {}
  ~Clock() {}

  static inline void Init() {
    Update(120, CLOCK_RESOLUTION_24_PPQN);
    options_.locked = false;
    LoadSettings();
  }

  static void Update(uint16_t bpm, ClockResolution resolution);

  static inline void Reset() { phase_ = 0; }

  static inline void Tick() { phase_ += phase_increment_; }
  static inline void Wrap(int8_t amount) {
    LongWord *w = (LongWord *)(&phase_);
    if (amount == 0) {
      w->bytes[3] &= 0x7f;
      falling_edge_ = 0x40;
    } else {
      if (w->bytes[3] >= 128 + amount) {
        w->bytes[3] = 0;
      }
      falling_edge_ = (128 + amount) >> 1;
    }
  }

  static inline void TickClock(uint8_t num_pulses) {
    beat_ = pulse_ == 0;
    first_half_ = pulse_ < (kPulsesPerBeat / 2);
    pulse_ += num_pulses;

    // Wrap into ppqn steps.
    while (pulse_ >= kPulsesPerBeat) {
      pulse_ -= kPulsesPerBeat;
    }
  }

  static inline bool raising_edge() { return phase_ < phase_increment_; }
  static inline bool past_falling_edge() {
    LongWord w;
    w.value = phase_;
    return w.bytes[3] >= falling_edge_;
  }
  static inline bool new_pulse() {
    if (pulse_ != last_pulse_) {
      last_pulse_ = pulse_;
      return true;
    }
    return false;
  }
  static inline void Lock() { options_.locked = true; }
  static inline void Unlock() { options_.locked = false; }
  static inline bool locked() { return options_.locked; }
  static inline uint16_t bpm() { return bpm_; }

  // Options stuff
  static void SaveSettings();
  static inline bool legacy_mode() { return options_.legacy_mode; }
  static void set_legacy_mode(bool value) { options_.legacy_mode = value; }
  static inline bool tap_tempo() { return options_.tap_tempo; }
  static void set_tap_tempo(bool value) { options_.tap_tempo = value; }
  static inline ClockResolution clock_resolution() {
    return options_.clock_resolution;
  }
  static void set_clock_resolution(uint8_t value) {
    if (value >= CLOCK_RESOLUTION_24_PPQN) {
      value = CLOCK_RESOLUTION_24_PPQN;
    }
    options_.clock_resolution = static_cast<ClockResolution>(value);
  }
  static bool on_beat() { return beat_; }
  static bool on_first_half() { return first_half_; }

private:
  static void LoadSettings();
  static Options options_;
  static bool beat_;
  static bool first_half_;
  static uint8_t pulse_;
  static uint8_t last_pulse_;

  static uint16_t bpm_;
  static uint32_t phase_;
  static uint32_t phase_increment_;
  static uint8_t falling_edge_;

  DISALLOW_COPY_AND_ASSIGN(Clock);
};

extern Clock clock;

} // namespace clkr