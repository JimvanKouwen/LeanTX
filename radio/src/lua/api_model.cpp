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

#define LUA_LIB

#include <ctype.h>
#include <stdio.h>
#include "edgetx.h"
#include "lua_api.h"
#include "../timers.h"
#include "model_init.h"
#include "mixes.h"

#include "lua_states.h"

#include <storage/sdcard_yaml.h>

/*luadoc
@function model.getInfo()

Get current Model information

@retval table model information:
 * `name` (string) model name
 * `extendedLimits` (boolean) extended limits enabled
 * `jitterFilter` (number) model level ADC filter
 * `bitmap` (string) bitmap name (not present on X7)
 * `filename` (string) model filename

@status current Introduced in 2.0.6, changed in 2.2.0, filename added in 2.6.0, extendedLimits, jitterFilter, and labels added in 2.8.0
*/
static int luaModelGetInfo(lua_State *L)
{
  lua_newtable(L);
  lua_pushtablenstring(L, "name", g_model.header.name);
  lua_pushtableboolean(L, "extendedLimits", g_model.extendedLimits);
  lua_pushtableinteger(L, "jitterFilter", g_model.jitterFilter);
#if LCD_DEPTH > 1
  lua_pushtablenstring(L, "bitmap", g_model.header.bitmap);
#endif

#if defined(STORAGE_MODELSLIST)
  lua_pushtablenstring(L, "labels", g_model.header.labels);
  lua_pushtablenstring(L, "filename", g_eeGeneral.currModelFilename);
#else
  lua_pushtablenstring(L, "labels", "");
  char fname[MODELIDX_STRLEN + sizeof(YAML_EXT)];
  getModelNumberStr(g_eeGeneral.currModel, fname);
  strcat(fname, YAML_EXT);
  lua_pushtablenstring(L, "filename", fname);
#endif

  return 1;
}

/*luadoc
@function model.setInfo(value)

Set the current Model information

@param value model information data, see model.getInfo()

@notice If a parameter is missing from the value, then
that parameter remains unchanged.

@status current Introduced in 2.0.6, extendedLimits and jitterFilter added in 2.8.0
*/
static int luaModelSetInfo(lua_State *L)
{
  luaL_checktype(L, -1, LUA_TTABLE);
  for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
    luaL_checktype(L, -2, LUA_TSTRING); // key is string
    const char * key = luaL_checkstring(L, -2);
    if (!strcmp(key, "name")) {
      const char * name = luaL_checkstring(L, -1);
      strncpy(g_model.header.name, name, sizeof(g_model.header.name));
    }
    else if (!strcmp(key, "extendedLimits")) {
      g_model.extendedLimits = lua_toboolean(L, -1);
    }
    else if (!strcmp(key, "jitterFilter")) {
      auto j = lua_tointeger(L, -1);
      if (j > OVERRIDE_ON) j = OVERRIDE_ON;
      g_model.jitterFilter = j;
    }
#if LCD_DEPTH > 1
    else if (!strcmp(key, "bitmap")) {
      const char * name = luaL_checkstring(L, -1);
      strncpy(g_model.header.bitmap, name, LEN_BITMAP_NAME);
    }
#endif
  }
  storageDirty(EE_MODEL);
  return 0;
}

