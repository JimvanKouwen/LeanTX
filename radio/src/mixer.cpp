/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "edgetx.h"
#include "edgetx_types.h"
#include "timers.h"
#include "switches.h"
#include "input_mapping.h"
#include "mixes.h"
#include "tasks/mixer_task.h"

#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "hal/audio_driver.h"

#if defined(LUMINOSITY_SENSOR)
#include "luminosity_sensor.h"
#endif

#if defined(RADIO_GX12)
#include "targets/taranis/gx12/bsp_io.h"
#endif

uint8_t s_mixer_first_run_done = false;

int32_t chans[MAX_OUTPUT_CHANNELS] = {0};

BeepANACenter bpanaCenter = 0;

int16_t calibratedAnalogs[MAX_ANALOG_INPUTS];
int16_t channelOutputs[MAX_OUTPUT_CHANNELS] = {0};
int16_t ex_chans[MAX_OUTPUT_CHANNELS] = {0}; // Mapped values from the last mixer evaluation;

static const getvalue_t _switch_2pos_lookup[] = {
  -1024, // SWITCH_HW_UP
  +1024, // SWITCH_HW_MID
  +1024, // SWITCH_HW_DOWN
};

static const getvalue_t _switch_3pos_lookup[] = {
  -1024, // SWITCH_HW_UP
  0,     // SWITCH_HW_MID
  +1024, // SWITCH_HW_DOWN
};

// TODO same naming convention than the drawSource
// *valid added to return status to Lua for invalid sources
getvalue_t _getValue(mixsrc_t i, bool* valid)
{
  if (i == MIXSRC_NONE) {
    if (valid != nullptr) *valid = false;
    return 0;
  }

  else if (i <= MIXSRC_LAST_STICK) {
    i -= MIXSRC_FIRST_STICK;
    if (i >= adcGetMaxInputs(ADC_INPUT_MAIN)) {
      if (valid != nullptr) *valid = false;
      return 0;
    }
    return calibratedAnalogs[inputMappingConvertMode(i)];
  }
  else if (i <= MIXSRC_LAST_POT) {
    i -= MIXSRC_FIRST_POT;
    if (i >= adcGetMaxInputs(ADC_INPUT_FLEX)) {
      if (valid != nullptr) *valid = false;
      return 0;
    }
    return calibratedAnalogs[i + adcGetInputOffset(ADC_INPUT_FLEX)];
  }

#if defined(IMU)
  else if (i == MIXSRC_TILT_X) {
    return gyroScaledX();
  }
  else if (i == MIXSRC_TILT_Y) {
    return gyroScaledY();
  }
#endif

#if defined(PCBHORUS)
  else if (i >= MIXSRC_FIRST_SPACEMOUSE && i <= MIXSRC_LAST_SPACEMOUSE) {
#if defined(SPACEMOUSE)
    return get_spacemouse_value(i - MIXSRC_FIRST_SPACEMOUSE);
#else
    return 0;
#endif
  }
#endif

  else if (i == MIXSRC_MIN) {
    return -RESX;
  }
  else if (i == MIXSRC_MAX) {
    return RESX;
  }

#if defined(LUMINOSITY_SENSOR)
  else if (i == MIXSRC_LIGHT) {
    return getLuxSensorValue() - RESX;
  }
#endif

  else if (i < MIXSRC_FIRST_SWITCH) {
    if (valid != nullptr) *valid = false;
    return 0; // Reserved source IDs from the removed trim-value system.
  }
  else if (i >= MIXSRC_FIRST_SWITCH && i <= MIXSRC_LAST_SWITCH) {
    auto sw_idx = (uint8_t)(i - MIXSRC_FIRST_SWITCH);
#if defined(FUNCTION_SWITCHES)
    auto max_switches = switchGetMaxSwitches();
    if (sw_idx < max_switches && switchIsCustomSwitch(sw_idx)) {
      return _switch_2pos_lookup[g_model.cfsState(sw_idx)];
    }
#endif
    auto sw_cfg = g_model.getSwitchType(sw_idx);
    switch(sw_cfg) {
    default:
      if (valid != nullptr) *valid = false;
      return 0;
    case SWITCH_TOGGLE:
    case SWITCH_2POS:
      return _switch_2pos_lookup[switchGetPosition(sw_idx)];
    case SWITCH_3POS:
      return _switch_3pos_lookup[switchGetPosition(sw_idx)];
    }
  }
#if defined(FUNCTION_SWITCHES)
    else if (i <= MIXSRC_LAST_CUSTOMSWITCH_GROUP) {
      uint8_t group_idx = (uint8_t)(i - MIXSRC_FIRST_CUSTOMSWITCH_GROUP + 1);
      uint8_t stepcount = getSwitchCountInFSGroup(group_idx);
      if (stepcount == 0)
        return 0;

      if (g_model.cfsGroupAlwaysOn(group_idx))
        stepcount--;

      int stepsize = (2 * RESX) / stepcount;
      int value = -RESX;

      for (uint8_t i =  0; i < switchGetMaxSwitches(); i++) {
        if (switchIsCustomSwitch(i)) {
          if (g_model.getSwitchGroup(i) == group_idx) {
            if (g_model.cfsState(i) == 1)
              return value + (g_model.cfsGroupAlwaysOn(group_idx) ? 0 : stepsize);
            else
              value += stepsize;
          }
        }
      }
      return -RESX;
  }
#endif

   else if (i <= MIXSRC_LAST_CH) {
    return ex_chans[i - MIXSRC_FIRST_CH];
  }

  else if (i == MIXSRC_TX_VOLTAGE) {
    return g_vbat100mV;
  } else if (i < MIXSRC_FIRST_TIMER) {
    // TX_TIME + SPARES
#if defined(RTCLOCK)
    return (g_rtcTime % SECS_PER_DAY) / 60; // number of minutes from midnight
#else
    if (valid != nullptr) *valid = false;
    return 0;
#endif
  } else if (i <= MIXSRC_LAST_TIMER) {
    return timersStates[i - MIXSRC_FIRST_TIMER].val;
  }

  // Shared lookup for Lua/UI; ordinary Mixes reject these IDs.
  else if (i <= MIXSRC_LAST_TELEM) {
    if (IS_FAI_FORBIDDEN(i)) {
      if (valid != nullptr) *valid = false;
      return 0;
    }
    i -= MIXSRC_FIRST_TELEM;
    div_t qr = div((uint16_t)i, 3);
    TelemetryItem & telemetryItem = telemetryItems[qr.quot];
    switch (qr.rem) {
      case 1:
        return telemetryItem.valueMin;
      case 2:
        return telemetryItem.valueMax;
      default:
        return telemetryItem.value;
    }
  }

  if (valid != nullptr) *valid = false;
  return 0;
}

