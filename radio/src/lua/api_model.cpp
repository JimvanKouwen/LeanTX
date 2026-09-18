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
#include "lua_host_api.h"
#include "lua_api.h"
#include "../timers.h"
#include "model_init.h"

#include "lua_states.h"

#include <storage/sdcard_yaml.h>

/*luadoc
@function model.getInfo()

Get current Model information

@retval table model information:
 * `name` (string) model name
 * `jitterFilter` (number) model level ADC filter
 * `bitmap` (string) bitmap name (not present on X7)
 * `filename` (string) model filename

@status current Introduced in 2.0.6, changed in 2.2.0, filename added in 2.6.0, jitterFilter and labels added in 2.8.0
*/
static int luaModelGetInfo(lua_State *L)
{
  lua_newtable(L);
  lua_pushtablenstring(L, "name", LuaHostApi::model().header.name);
  lua_pushtableinteger(L, "jitterFilter", LuaHostApi::model().jitterFilter);
#if LCD_DEPTH > 1
  lua_pushtablenstring(L, "bitmap", LuaHostApi::model().header.bitmap);
#endif

#if defined(STORAGE_MODELSLIST)
  lua_pushtablenstring(L, "labels", LuaHostApi::model().header.labels);
  lua_pushtablenstring(L, "filename", LuaHostApi::radio().currModelFilename);
#else
  lua_pushtablenstring(L, "labels", "");
  char fname[MODELIDX_STRLEN + sizeof(YAML_EXT)];
  getModelNumberStr(LuaHostApi::radio().currModel, fname);
  strcat(fname, YAML_EXT);
  lua_pushtablenstring(L, "filename", fname);
#endif

  return 1;
}

/*luadoc
@function model.setInfo(value)

Set cosmetic model information (name and bitmap only). Control preprocessing
fields such as jitterFilter are read-only and ignored by this setter.

@param value model information data, see model.getInfo()

@notice If a parameter is missing from the value, then
that parameter remains unchanged.

@status current Introduced in 2.0.6, jitterFilter added in 2.8.0
*/
static int luaModelSetInfo(lua_State *L)
{
  luaL_checktype(L, -1, LUA_TTABLE);
  for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
    luaL_checktype(L, -2, LUA_TSTRING); // key is string
    const char * key = luaL_checkstring(L, -2);
    if (!strcmp(key, "name")) {
      const char * name = luaL_checkstring(L, -1);
      LuaHostApi::setModelName(name);
    }
#if LCD_DEPTH > 1
    else if (!strcmp(key, "bitmap")) {
      const char * name = luaL_checkstring(L, -1);
      LuaHostApi::setModelBitmap(name);
    }
#endif
  }
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
    const ModuleData & module = LuaHostApi::model().moduleData[idx];
    lua_newtable(L);
    lua_pushtableinteger(L, "subType", 0);
    lua_pushtableinteger(L, "modelId", LuaHostApi::model().header.modelId[idx]);
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
@function model.getTimer(timer)

Get model timer parameters

@param timer (number) timer index (0 for Timer 1)

@retval nil requested timer does not exist

@retval table timer parameters:
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
    const TimerData & timer = LuaHostApi::model().timers[idx];
    lua_newtable(L);
    lua_pushtableinteger(L, "start", timer.start);
    lua_pushtableinteger(L, "value", LuaHostApi::timerValue(idx));
    lua_pushtableinteger(L, "countdownBeep", timer.countdownBeep);
    lua_pushtableboolean(L, "minuteBeep", timer.minuteBeep);
    lua_pushtableinteger(L, "persistent", timer.persistent);
    lua_pushtablenstring(L, "name", timer.name);
    lua_pushtableboolean(L, "showElapsed", timer.showElapsed);
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
    TimerData timer = LuaHostApi::model().timers[idx];
    int32_t value = 0;
    bool updateValue = false;
    luaL_checktype(L, -1, LUA_TTABLE);
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
      luaL_checktype(L, -2, LUA_TSTRING); // key is string
      const char * key = luaL_checkstring(L, -2);
      if (!strcmp(key, "start")) {
        timer.start = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "value")) {
        value = luaL_checkinteger(L, -1);
        updateValue = true;
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
      else if (!strcmp(key, "countdownStart")) {
        timer.countdownStart = luaL_checkinteger(L, -1);
      }
      else if (!strcmp(key, "extraHaptic")) {
        timer.extraHaptic = lua_tointeger(L, -1);
      }
    }
    LuaHostApi::setTimer(idx, timer, value, updateValue);
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
    LuaHostApi::resetTimer(idx);
  }
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
    lua_pushinteger(L, LuaHostApi::switchWarning(sw));
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
    LuaHostApi::setSwitchWarning(sw, newstate);
  }
  else {
    lua_pushnil(L);
  }
  return 1;
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
    const TelemetrySensor & sensor = LuaHostApi::sensor(idx);
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
    LuaHostApi::resetSensor(idx);
  }

  lua_pushnil(L);
  return 1;
}

extern "C" {
LROT_BEGIN(modellib, NULL, 0)
  LROT_FUNCENTRY( getInfo, luaModelGetInfo )
  LROT_FUNCENTRY( setInfo, luaModelSetInfo )
  LROT_FUNCENTRY( getModule, luaModelGetModule )
  LROT_FUNCENTRY( getTimer, luaModelGetTimer )
  LROT_FUNCENTRY( setTimer, luaModelSetTimer )
  LROT_FUNCENTRY( resetTimer, luaModelResetTimer )
  LROT_FUNCENTRY( getSwitchWarning, luaModelGetSwitchWarning )
  LROT_FUNCENTRY( setSwitchWarning, luaModelSetSwitchWarning )
  LROT_FUNCENTRY( getSensor, luaModelGetSensor )
  LROT_FUNCENTRY( resetSensor, luaModelResetSensor )
LROT_END(modellib, NULL, 0)
}
