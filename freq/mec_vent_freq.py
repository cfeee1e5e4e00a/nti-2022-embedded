def mechanical_ventilation_freq(lungs_volume, prev_lv, timer, freq):
  if (lungs_volume - prev_lv) < 0:
    timer = 0
    freq = 60 / timer
  else:
    timer += 0.1
  prev_lv = lungs_volume
  return freq