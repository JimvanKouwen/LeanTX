local function run(event)
  inputt= {name = "In1", source  = 1, weight = 50, offset = 10}
  mixt= {name = "Mix1", source = 1, weight = 50, offset = 10, curveType = 1, curveValue = 50}
  outputt = {name = "Out1", min = -800, max = 800, offset = 10, ppmCenter = 10, symetrical = 1, revert = 1, switch = 3}
  timert = {mode = 4, start = 0, countdownBeep = 2, persistent = 1, minuteBeep = true, value = 10 }
  model.insertInput(4,0,inputt)
  model.insertMix(4,0,mixt)
  model.setOutput(4, outputt)
  model.setTimer(0, timert)
  return 2
end
return {run=run}
