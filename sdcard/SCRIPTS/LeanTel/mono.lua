-- SPDX-License-Identifier: GPL-2.0-only
local M={color=false,top=9,bottom=8,row=9}
function M.begin(w,h,title,footer)
  lcd.clear(); lcd.drawText(0,0,title,SMLSIZE)
  lcd.drawLine(0,8,w-1,8,SOLID,FORCE)
  lcd.drawText(0,h-7,footer,SMLSIZE)
end
function M.text(x,y,text,selected,big)
  lcd.drawText(x,y,text,(big and MIDSIZE or SMLSIZE)+(selected and INVERS or 0))
end
function M.card(x,y,w,h,label,value,selected,stale,hero)
  if selected then lcd.drawRectangle(x,y,w,h) end
  if h<20 then
    M.text(x+2,y+1,label:sub(1,4),false,false)
    M.text(x+2,y+8,(stale and '~' or '')..value,false,false)
    return
  end
  M.text(x+2,y+1,label,false,false)
  -- Small cells reserve enough room for both label and value on 64px screens.
  M.text(x+2,y+9,(stale and '~' or '')..value,false,hero and h>=30 and #value<=8)
end
function M.line(x1,y1,x2,y2) lcd.drawLine(x1,y1,x2,y2,SOLID,FORCE) end
return M