/*luadoc
@function model.getModule(index)

Return RF module parameters. Type is 0 (off) or 5 (CRSF).

@param index module index (0 internal, 1 external)
@retval table with Type, modelId, firstChannel, channelsCount, and subType (always 0)
@retval nil if the module index does not exist
*/
static int luaModelGetModule(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);
  if (idx < NUM_MODULES) {
    ModuleData & module = g_model.moduleData[idx];
    lua_newtable(L);
    lua_pushtableinteger(L, "subType", 0);
    lua_pushtableinteger(L, "modelId", g_model.header.modelId[idx]);
    lua_pushtableinteger(L, "firstChannel", module.channelsStart);
    lua_pushtableinteger(L, "channelsCount", module.getChannelsCount());
    lua_pushtableinteger(L, "Type", module.type);
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function model.setModule(index, value)

Set RF module parameters

@param index (number) module index (0 for internal, 1 for external)

@param value module parameters, see model.getModule()

@notice If a parameter is missing from the value, then
that parameter remains unchanged.

@status current Introduced in 2.2.0, modified in 2.3.12 (proto/subproto)
*/
static int luaModelSetModule(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);

  if (idx < NUM_MODULES) {

    ModuleData & module = g_model.moduleData[idx];
    luaL_checktype(L, -1, LUA_TTABLE);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
      luaL_checktype(L, -2, LUA_TSTRING); // key is string
      const char * key = luaL_checkstring(L, -2);
      if (!strcmp(key, "Type")) {
        auto requested = luaL_checkinteger(L, -1);
        uint8_t newtype = requested == MODULE_TYPE_CROSSFIRE ? MODULE_TYPE_CROSSFIRE : MODULE_TYPE_NONE;
        if (newtype != module.type) {
          setModuleType(idx, newtype);
        }
      }
      else if (!strcmp(key, "subType")) {
        // CRSF has no RF sub-protocol.
      }
      else if (!strcmp(key, "modelId")) {
        g_model.header.modelId[idx] = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "firstChannel")) {
        module.channelsStart = limit<int>(0, luaL_checkinteger(L, -1), MAX_OUTPUT_CHANNELS - CROSSFIRE_CHANNELS_COUNT);
      }
      else if (!strcmp(key, "channelsCount")) {
        module.channelsCount = CROSSFIRE_CHANNELS_COUNT - 8;
      }
    }
    storageDirty(EE_MODEL);
  }
  return 0;
}

/*luadoc
@function model.getTimer(timer)

Get model timer parameters

@param timer (number) timer index (0 for Timer 1)

@retval nil requested timer does not exist

@retval table timer parameters:
 * `mode` (number) timer trigger source: off, abs, stk,  stk%, sw/!sw, !m_sw/!m_sw
 * `start` (number) start value [seconds], 0 for up timer, 0> down timer
 * `value` (number) current value [seconds]
 * `countdownBeep` (number) countdown beep (0­ = silent, 1 =­ beeps, 2­ = voice)
 * `minuteBeep` (boolean) minute beep
 * `persistent` (number) persistent timer
 * `name` (string) timer name
 * `showElapsed` (boolean) show elapsed

@status current Introduced in 2.0.0, name added in 2.3.6, showElapsed added in 2.8.0
*/
static int luaModelGetTimer(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);
  if (idx < MAX_TIMERS) {
    TimerData & timer = g_model.timers[idx];
    lua_newtable(L);
    lua_pushtableinteger(L, "mode", timer.mode);
    lua_pushtableinteger(L, "start", timer.start);
    lua_pushtableinteger(L, "value", timersStates[idx].val);
    lua_pushtableinteger(L, "countdownBeep", timer.countdownBeep);
    lua_pushtableboolean(L, "minuteBeep", timer.minuteBeep);
    lua_pushtableinteger(L, "persistent", timer.persistent);
    lua_pushtablenstring(L, "name", timer.name);
    lua_pushtableboolean(L, "showElapsed", timer.showElapsed);
    lua_pushtableinteger(L, "switch", timer.swtch);
    lua_pushtableinteger(L, "countdownStart", timer.countdownStart);
    lua_pushtableinteger(L, "extraHaptic", timer.extraHaptic);
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function model.setTimer(timer, value)

Set model timer parameters

@param timer (number) timer index (0 for Timer 1)

@param value timer parameters, see model.getTimer()

@notice If a parameter is missing from the value, then
that parameter remains unchanged.

@status current Introduced in 2.0.0, name added in 2.3.6, showElapsed added in 2.8.0
*/
static int luaModelSetTimer(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);

  if (idx < MAX_TIMERS) {
    TimerData & timer = g_model.timers[idx];
    luaL_checktype(L, -1, LUA_TTABLE);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
      luaL_checktype(L, -2, LUA_TSTRING); // key is string
      const char * key = luaL_checkstring(L, -2);
      if (!strcmp(key, "mode")) {
        timer.mode = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "start")) {
        timer.start = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "value")) {
        timersStates[idx].val = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "countdownBeep")) {
        timer.countdownBeep = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "minuteBeep")) {
        timer.minuteBeep = lua_toboolean(L, -1);
      }
      else if (!strcmp(key, "persistent")) {
        timer.persistent = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "name")) {
        const char * name = luaL_checkstring(L, -1);
        strncpy(timer.name, name, sizeof(timer.name));
      }
      else if (!strcmp(key, "showElapsed")) {
        timer.showElapsed = lua_toboolean(L, -1);
      }
      else if (!strcmp(key, "switch")) {
        timer.swtch = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "countdownStart")) {
        timer.countdownStart = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "extraHaptic")) {
        timer.extraHaptic = lua_tointeger(L, -1);
      }
    }
    storageDirty(EE_MODEL);
  }
  return 0;
}

