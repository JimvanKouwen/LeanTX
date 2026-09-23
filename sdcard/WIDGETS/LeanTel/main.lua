-- SPDX-License-Identifier: GPL-2.0-only
local App=loadScript('/SCRIPTS/LeanTel/app.lua')()
local function create(zone,options)
  return {zone=zone,state=App.new(true)}
end
local function refresh(widget,event,touch)
  if event==nil then
    -- Widgets in a home-screen zone are read-only; open full screen to configure.
    App.preview(widget.state,widget.zone)
  else App.run(widget.state,event,touch,LCD_W,LCD_H) end
end
return {name='LeanTel',options={},create=create,update=function() end,
  refresh=refresh,background=function(w) App.background(w.state) end}