// TODO: move to analogs.cpp
void evalAnalogControls(uint8_t mode)
{
  BeepANACenter anaCenter = 0;

#if defined(STICK_DEAD_ZONE)
  int16_t deadZoneOffset = g_eeGeneral.stickDeadZone ? 2 << (g_eeGeneral.stickDeadZone - 1) : 0;
#endif

  auto max_calib_analogs = adcGetInputOffset(ADC_INPUT_VBAT);
  auto pots_offset = adcGetInputOffset(ADC_INPUT_FLEX);

  for (uint8_t i = 0; i < max_calib_analogs; i++) {
    int16_t v = anaIn(i);
    uint8_t ch = (i < pots_offset ? inputMappingConvertMode(i) : i);

    // [0..2048] -> [-1024..1024]
    v -= RESX;

#if defined(STICK_DEAD_ZONE)
    // dead zone invented by FlySky in my opinion it should goes into ADC
    if (g_eeGeneral.stickDeadZone && ch != inputMappingGetThrottle()) {
      if (v > deadZoneOffset) {
        // y=ax+b
        v = (int16_t)((int32_t)(v - deadZoneOffset) * 1024L / (1024L - deadZoneOffset));
      } else if (v < -deadZoneOffset) {
        // y=ax+b
        v = (int16_t)((int32_t)(v + deadZoneOffset) * 1024L / (1024L - deadZoneOffset));
      } else {
        v = 0;
      }
    }
#endif

    if (g_model.throttleReversed && ch == inputMappingGetThrottle()) {
      v = -v;
    }

    BeepANACenter mask = (BeepANACenter)1 << ch; // TODO

    calibratedAnalogs[i] = v;

    // filtering for center beep
    uint8_t tmp = (uint16_t)abs(v) / 16;
    if (mode == e_perout_mode_normal) {
      if (tmp==0 || (tmp==1 && (bpanaCenter & mask))) {
        anaCenter |= mask;
        if ((g_model.beepANACenter & mask) && !(bpanaCenter & mask) &&
            s_mixer_first_run_done && !menuCalibrationState) {
          if (i < pots_offset || IS_POT_SLIDER_AVAILABLE(i - pots_offset)) {
            AUDIO_POT_MIDDLE(i);
          }
        }
      }
    }

  }

  if (mode == e_perout_mode_normal) {
    bpanaCenter = anaCenter;
  }
}