/*luadoc
@function model.resetTimer(timer)

Reset model timer to a startup value

@param timer (number) timer index (0 for Timer 1)

@status current Introduced in TODO
*/
static int luaModelResetTimer(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);
  if (idx < MAX_TIMERS) {
    timerReset(idx);
  }
  return 0;
}

static unsigned int getFirstMix(unsigned int chn)
{
  for (unsigned int i=0; i<MAX_MIXERS; i++) {
    MixData * mix = mixAddress(i);
    if (!mix->srcRaw || mix->destCh>=chn) {
      return i;
    }
  }
  return 0;
}

static unsigned int getMixesCountFromFirst(unsigned int chn, unsigned int first)
{
  unsigned int count = 0;
  for (unsigned int i=first; i<MAX_MIXERS; i++) {
    MixData * mix = mixAddress(i);
    if (!mix->srcRaw || mix->destCh!=chn) break;
    count++;
  }
  return count;
}

static unsigned int getMixesCount(unsigned int chn)
{
  return getMixesCountFromFirst(chn, getFirstMix(chn));
}

/*luadoc
@function model.getMixesCount(channel)

Get the number of Mixer lines that the specified Channel has

@param channel (unsigned number) channel number (use 0 for CH1)

@retval number number of mixes for requested channel

@status current Introduced in 2.0.0
*/
static int luaModelGetMixesCount(lua_State *L)
{
  unsigned int chn = luaL_checkinteger(L, 1);
  unsigned int count = getMixesCount(chn);
  lua_pushinteger(L, count);
  return 1;
}

