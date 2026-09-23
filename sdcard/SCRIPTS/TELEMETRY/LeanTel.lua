-- SPDX-License-Identifier: GPL-2.0-only
local App=loadScript('/SCRIPTS/LeanTel/app.lua')()
local state
local function init() state=App.new(false) end
local function run(event)
  if not state then init() end
  return App.run(state,event,nil,LCD_W,LCD_H)
end
local function background() if state then App.background(state) end end
return {init=init,run=run,background=background}
