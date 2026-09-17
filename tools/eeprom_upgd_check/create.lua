local function run(event)
  mixt= {name = "Mix1", source = MIXSRC_Rud, weight = 50, offset = 10}
  timert = {mode = 4, start = 0, countdownBeep = 2, persistent = 1, minuteBeep = true, value = 10 }
  model.insertMix(4,0,mixt)
  model.setTimer(0, timert)
  return 2
end
return {run=run}