/*luadoc
@function model.getMix(channel, line)

Get configuration for specified Mix

@param channel (unsigned number) channel number (use 0 for CH1)

@param line (unsigned number) mix number (use 0 for first line(mix))

@retval nil requested channel or line does not exist

@retval table mix data:
 * `name` (string) mix line name
 * `source` (number) source index
 * `weight` (number) literal weight (-500 to 500)
 * `offset` (number) literal offset (-500 to 500)
 * `curveType` (number) curve type (function, expo, custom curve)
 * `curveValue` (number) curve index

*/
static int luaModelGetMix(lua_State *L)
{
  unsigned int chn = luaL_checkinteger(L, 1);
  unsigned int idx = luaL_checkinteger(L, 2);
  unsigned int first = getFirstMix(chn);
  unsigned int count = getMixesCountFromFirst(chn, first);
  if (idx < count) {
    MixData * mix = mixAddress(first+idx);
    lua_newtable(L);
    lua_pushtablenstring(L, "name", mix->name);
    lua_pushtableinteger(L, "source", mix->srcRaw);
    lua_pushtableinteger(L, "weight", (mix->weight));
    lua_pushtableinteger(L, "offset", (mix->offset));
    lua_pushtableinteger(L, "curveType", mix->curve.type);
    lua_pushtableinteger(L, "curveValue", (mix->curve.value));
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function model.insertMix(channel, line, value)

Insert a mixer line into Channel

@param channel (unsigned number) channel number (use 0 for CH1)

@param line (unsigned number) mix number (use 0 for first line(mix))

@param value (table) see model.getMix() for table format

@status current Introduced in 2.0.0
*/
static int luaModelInsertMix(lua_State *L)
{
  unsigned int chn = luaL_checkinteger(L, 1);
  unsigned int idx = luaL_checkinteger(L, 2);

  unsigned int first = getFirstMix(chn);
  unsigned int count = getMixesCountFromFirst(chn, first);

  if (chn<MAX_OUTPUT_CHANNELS && getMixCount()<MAX_MIXERS && idx<=count) {
    idx += first;
    insertMix(idx, chn);
    MixData *mix = mixAddress(idx);
    luaL_checktype(L, -1, LUA_TTABLE);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
      luaL_checktype(L, -2, LUA_TSTRING); // key is string
      const char * key = luaL_checkstring(L, -2);
      if (!strcmp(key, "name")) {
        const char * name = luaL_checkstring(L, -1);
        strncpy(mix->name, name, sizeof(mix->name));
      }
      else if (!strcmp(key, "source")) {
        mix->srcRaw = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "weight")) {
        mix->weight = (luaL_checkinteger(L, -1));
      }
      else if (!strcmp(key, "offset")) {
        mix->offset = (luaL_checkinteger(L, -1));
      }
      else if (!strcmp(key, "curveType")) {
        mix->curve.type = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "curveValue")) {
        mix->curve.value = (luaL_checkinteger(L, -1));
      }

    }
  }

  return 0;
}

/*luadoc
@function model.deleteMix(channel, line)

Delete mixer line from specified Channel

@param channel (unsigned number) channel number (use 0 for CH1)

@param line (unsigned number) mix number (use 0 for first line(mix))

@status current Introduced in 2.0.0
*/
static int luaModelDeleteMix(lua_State *L)
{
  unsigned int chn = luaL_checkinteger(L, 1);
  unsigned int idx = luaL_checkinteger(L, 2);

  unsigned int first = getFirstMix(chn);
  unsigned int count = getMixesCountFromFirst(chn, first);

  if (idx < count) {
    deleteMix(first+idx);
  }

  return 0;
}

/*luadoc
@function model.deleteMixes()

Remove all mixers

@status current Introduced in 2.0.0
*/
static int luaModelDeleteMixes(lua_State *L)
{
  memset(g_model.mixData, 0, sizeof(g_model.mixData));
  return 0;
}

/*luadoc
@function model.getSwitchWarning(switch)

Get warning state for a switch

@param switch (unsigned number) switch number (use 0 for SA)
@param switch (string) switch name

@retval nil when switch is a toggle or does not exist
@retval number
0 = no warning
1 = switch up
2 = switch middle
3 = switch down

@status current Introduced in 3.0.0
*/

