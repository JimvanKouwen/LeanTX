-- Run from the repository root with Lua 5.3 or later.
local nativeIO=io
local files={}; local failWrite=false
io={open=function(p,m)
  if m=='r' and not files[p] then return nil end
  if m=='w' and failWrite then return nil end
  return {p=p,m=m}
end,read=function(f,n) return files[f.p]:sub(1,n) end,
write=function(f,s) files[f.p]=s; return f end,close=function() end}
local modelName='quad.yml'; local duplicate=false; local stale=false; local clock=0
model={getInfo=function() return {filename=modelName} end,
getSensor=function(i)
  local n=({'VFAS','RQly','Curr','Capa','RSSI'})[i+1]
  if i==5 and duplicate then n='VFAS' end
  if n then return {name=n,type=0,id=i+1,instance=0,unit=i==0 and 1 or 13,prec=1} end
end}
local ids={VFAS=1,RQly=2,Curr=3,Capa=4,RSSI=5,timer1=6,timer2=7,['tx-voltage']=8}
getFieldInfo=function(n) return ids[n] and {id=ids[n]} end
getSourceValue=function(id) return id==1 and 0 or 99,not stale,not stale end
getTime=function() return clock end
loadScript=function(p) return assert(loadfile('sdcard'..p)) end
for i,n in ipairs({'SMLSIZE','MIDSIZE','DBLSIZE','INVERS','SOLID','FORCE','CUSTOM_COLOR','EVT_VIRTUAL_NEXT_PAGE','EVT_VIRTUAL_PREV_PAGE','EVT_VIRTUAL_NEXT','EVT_VIRTUAL_PREV','EVT_VIRTUAL_ENTER','EVT_VIRTUAL_ENTER_LONG','EVT_VIRTUAL_EXIT','EVT_TOUCH_TAP'}) do _G[n]=i end
killEvents=function() end
local drawCount=0
local function draw(...) drawCount=drawCount+1 end
lcd={clear=draw,drawText=draw,drawRectangle=draw,drawFilledRectangle=draw,drawLine=draw,setColor=draw,RGB=function() return 0 end}
local C=loadScript('/SCRIPTS/LeanTel/config.lua')()
local path=C.path('mono'); local c=C.defaults()
assert(C.save(path,c)); c.slots[1]='s:1:0:VFAS'; assert(C.save(path,c))
assert(C.load(path).slots[1]=='s:1:0:VFAS')
files[path..'.b']='truncated'; assert(C.load(path).generation==1)
failWrite=true; assert(not C.save(path,c)); assert(c.generation==2); failWrite=false
assert(C.path('mono')~=C.path('color'))
local S=loadScript('/SCRIPTS/LeanTel/sensors.lua')(); local s=S.new(); S.tick(s,{'VFAS'})
local v,current=S.value(S.resolve(s,'VFAS')); assert(v==0 and current)
stale=true; clock=100; S.tick(s,{'VFAS'}); assert(s.history.VFAS.values[2]==false)
stale=false
for i=1,200 do clock=clock+100; S.tick(s,{'VFAS'}) end
assert(s.history.VFAS.count==60 and #s.history.VFAS.values==60)
duplicate=true; S.scan(s); assert(not S.resolve(s,'VFAS')); duplicate=false
local App=loadScript('/SCRIPTS/LeanTel/app.lua')()
for _,color in ipairs({false,true}) do
 local a=App.new(color)
 local w,h=color and 480 or 128,color and 272 or 64
 for page=1,5 do a.page=page; App.run(a,0,nil,w,h) end
 a.page=1; App.run(a,EVT_VIRTUAL_ENTER,nil,w,h); assert(a.mode=='edit')
 App.run(a,EVT_VIRTUAL_ENTER,nil,w,h); assert(a.mode=='pick')
 App.run(a,EVT_VIRTUAL_ENTER,nil,w,h); assert(a.mode=='edit' and a.dirty)
 failWrite=true; App.run(a,EVT_VIRTUAL_EXIT,nil,w,h); assert(a.mode=='edit' and a.error)
 failWrite=false; App.run(a,EVT_VIRTUAL_EXIT,nil,w,h); assert(a.mode=='view' and not a.dirty)
 local saved=a.config.slots[1]; assert(App.new(color).config.slots[1]==saved)
 if color then
  App.run(a,EVT_TOUCH_TAP,{x=470,y=265},w,h); assert(a.page==2)
  App.run(a,EVT_TOUCH_TAP,{x=240,y=265},w,h); assert(a.mode=='edit')
 end
 modelName='other.yml'; App.background(a); assert(a.config.generation==0 and a.mode=='view')
 modelName='quad.yml'
end
for _,size in ipairs({{128,64},{212,64},{320,240},{480,272},{800,480}}) do
 local a=App.new(size[1]>212)
 for _,layout in ipairs({4,6}) do a.config.layout=layout
  for p=1,5 do a.page=p; App.run(a,0,nil,size[1],size[2]) end
 end
end
assert(drawCount>100)
nativeIO.write('PASS: persistence, failed saves, corruption recovery, zero/stale data, bounded history, duplicate sensors, model switching, editing, touch, five display sizes\n')
