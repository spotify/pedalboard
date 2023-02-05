# This file is not intended to be executed - it's intended to be used to ensure MyPy compatibility!

# Don't do import *! (It just makes this example smaller)
from pedalboard import (
    Pedalboard,
    Compressor,
    Gain,
    Chorus,
    LadderFilter,
    Limiter,
    Phaser,
    Convolution,
    Reverb,
)
from pedalboard.io import AudioFile

# Read in a whole file, resampling to our desired sample rate:
samplerate = 44100.0
with AudioFile("guitar-input.wav").resampled_to(samplerate) as f:
    audio = f.read(f.frames)

# Make a pretty interesting sounding guitar pedalboard:
board = Pedalboard(
    [
        Compressor(threshold_db=-50, ratio=25),
        Gain(gain_db=30),
        Chorus(),
        LadderFilter(mode=LadderFilter.Mode.HPF12, cutoff_hz=900),
        Phaser(),
        Convolution("./guitar_amp.wav", 1.0),
        Reverb(room_size=0.25),
    ]
)

# Pedalboard objects behave like lists, so you can add plugins:
compressor = Compressor(threshold_db=-25, ratio=10)
board.append(compressor)
board.append(Gain(gain_db=10))
board.append(Limiter())

# ... or change parameters easily:
compressor.threshold_db = -40

# Run the audio through this pedalboard!
effected = board(audio, samplerate)

# Write the audio back as a wav file:
with AudioFile("processed-output.wav", "w", samplerate, effected.shape[0]) as f:
    f.write(effected)