static int luaModelGetSwitchWarning(lua_State *L)
{
  unsigned int sw = MIXSRC_NONE;
  if (lua_isnumber(L, 1)) {
    sw = luaL_checkinteger(L, 1);
  }
  else {
    // convert from field name to its number
    const char *name = luaL_checkstring(L, 1);
    LuaField field;
    bool found = luaFindFieldByName(name, field);
    if (found) {
      sw = field.id - MIXSRC_FIRST_SWITCH;
    }
  }

  if (sw <= switchGetMaxAllSwitches() && SWITCH_WARNING_ALLOWED(sw)) {
    lua_pushinteger(L, g_model.getSwitchWarning(sw));
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function model.setSwitchWarning(switch, state)

Set warning state for a switch

@param switch (unsigned number) switch number (use 0 for SA)
@param switch (string) switch name

@param state (number) state
0 = no warning
1 = switch up
2 = switch middle
3 = switch down

@retval nil when switch is a toggle or does not exist

@status current Introduced in 3.0.0
*/

static int luaModelSetSwitchWarning(lua_State *L)
{
  unsigned int sw = MIXSRC_NONE;
  if (lua_isnumber(L, 1)) {
    sw = luaL_checkinteger(L, 1);
  }
  else {
    // convert from field name to its number
    const char *name = luaL_checkstring(L, 1);
    LuaField field;
    bool found = luaFindFieldByName(name, field);
    if (found) {
      sw = field.id - MIXSRC_FIRST_SWITCH;
    }
  }
  unsigned int newstate = luaL_checkinteger(L, 2);

  if (sw <= switchGetMaxAllSwitches() && SWITCH_WARNING_ALLOWED(sw) && newstate < 4) {
    g_model.setSwitchWarning(sw, newstate);
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function model.getCurve(curve)

Get Curve parameters

@param curve (unsigned number) curve number (use 0 for Curve1)

@retval nil requested curve does not exist

@retval table curve data:
 * `name` (string) name
 * `type` (number) type
 * `smooth` (boolean) smooth
 * `points` (number) number of points
 * `y` (table) table of Y values:
   * `key` is point number (zero based)
   * `value` is y value
 * `x` (table) **only included for custom curve type**:
   * `key` is point number (zero based)
   * `value` is x value

 Note that functions returns the tables starting with index 0 contrary to LUA's
 usual index starting with 1

@status current Introduced in 2.0.12
*/
static int luaModelGetCurve(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);
  if (idx < MAX_CURVES) {
    CurveHeader & CurveHeader = g_model.curves[idx];
    lua_newtable(L);
    lua_pushtablenstring(L, "name", CurveHeader.name);
    lua_pushtableinteger(L, "type", CurveHeader.type);
    lua_pushtableboolean(L, "smooth", CurveHeader.smooth);
    lua_pushtableinteger(L, "points", CurveHeader.points + 5);
    lua_pushstring(L, "y");
    lua_newtable(L);
    int8_t * point = curveAddress(idx);
    for (int i=0; i < CurveHeader.points + 5; i++) {
      lua_pushinteger(L, i + 1);
      lua_pushinteger(L, *point++);
      lua_settable(L, -3);
    }
    lua_settable(L, -3);
    if (CurveHeader.type == CURVE_TYPE_CUSTOM) {
      lua_pushstring(L, "x");
      lua_newtable(L);
      lua_pushinteger(L, 1);
      lua_pushinteger(L, -100);
      lua_settable(L, -3);
      for (int i=0; i < CurveHeader.points + 3; i++) {
        lua_pushinteger(L, i + 2);
        lua_pushinteger(L, *point++);
        lua_settable(L, -3);
      }
      lua_pushinteger(L, CurveHeader.points + 5);
      lua_pushinteger(L, 100);
      lua_settable(L, -3);
      lua_settable(L, -3);
    }
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function model.setCurve(curve, params)

Set Curve parameters

@param curve (unsigned number) curve number (use 0 for Curve1)

@param params see model.getCurve return format for table format. setCurve uses standard
 lua array indexing and arrays start at index 1

The first and last x value must -100 and 100 and x values must be monotonically increasing

@retval  0 - Everything okay
         1 - Wrong number of points
         2 - Invalid Curve number
         3 - Cuve does not fit anymore
         4 - point of out of index
         5 - x value not monotonically increasing
         6 - y value not in range [-100;100]
         7 - extra values for y are set
         8 - extra values for x are set

@status current Introduced in 2.2.0

Example setting a 4-point custom curve:
```lua
  params = {}
  params["x"] =  {-100, -34, 77, 100}
  params["y"] = {-70, 20, -89, -100}
  params["smooth"] = true
  params["type"] = 1
  val =  model.setCurve(2, params)
 ```
setting a 6-point standard smoothed curve
 ```lua
 val = model.setCurve(3, {smooth=true, y={-100, -50, 0, 50, 100, 80}})
 ```

*/
static int luaModelSetCurve(lua_State *L)
{
  unsigned int curveIdx = luaL_checkinteger(L, 1);

  if (curveIdx >= MAX_CURVES) {
    lua_pushinteger(L, 2);
    return 1;
  }
  int8_t xPoints[MAX_POINTS_PER_CURVE];
  int8_t yPoints[MAX_POINTS_PER_CURVE];

  // Init to invalid values
  memset(xPoints, -127, sizeof(xPoints));
  memset(yPoints, -127, sizeof(yPoints));

  CurveHeader &destCurveHeader = g_model.curves[curveIdx];
  CurveHeader newCurveHeader;
  memclear(&newCurveHeader, sizeof(CurveHeader));

  luaL_checktype(L, -1, LUA_TTABLE);
  for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
    luaL_checktype(L, -2, LUA_TSTRING); // key is string
    const char *key = luaL_checkstring(L, -2);
    if (!strcmp(key, "name")) {
      const char *name = luaL_checkstring(L, -1);
      strncpy(newCurveHeader.name, name, sizeof(newCurveHeader.name));
    }
    else if (!strcmp(key, "type")) {
      newCurveHeader.type = luaL_checkinteger(L, -1);
    }
    else if (!strcmp(key, "smooth")) {
      // Earlier version of this api expected a 0/1 integer instead of a boolean
      // Still accept a 0/1 here
      if (lua_isboolean(L,-1))
        newCurveHeader.smooth = lua_toboolean(L, -1);
      else
        newCurveHeader.smooth = luaL_checkinteger(L, -1);
    }
    else if (!strcmp(key, "x") || !strcmp(key, "y")) {
      luaL_checktype(L, -1, LUA_TTABLE);
      bool isX = !strcmp(key, "x");

      for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        int idx = luaL_checkinteger(L, -2) - 1; // key is integer
        if (idx < 0 || idx > MAX_POINTS_PER_CURVE) {
          lua_pushinteger(L, 4);
          return 1;
        }
        int8_t val = luaL_checkinteger(L, -1);
        if (val < -100 || val > 100) {
          lua_pushinteger(L, 6);
          return 1;
        }
        if (isX)
          xPoints[idx] = val;
        else
          yPoints[idx] = val;
      }
    }
  }
  // Check how many points are set
  int numPoints = 0;
  for (numPoints = 0; numPoints < MAX_POINTS_PER_CURVE; numPoints += 1) {
    if (yPoints[numPoints] == -127)
      break;
  }
  newCurveHeader.points = numPoints - 5;

  if (numPoints < MIN_POINTS_PER_CURVE || numPoints > MAX_POINTS_PER_CURVE) {
    lua_pushinteger(L, 1);
    return 1;
  }

  if (newCurveHeader.type == CURVE_TYPE_CUSTOM) {

    // The rest of the points are checked by the monotonic condition
    for (unsigned int i=numPoints; i < sizeof(xPoints);i++)
    {
      if (xPoints[i] != -127)
      {
        lua_pushinteger(L, 8);
        return 1;
      }
    }

    // Check first and last point
    if (xPoints[0] != -100 || xPoints[newCurveHeader.points + 4] != 100) {
      lua_pushinteger(L, 5);
      return 1;
    }

    // Check that x values are increasing
    for (int i = 1; i < numPoints; i++) {
      if (xPoints[i - 1] > xPoints[i]) {
        lua_pushinteger(L, 5);
        return 1;
      }
    }
  }

  // Check that ypoints have the right number of points set
  for (int i=0; i <  5 + newCurveHeader.points;i++)
  {
    if (yPoints[i] == -127)
    {
      lua_pushinteger(L, 7);
      return 1;
    }
  }

  // Calculate size of curve we replace
  int oldCurveMemSize;
  if (destCurveHeader.type == CURVE_TYPE_STANDARD) {
    oldCurveMemSize = 5 + destCurveHeader.points;
  }
  else {
    oldCurveMemSize = 8 + 2 * destCurveHeader.points;
  }

  // Calculate own size
  int newCurveMemSize;
  if (newCurveHeader.type == CURVE_TYPE_STANDARD)
    newCurveMemSize = 5 + newCurveHeader.points;
  else
    newCurveMemSize = 8 + 2 * newCurveHeader.points;

  int shift = newCurveMemSize - oldCurveMemSize;

  // Also checks if new curve size would fit
  if (!moveCurve(curveIdx, shift)) {
    lua_pushinteger(L, 3);
    TRACE("curve shift is  %d", shift);
    return 1;
  }

  // Curve fits into mem, fill new curve
  destCurveHeader = newCurveHeader;

  int8_t *point = curveAddress(curveIdx);
  for (int i = 0; i < destCurveHeader.points + 5; i++) {
    *point++ = yPoints[i];
  }

  if (destCurveHeader.type == CURVE_TYPE_CUSTOM) {
    for (int i = 1; i < destCurveHeader.points + 4; i++) {
      *point++ = xPoints[i];
    }
  }
  storageDirty(EE_MODEL);

  lua_pushinteger(L, 0);
  return 1;
}

/*luadoc
@function model.getOutput(index)

Get servo parameters

@param index (unsigned number) output number (use 0 for CH1)

@retval nil requested output does not exist

@retval table output parameters:
 * `name` (string) name
 * `min` (number) Minimum % * 10
 * `max` (number) Maximum % * 10
 * `offset` (number) Subtrim * 10
 * `ppmCenter` (number) offset from PPM Center. 0 = 1500
 * `symetrical` (number) linear Subtrim 0 = Off, 1 = On
 * `revert` (number) irection 0 = ­­­---, 1 = INV
 * `curve`
   * (number) Curve number (0 for Curve1)
   * or `nil` if no curve set

@status current Introduced in 2.0.0
*/
static int luaModelGetOutput(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);
  if (idx < MAX_OUTPUT_CHANNELS) {
    LimitData * limit = limitAddress(idx);
    lua_newtable(L);
    lua_pushtablenstring(L, "name", limit->name);
    lua_pushtableinteger(L, "min", limit->min-1000);
    lua_pushtableinteger(L, "max", limit->max+1000);
    lua_pushtableinteger(L, "offset", limit->offset);
    lua_pushtableinteger(L, "ppmCenter", limit->ppmCenter);
    lua_pushtableinteger(L, "symetrical", limit->symetrical);
    lua_pushtableinteger(L, "revert", limit->revert);
    if (limit->curve)
      lua_pushtableinteger(L, "curve", limit->curve-1);
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function model.setOutput(index, value)

Set servo parameters

@param index (unsigned number) channel number (use 0 for CH1)

@param value (table) servo parameters, see model.getOutput() for table format

@notice If a parameter is missing from the value, then
that parameter remains unchanged.

@status current Introduced in 2.0.0
*/
static int luaModelSetOutput(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);
  if (idx < MAX_OUTPUT_CHANNELS) {
    LimitData * limit = limitAddress(idx);
    memclear(limit, sizeof(LimitData));
    luaL_checktype(L, -1, LUA_TTABLE);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
      luaL_checktype(L, -2, LUA_TSTRING); // key is string
      const char * key = luaL_checkstring(L, -2);
      if (!strcmp(key, "name")) {
        const char * name = luaL_checkstring(L, -1);
        strncpy(limit->name, name, sizeof(limit->name));
      }
      else if (!strcmp(key, "min")) {
        limit->min = luaL_checkinteger(L, -1)+1000;
      }
      else if (!strcmp(key, "max")) {
        limit->max = luaL_checkinteger(L, -1)-1000;
      }
      else if (!strcmp(key, "offset")) {
        limit->offset = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "ppmCenter")) {
        limit->ppmCenter = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "symetrical")) {
        limit->symetrical = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "revert")) {
        limit->revert = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "curve")) {
        limit->curve = luaL_checkinteger(L, -1) + 1;
      }
    }
    storageDirty(EE_MODEL);
  }

  return 0;
}

