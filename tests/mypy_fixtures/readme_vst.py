# This file is not intended to be executed - it's intended to be used to ensure MyPy compatibility!

from pedalboard import Pedalboard, Reverb, load_plugin
from pedalboard.io import AudioFile

# Load a VST3 or Audio Unit plugin from a known path on disk:
vst = load_plugin("./VSTs/RoughRider3.vst3")

# TODO: Add type hints for `parameters` here!
print(vst.parameters.keys())  # type: ignore
# dict_keys([
#   'sc_hpf_hz', 'input_lvl_db', 'sensitivity_db',
#   'ratio', 'attack_ms', 'release_ms', 'makeup_db',
#   'mix', 'output_lvl_db', 'sc_active',
#   'full_bandwidth', 'bypass', 'program',
# ])

# Set the "ratio" parameter to 15
vst.ratio = 15  # type: ignore

# Use this VST to process some audio:
with AudioFile("some-file.wav", "r") as f:
    audio = f.read(f.frames)
    samplerate = f.samplerate
effected = vst(audio, samplerate)

# ...or put this VST into a chain with other plugins:
board = Pedalboard([vst, Reverb()])
# ...and run that pedalboard with the same VST instance!
effected = board(audio, samplerate)
