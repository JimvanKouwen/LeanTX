-- SPDX-License-Identifier: GPL-2.0-only
local M={color=true,top=42,bottom=32,row=30}
local function ink(rgb) lcd.setColor(CUSTOM_COLOR,lcd.RGB(rgb[1],rgb[2],rgb[3])); return CUSTOM_COLOR end
local white={228,235,244}; local muted={151,170,193}; local accent={62,211,179}
function M.begin(w,h,title,footer)
  lcd.drawFilledRectangle(0,0,w,h,ink({16,23,34}))
  lcd.drawText(12,8,title,ink(white))
  lcd.drawText(12,h-27,footer,SMLSIZE+ink(muted))
end
function M.text(x,y,text,selected,big)
  lcd.drawText(x,y,text,(big and DBLSIZE or SMLSIZE)+ink(selected and accent or white))
end
function M.card(x,y,w,h,label,value,selected,stale,hero)
  lcd.drawFilledRectangle(x+3,y+3,w-6,h-6,ink({27,39,54}))
  if selected then lcd.drawRectangle(x+3,y+3,w-6,h-6,ink(accent),2) end
  lcd.drawText(x+12,y+8,label,SMLSIZE+ink(muted))
  local font=(h>=80 and #value<=10) and DBLSIZE or 0
  lcd.drawText(x+12,y+math.max(28,math.floor(h/2)-8),(stale and '~ ' or '')..value,font+ink(stale and {245,181,74} or white))
end
function M.line(x1,y1,x2,y2) lcd.drawLine(x1,y1,x2,y2,SOLID,ink(accent)) end
return M