/*luadoc
@function model.getSensor(sensor)

Get Telemetry Sensor parameters

@param sensor (unsigned number) sensor number (use 0 for sensor 1)

@retval nil requested sensor does not exist

@retval table with sensor data:
 * `type` (number) 0 = custom, 1 = calculated
 * `name` (string) Name
 * `unit` (number) See list of units in the appendix of the OpenTX Lua Reference Guide
 * `prec` (number) Number of decimals
 * `id`   (number) Only custom sensors
 * `instance` (number) Only custom sensors
 * `formula` (number) Only calculated sensors. 0 = Add etc. see list of formula choices in Companion popup

@status current Introduced in 2.3.0
*/
static int luaModelGetSensor(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);
  if (idx < MAX_TELEMETRY_SENSORS) {
    TelemetrySensor & sensor = g_model.telemetrySensors[idx];
    lua_newtable(L);
    lua_pushtableinteger(L, "type", sensor.type);
    lua_pushtablenstring(L, "name", sensor.label);
    lua_pushtableinteger(L, "unit", sensor.unit);
    lua_pushtableinteger(L, "prec", sensor.prec);
    if (sensor.type == TELEM_TYPE_CUSTOM) {
      lua_pushtableinteger(L, "id", sensor.id);
      lua_pushtableinteger(L, "instance", sensor.instance);
    }
    else {
      lua_pushtableinteger(L, "formula", sensor.formula);
    }
  }
  else {
    lua_pushnil(L);
  }
  return 1;
}

