import mido

def note_on(time:float, note: int, velocity: int, channel: int = 1):
  return mido.Message('note_on',  note=note, velocity=velocity, channel=channel).bytes() + [time]

def note_off(time:float, note: int, velocity: int,channel: int = 1):
  return mido.Message('note_off', note=note, velocity=velocity, channel=channel).bytes() + [time]