getvalue_t getValue(mixsrc_t i, bool* valid)
{
  bool invert = false;
  if (i < 0) {
    invert = true;
    i = -i;
  }
  getvalue_t v = _getValue(i, valid);
  if (invert) v = -v;
  return v;
}

constexpr bitfield_channels_t all_channels_dirty = (bitfield_channels_t)-1;

static inline bitfield_channels_t channel_bit(uint16_t ch)
{
  return (bitfield_channels_t)1 << ch;
}

static inline bitfield_channels_t channel_dirty(bitfield_channels_t mask, uint16_t ch)
{
  return mask & channel_bit(ch);
}

static inline bitfield_channels_t upper_channels_mask(uint16_t ch)
{
  // take the 1's complement to generate a bit pattern
  // that has all bits of 'ch' order and above set
  //
  // Examples (mask for max 8 channels):
  // - channel 0: 0b11111111
  // - channel 1: 0b11111110
  // - channel 2: 0b11111100

  return ~(channel_bit(ch)) + 1;
}

void evalChannelMixes(uint8_t mode)
{
  evalAnalogControls(mode);

  memclear(chans, sizeof(chans)); // all outputs to 0

  //========== MIXER LOOP ===============

  uint8_t pass = 0;
  bitfield_channels_t dirtyChannels = all_channels_dirty;

  do {
    bitfield_channels_t passDirtyChannels = 0;

    for (uint8_t i=0; i<MAX_MIXERS; i++) {

      MixData * md = mixAddress(i);
      mixsrc_t srcRaw = md->srcRaw;
      mixsrc_t srcRawAbs = abs(srcRaw);

      if (srcRaw == 0) {
#if defined(COLORLCD)
        continue;
#else
        break;
#endif
      }

      if (!channel_dirty(dirtyChannels, md->destCh))
        continue;

      // if this is the first calculation for the destination channel,
      // initialize it with 0 (otherwise would be random)
      if (i == 0 || md->destCh != (md - 1)->destCh)
        chans[md->destCh] = 0;

      // Reject non-control sources even when supplied by storage or Lua.
      if (!isMixerSource(srcRaw)) continue;

      //========== VALUE ===============
      getvalue_t v = 0;

      if (mode != e_perout_mode_normal) {
        v = getValue(srcRaw);
      } else {
        v = getValue(srcRaw);

        if (srcRawAbs >= MIXSRC_FIRST_CH && srcRawAbs <= MIXSRC_LAST_CH) {

          auto srcChan = srcRawAbs - MIXSRC_FIRST_CH;
          if (srcChan < MAX_OUTPUT_CHANNELS && md->destCh != srcChan) {

            // check whether we need to recompute the current channel later
            bitfield_channels_t upperChansMask = upper_channels_mask(md->destCh);
            bitfield_channels_t srcChanDirtyMask = channel_dirty(dirtyChannels, srcChan);

            // if the source is any of the channels marked as dirty
            // or contained in [ destCh, MAX_OUTPUT_CHANNELS [
            if (srcChanDirtyMask & (passDirtyChannels | upperChansMask)) {
              passDirtyChannels |= channel_bit(md->destCh);
            }

            // if the source has already be computed,
            // then use it!
            if (srcChan < md->destCh || pass > 0) {
              // channels are in [ -1024 * 256, 1024 * 256 ]
              v = (chans[srcChan] >> 8) * (srcRaw < 0 ? -1 : 1);
            }
          }
        }
      }

      int32_t weight = (10 * limit<int16_t>(-RESX, md->weight, RESX));
      weight = calc100to256_16Bits(weight);

      //========== WEIGHT ===============
      int32_t dv = (int32_t)v * weight;
      dv = divRoundClosest(dv, 10);

      //========== OFFSET / AFTER ===============
      {
        int32_t offset = (10 * limit<int16_t>(-RESX, md->offset, RESX));
        if (offset) dv += divRoundClosest(calc100toRESX_16Bits(offset), 10) << 8;
      }

      // Every mapping contributes numerically to its destination.
      chans[md->destCh] += dv;
    } //endfor mixers

    dirtyChannels &= passDirtyChannels;

  } while (++pass < 5 && dirtyChannels);

}

