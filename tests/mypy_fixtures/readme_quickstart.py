# This file is not intended to be executed - it's intended to be used to ensure MyPy compatibility!

from pedalboard import Pedalboard, Chorus, Reverb
from pedalboard.io import AudioFile

# Make a Pedalboard object, containing multiple audio plugins:
board = Pedalboard([Chorus(), Reverb(room_size=0.25)])

# Open an audio file for reading, just like a regular file:
with AudioFile("some-file.wav") as f:
    # Open an audio file to write to:
    with AudioFile("output.wav", "w", f.samplerate, f.num_channels) as o:
        # Read one second of audio at a time, until the file is empty:
        while f.tell() < f.frames:
            chunk = f.read(int(f.samplerate))

            # Run the audio through our pedalboard:
            effected = board(chunk, f.samplerate, reset=False)

            # Write the output to our output file:
            o.write(effected)