/*luadoc
@function model.resetSensor(sensor)

Reset Telemetry Sensor parameters

@param sensor (unsigned number) sensor number (use 0 for sensor 1)

@retval nil

@status current Introduced in 2.3.11
*/
static int luaModelResetSensor(lua_State *L)
{
  unsigned int idx = luaL_checkinteger(L, 1);
  if (idx < MAX_TELEMETRY_SENSORS) {
    telemetryItems[idx].clear();
  }

  lua_pushnil(L);
  return 1;
}

extern "C" {
LROT_BEGIN(modellib, NULL, 0)
  LROT_FUNCENTRY( getInfo, luaModelGetInfo )
  LROT_FUNCENTRY( setInfo, luaModelSetInfo )
  LROT_FUNCENTRY( getModule, luaModelGetModule )
  LROT_FUNCENTRY( setModule, luaModelSetModule )
  LROT_FUNCENTRY( getTimer, luaModelGetTimer )
  LROT_FUNCENTRY( setTimer, luaModelSetTimer )
  LROT_FUNCENTRY( resetTimer, luaModelResetTimer )
  LROT_FUNCENTRY( getMixesCount, luaModelGetMixesCount )
  LROT_FUNCENTRY( getMix, luaModelGetMix )
  LROT_FUNCENTRY( insertMix, luaModelInsertMix )
  LROT_FUNCENTRY( deleteMix, luaModelDeleteMix )
  LROT_FUNCENTRY( deleteMixes, luaModelDeleteMixes )
  LROT_FUNCENTRY( getSwitchWarning, luaModelGetSwitchWarning )
  LROT_FUNCENTRY( setSwitchWarning, luaModelSetSwitchWarning )
  LROT_FUNCENTRY( getCurve, luaModelGetCurve )
  LROT_FUNCENTRY( setCurve, luaModelSetCurve )
  LROT_FUNCENTRY( getOutput, luaModelGetOutput )
  LROT_FUNCENTRY( setOutput, luaModelSetOutput )
  LROT_FUNCENTRY( getSensor, luaModelGetSensor )
  LROT_FUNCENTRY( resetSensor, luaModelResetSensor )
LROT_END(modellib, NULL, 0)
}
