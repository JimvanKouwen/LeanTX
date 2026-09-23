-- SPDX-License-Identifier: GPL-2.0-only
local base='/SCRIPTS/LeanTel/'
local function module(n) return loadScript(base..n..'.lua')() end
local Config, Sensors=module('config'),module('sensors')
local names={'Dashboard','Glance','Status','Inspector','Trends'}
local M={}
function M.new(color)
  local profile=color and 'color' or 'mono'
  local path=Config.path(profile)
  return {ui=module(profile),path=path,config=Config.load(path),sensors=Sensors.new(),page=1,mode='view',cursor=1,pick=1,scroll=1,dirty=false}
end
local function event(e,n) return _G[n]~=nil and e==_G[n] end
function M.background(a)
  local path=Config.path(a.ui.color and 'color' or 'mono')
  if path~=a.path then
    -- Widget instances can survive a model switch. Never transfer its configuration.
    a.path=path; a.config=Config.load(path); a.sensors=Sensors.new(); a.mode='view'; a.dirty=false
  end
  Sensors.tick(a.sensors,a.config.slots)
end
local function save(a)
  if not a.dirty or Config.save(a.path,a.config) then
    a.dirty=false; a.mode='view'; a.error=nil
  else a.error='Save failed - EXIT retries' end
end
local function choices(a)
  local list={}
  for _,r in ipairs(a.sensors.list) do list[#list+1]=r end
  table.sort(list,function(x,y)
    local _,xc=Sensors.value(x); local _,yc=Sensors.value(y)
    if (xc==true)~=(yc==true) then return xc==true end
    return x.name<y.name
  end)
  return list
end
local function select(a)
  if a.mode=='edit' then
    if a.cursor==7 then a.config.layout=a.config.layout==4 and 6 or 4; a.dirty=true
    else a.options=choices(a); a.pick=1; a.mode='pick' end
  elseif a.mode=='pick' then
    local r=a.options[a.pick]
    if r then a.config.slots[a.cursor]=r.key; a.dirty=true; a.mode='edit' end
  elseif a.page==4 then a.detail=not a.detail
  else a.mode='edit'; a.cursor=1; a.error=nil end
end
local function move(a,d)
  if a.mode=='edit' then a.cursor=(a.cursor-1+d)%7+1
  elseif a.mode=='pick' then a.pick=math.max(1,math.min(#a.options,a.pick+d))
  elseif a.page==4 then a.scroll=math.max(1,math.min(#a.sensors.list,a.scroll+d))
  else a.page=(a.page-1+d)%#names+1 end
end
-- Zone preview deliberately uses the widget zone; full-screen drawing starts at 0,0.
function M.preview(a,z)
  M.background(a)
  if z.w<70 or z.h<22 then return end
  local count=math.min(4,math.floor(z.h/22))
  for i=1,count do
    local key=a.config.slots[i]
    local r=Sensors.resolve(a.sensors,key); local v,current=Sensors.value(r)
    local label=(r and r.name or key)..' '..(current and '' or '~')..Sensors.format(r,v)
    local maxChars=math.max(1,math.floor((z.w-8)/8))
    if #label>maxChars then label=label:sub(1,maxChars-1)..'>' end
    lcd.drawText(z.x+4,z.y+(i-1)*22,label,SMLSIZE)
  end
end
function M.run(a,e,touch,w,h)
  M.background(a)
  local ui=a.ui
  local nextPage=event(e,'EVT_VIRTUAL_NEXT_PAGE')
  local prevPage=event(e,'EVT_VIRTUAL_PREV_PAGE')
  if a.mode=='view' and (nextPage or prevPage) then a.page=(a.page-1+(nextPage and 1 or -1))%#names+1; a.detail=false
  elseif event(e,'EVT_VIRTUAL_NEXT') or event(e,'EVT_VIRTUAL_INC') then move(a,1)
  elseif event(e,'EVT_VIRTUAL_PREV') or event(e,'EVT_VIRTUAL_DEC') then move(a,-1)
  elseif event(e,'EVT_VIRTUAL_ENTER') then select(a)
  elseif event(e,'EVT_VIRTUAL_ENTER_LONG') then
    if killEvents then killEvents(e) end
    if a.mode=='view' then a.mode='edit'; a.cursor=1 else save(a) end
  elseif event(e,'EVT_VIRTUAL_EXIT') then
    if a.mode=='pick' then a.mode='edit'
    elseif a.mode=='edit' then save(a)
    elseif a.detail then a.detail=false end
  end
  -- Touch controls use the same actions as the encoder. Footer: previous / edit / next.
  if ui.color and touch and event(e,'EVT_TOUCH_TAP') then
    if touch.y>=h-ui.bottom then
      if touch.x<w/3 then
        if a.mode=='view' then a.page=(a.page-2)%#names+1 else move(a,-1) end
      elseif touch.x>2*w/3 then
        if a.mode=='view' then a.page=a.page%#names+1 else move(a,1) end
      elseif a.mode=='edit' then save(a)
      elseif a.mode=='pick' then a.mode='edit'
      else a.mode='edit'; a.cursor=1 end
    elseif a.mode=='pick' or a.mode=='edit' then
      local pos=math.floor((touch.y-ui.top)/ui.row)+1
      local rows=math.max(1,math.floor((h-ui.top-ui.bottom)/ui.row))
      local active=a.mode=='pick' and a.pick or a.cursor
      local start=math.max(1,active-rows+1)
      local idx=start+pos-1
      local count=a.mode=='pick' and #a.options or 7
      if pos>=1 and pos<=rows and idx<=count then
        if a.mode=='pick' then a.pick=idx else a.cursor=idx end; select(a)
      end
    end
  end
  local footer=ui.color and '< Previous       Edit       Next >' or 'PAGE view  ENT edit'
  if a.mode=='edit' then footer=ui.color and '< Up          Save          Down >' or 'ENT select EXIT save' end
  if a.mode=='pick' then footer=ui.color and '< Up          Back          Down >' or 'ENT choose EXIT back' end
  local title=a.mode=='view' and (a.page..'/5 '..names[a.page]) or (a.mode=='pick' and 'Choose source' or 'Edit dashboard')
  ui.begin(w,h,title,a.error or footer)
  local rows=math.max(1,math.floor((h-ui.top-ui.bottom)/ui.row))
  if a.mode~='view' then
    local count=a.mode=='pick' and #a.options or 7
    local active=a.mode=='pick' and a.pick or a.cursor
    local start=math.max(1,active-rows+1)
    if count==0 then ui.text(2,ui.top,'No sources discovered') end
    for i=start,math.min(count,start+rows-1) do
      local label
      if a.mode=='pick' then label=a.options[i].name..(a.options[i].id and '' or ' (duplicate)')
      elseif i==7 then label='Layout: '..a.config.layout..' cells'
      else local r=Sensors.resolve(a.sensors,a.config.slots[i]); label=i..': '..(r and r.name or a.config.slots[i]) end
      ui.text(2,ui.top+(i-start)*ui.row,label,i==active)
    end
    return 0
  end
  if a.page==4 then
    local list=a.sensors.list
    a.scroll=math.max(1,math.min(a.scroll,#list))
    if #list==0 then ui.text(2,ui.top,'No sensors discovered') end
    if a.detail and list[a.scroll] then
      local r=list[a.scroll]; local v,current,fresh=Sensors.value(r)
      local lines={r.name,Sensors.format(r,v),v==nil and 'Missing' or (current and (fresh and 'Fresh' or 'Current') or 'Stale')}
      -- Firmware min/max are queried by source name; ambiguous names are excluded.
      for _,suffix in ipairs({'-','+'}) do
        local f=r.id and getFieldInfo(r.name..suffix)
        local n=f and getSourceValue(f.id)
        if n~=nil then lines[#lines+1]=(suffix=='-' and 'Min ' or 'Max ')..Sensors.format(r,n) end
      end
      for i=1,math.min(rows,#lines) do ui.text(2,ui.top+(i-1)*ui.row,lines[i]) end
    else
      for i=a.scroll,math.min(#list,a.scroll+rows-1) do
        local r=list[i]; local v,current=Sensors.value(r)
        ui.text(2,ui.top+(i-a.scroll)*ui.row,(current and ' ' or '~')..r.name..' '..Sensors.format(r,v),i==a.scroll)
      end
    end
    return 0
  end
  local available=h-ui.top-ui.bottom
  if a.page==5 then
    for i=1,2 do
      local key=a.config.slots[i]; local r=Sensors.resolve(a.sensors,key); local v,current=Sensors.value(r)
      local y=ui.top+(i-1)*math.floor(available/2); local gh=math.floor(available/2)
      ui.text(2,y,(r and r.name or key)..' '..(current and '' or '~')..Sensors.format(r,v))
      local history=a.sensors.history[key]
      if history then
        local low,high
        for _,n in pairs(history.values) do if type(n)=='number' then low=math.min(low or n,n); high=math.max(high or n,n) end end
        local prevX,prevY; local top=y+(ui.color and 22 or 8); local height=math.max(1,gh-(ui.color and 26 or 10))
        if low then
          for j=1,history.count do
            local n=history.values[(history.head-history.count+j-1)%60+1]
            if type(n)=='number' then
              local x=2+math.floor((j-1)*(w-5)/59)
              local yy=top+height-math.floor((n-low)/math.max(high-low,0.001)*height)
              if prevX then ui.line(prevX,prevY,x,yy) end
              prevX,prevY=x,yy
            else prevX,prevY=nil,nil end
          end
        end
      end
    end
    return 0
  end
  local count=a.page==1 and a.config.layout or 4
  for i=1,count do
    local x,y,cw,ch
    if a.page==2 then
      if i==1 then x=0;y=ui.top;cw=w;ch=math.floor(available*.66)
      else cw=math.floor(w/3);ch=available-math.floor(available*.66);x=(i-2)*cw;y=ui.top+math.floor(available*.66) end
    else cw=math.floor(w/2);ch=math.floor(available/(count/2));x=((i-1)%2)*cw;y=ui.top+math.floor((i-1)/2)*ch end
    local key=a.config.slots[i]; local r=Sensors.resolve(a.sensors,key); local v,current=Sensors.value(r)
    local label=r and r.name or key; local value=Sensors.format(r,v)
    if a.page==3 then
      -- No inferred battery percentages or universal RF thresholds.
      label=({'Battery','Link','Current','Used'})[i]..'/'..label
      if not ui.color then label=label:sub(1,10) end
    end
    if not ui.color then
      -- 2x3 and Glance's small footer show compact values that fit their cells.
      if ch<22 then label=label:sub(1,8) end
      if #value>math.floor((cw-4)/5) then value=value:sub(1,math.max(1,math.floor((cw-4)/5)-1))..'>' end
    end
    ui.card(x,y,cw,ch,label,value,false,v~=nil and not current,a.page==2 and i==1)
  end
  return 0
end
return M