void evalMixes()
{
#if defined(RADIO_GX12)
  // see #6159
  _poll_switches();
#endif
  evalChannelMixes(e_perout_mode_normal);

  for (uint8_t i = 0; i < MAX_OUTPUT_CHANNELS; ++i) {
    // Convert the mixer's fixed-point representation to channel units only.
    ex_chans[i] = chans[i] / 256;
    channelOutputs[i] = ex_chans[i];
  }
}

#if defined(THRTRACE)
uint8_t  s_traceBuf[MAXTRACE];
uint16_t s_traceWr;
uint8_t  s_cnt_10s;
uint16_t s_cnt_samples_thr_10s;
uint16_t s_sum_samples_thr_10s;
#endif

void doMixerPeriodicUpdates()
{
  static tmr10ms_t lastTMR = 0;

  tmr10ms_t tmr10ms = get_tmr10ms();

  uint8_t tick10ms = (tmr10ms >= lastTMR ? tmr10ms - lastTMR : 1);
  // handle tick10ms overrun
  // correct overflow handling costs a lot of code; happens only each 11 min;
  // therefore forget the exact calculation and use only 1 instead; good compromise
  lastTMR = tmr10ms;

  DEBUG_TIMER_START(debugTimerMixes10ms);
  if (tick10ms) {
    /* Throttle trace */
    int16_t val;

    if (g_model.thrTraceSrc > MAX_POTS) {
      uint8_t ch = g_model.thrTraceSrc - MAX_POTS - 1;
      val = limit<int32_t>(0, RESX + channelOutputs[ch], 2 * RESX);
    }
    else {
      val = RESX + calibratedAnalogs[g_model.thrTraceSrc == 0 ? inputMappingConvertMode(inputMappingGetThrottle()) : g_model.thrTraceSrc + MAX_STICKS - 1];
    }

    // calibrate it (resolution increased by factor 4)
    // Use previous calculation for air radios to avoid breaking things
    val = val >> (RESX_SHIFT - 6);

    evalTimers(val, tick10ms);

    static uint8_t  s_cnt_100ms;
    static uint8_t  s_cnt_1s;
    static uint8_t  s_cnt_samples_thr_1s;
    static uint16_t s_sum_samples_thr_1s;

    s_cnt_samples_thr_1s++;
    s_sum_samples_thr_1s+=val;

    if ((s_cnt_100ms += tick10ms) >= 10) { // 0.1sec
      s_cnt_100ms -= 10;
      s_cnt_1s += 1;

      if (s_cnt_1s >= 10) { // 1sec
        s_cnt_1s -= 10;
        sessionTimer += 1;
        inactivity.counter++;
        if ((((uint8_t)inactivity.counter) & 0x07) == 0x01 && g_eeGeneral.inactivityTimer && inactivity.counter > ((uint16_t)g_eeGeneral.inactivityTimer * 60))
          AUDIO_INACTIVITY();

        val = s_sum_samples_thr_1s / s_cnt_samples_thr_1s;
        s_timeCum16ThrP += (val>>3);  // s_timeCum16ThrP would overrun if we would store throttle value with higher accuracy; therefore stay with 16 steps
        if (val)
          s_timeCumThr += 1;
        s_sum_samples_thr_1s >>= 2;  // correct better accuracy now, because trace graph can show this information; in case thrtrace is not active, the compile should remove this

#if defined(THRTRACE)
        // throttle trace is done every 10 seconds; Tracebuffer is adjusted to screen size.
        // in case buffer runs out, it wraps around
        // resolution for y axis is only 32, therefore no higher value makes sense
        s_cnt_samples_thr_10s += s_cnt_samples_thr_1s;
        s_sum_samples_thr_10s += s_sum_samples_thr_1s;

        if (++s_cnt_10s >= 10) { // 10s
          s_cnt_10s -= 10;
          val = s_sum_samples_thr_10s / s_cnt_samples_thr_10s;
          s_sum_samples_thr_10s = 0;
          s_cnt_samples_thr_10s = 0;
          s_traceBuf[s_traceWr % MAXTRACE] = val;
          s_traceWr++;
        }
#endif

        s_cnt_samples_thr_1s = 0;
        s_sum_samples_thr_1s = 0;
      }
    }

  }

  DEBUG_TIMER_STOP(debugTimerMixes10ms);

  s_mixer_first_run_done = true;
}
