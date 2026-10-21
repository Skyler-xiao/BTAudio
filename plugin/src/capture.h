/*
 *   System audio capture for the Bluetooth audio plugin (hooks the final mix of the console's audio engine)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace capture {

// usual rate of the console's audio engine (some applications run it at 32 kHz)
constexpr uint32_t SAMPLE_RATE = 48000;

// which mix is sent to the headphones
enum {
    SOURCE_DRC  = 0, // GamePad (has the system menu's sounds)
    SOURCE_TV   = 1,
    SOURCE_AUTO = 2, // TV, the GamePad while the TV is silent
};
void setSource(int source);

// rate of the samples in the ring at the moment
uint32_t sampleRate();

// while not active the callbacks do nothing but forward to the application's own callback
void setActive(bool active);

// number of stereo frames waiting
size_t available();

// copy up to maxFrames stereo frames (interleaved 16 bit) out of the ring, returns the number of frames
size_t read(int16_t* out, size_t maxFrames);

// forget everything about the previous application (call at application start)
void reset();

// call regularly: installs the hooks once the audio engine runs
void poll();

// one line of counters for the log
void describe(char* out, size_t size);

// HOME Menu engine: how often it started, how many mix callbacks it delivered, whether ours is in
void homeStats(uint32_t *inits, uint32_t *calls, bool *installed);

// the HOME Menu delivers the sound at the moment
bool homeMenuActive();

// throw away the oldest frames until only keepFrames are left
void trim(size_t keepFrames);

} // namespace capture
