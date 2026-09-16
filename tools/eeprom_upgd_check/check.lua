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
  mixt= {name = "Mix1", source = MIXSRC_Rud, weight = 50, offset = 10, curveType = 1, curveValue = 50}
  outputt = {name = "Out1", min = -800, max = 800, offset = 10, ppmCenter = 10, symetrical = 1, revert = 1, switch = 3}
  timert = {mode = 4, start = 0, countdownBeep = 2, persistent = 1, minuteBeep = true, value = 10 }
  mix = model.getMix(4,0)
  output = model.getOutput(4)
  timer = model.getTimer(0)
  checktable(mix, mixt)
  checktable(output, outputt)
  checktable(timer, timert)
  return 2
end
return {run=run}
