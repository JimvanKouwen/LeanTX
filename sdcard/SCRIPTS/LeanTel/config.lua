-- SPDX-License-Identifier: GPL-2.0-only
-- Data-only configuration. Two alternating snapshots preserve the last valid save.
local M = {}
function M.defaults()
  return {generation=0, layout=4, slots={'VFAS','RQly','Curr','Capa','RSSI','timer1'}}
end
local function read(path)
  local f=io.open(path,'r'); if not f then return nil end
  local s=io.read(f,1024); io.close(f); return s
end
local function encode(c)
  return 'LT1\n'..c.generation..'\n'..c.layout..'\n'..table.concat(c.slots,'\n')..'\nEND\n'
end
local function decode(s)
  if not s then return nil end
  local lines={}; for v in s:gmatch('([^\n]*)\n') do lines[#lines+1]=v end
  if #lines~=10 or lines[1]~='LT1' or lines[10]~='END' then return nil end
  local g,l=tonumber(lines[2]),tonumber(lines[3])
  if not g or g<0 or g%1~=0 or (l~=4 and l~=6) then return nil end
  local c={generation=g,layout=l,slots={}}
  for i=1,6 do if #lines[i+3]>80 then return nil end; c.slots[i]=lines[i+3] end
  return c
end
function M.path(profile)
  local info=model.getInfo()
  -- Reversible encoding avoids collisions between punctuation in model filenames.
  local key=(info.filename or ''):gsub('[^%w]',function(c) return string.format('_%02X',string.byte(c)) end)
  if key=='' then return nil end
  return '/SCRIPTS/LeanTel/config/'..key..'-'..profile
end
function M.load(path)
  if not path then return M.defaults() end
  local a,b=decode(read(path..'.a')),decode(read(path..'.b'))
  if b and (not a or b.generation>a.generation) then return b end
  return a or M.defaults()
end
function M.save(path,c)
  if not path then return false end
  c.generation=c.generation+1
  local p=path..(c.generation%2==1 and '.a' or '.b')
  local data=encode(c); local f=io.open(p,'w')
  if not f then c.generation=c.generation-1; return false end
  io.write(f,data); io.close(f)
  if read(p)~=data then c.generation=c.generation-1; return false end
  return true
end
return M
