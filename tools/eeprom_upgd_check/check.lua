local function checktable(actual, reference)
  for k, v in pairs( actual ) do
    if v == reference[k] then
      print("OK ", k, v, reference[k])
    else
      print("ISSUE ", k, v, reference[k])
    end
  end
end

local function run(event)
  mixt= {name = "Mix1", source = MIXSRC_Rud, weight = 50, offset = 10}
  timert = {mode = 4, start = 0, countdownBeep = 2, persistent = 1, minuteBeep = true, value = 10 }
  mix = model.getMix(4,0)
  timer = model.getTimer(0)
  checktable(mix, mixt)
  checktable(timer, timert)
  return 2
end
return {run=run}
