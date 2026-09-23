-- SPDX-License-Identifier: GPL-2.0-only
local M={}
local units={[1]='V',[2]='A',[3]='mA',[4]='kt',[5]='m/s',[6]='ft/s',[7]='km/h',[8]='mph',[9]='m',[10]='ft',[11]='C',[12]='F',[13]='%',[14]='mAh',[15]='W',[17]='dB',[18]='rpm',[25]='Hz',[26]='ms',[29]='dBm'}
function M.new() return {list={},byKey={},nextScan=0,nextSample=0,history={},lastTime=0} end
function M.scan(s)
  s.list={}; s.byKey={}
  for i=0,(MAX_SENSORS or 64)-1 do
    local sensor=model.getSensor(i)
    if sensor and sensor.name and sensor.name~='' then
      -- A duplicate name is ambiguous to getFieldInfo; omit it instead of guessing.
      local name=sensor.name
      local key=sensor.type==0 and ('s:'..sensor.id..':'..sensor.instance..':'..name) or ('c:'..name)
      local field=getFieldInfo(name)
      if field then
        local row={key=key,name=name,id=field.id,unit=units[sensor.unit] or '',prec=sensor.prec or 0}
        s.list[#s.list+1]=row; s.byKey[key]=row
      end
    end
  end
  local counts={}; for _,r in ipairs(s.list) do counts[r.name]=(counts[r.name] or 0)+1 end
  for _,r in ipairs(s.list) do if counts[r.name]>1 then r.id=nil end end
  for _,name in ipairs({'timer1','timer2','tx-voltage'}) do
    local f=getFieldInfo(name)
    if f then local r={key=name,name=name,id=f.id,unit=name=='tx-voltage' and 'V' or '',prec=1}; s.list[#s.list+1]=r; s.byKey[name]=r end
  end
end
function M.resolve(s,key)
  if s.byKey[key] then return s.byKey[key] end
  -- Preset names resolve only when unique. Saved choices use the full key.
  local found
  for _,r in ipairs(s.list) do if r.name==key then if found then return nil end; found=r end end
  return found
end
function M.value(r)
  if not r or not r.id then return nil,false end
  return getSourceValue(r.id)
end
function M.format(r,v)
  if v==nil then return '--' end
  if type(v)=='table' then
    if v.lat then return string.format('%.4f,%.4f',v.lat,v.lon) end
    return 'Structured'
  end
  if type(v)~='number' then return tostring(v) end
  if r.name:match('^timer') then return string.format('%s%d:%02d',v<0 and '-' or '',math.floor(math.abs(v)/60),math.floor(math.abs(v)%60)) end
  return string.format('%.'..math.min(2,r.prec)..'f',v)..(r.unit~='' and ' '..r.unit or '')
end
function M.tick(s,slots)
  local now=getTime()
  if now>=s.nextScan or now<s.lastTime then M.scan(s); s.nextScan=now+300 end
  if now>=s.nextSample or now<s.lastTime then
    s.nextSample=now+100
    local keep={}
    for _,key in ipairs(slots) do
      if not keep[key] then
        local h=s.history[key] or {values={},head=0,count=0}; keep[key]=h
        local r=M.resolve(s,key); local v,current=M.value(r)
        h.head=h.head%60+1; h.count=math.min(60,h.count+1)
        h.values[h.head]=(current and type(v)=='number') and v or false
        if current and type(v)=='number' then h.min=math.min(h.min or v,v); h.max=math.max(h.max or v,v) end
      end
    end
    s.history=keep
  end
  s.lastTime=now
end
return M
