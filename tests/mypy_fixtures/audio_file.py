# This file is not intended to be executed - it's intended to be used to ensure MyPy compatibility!

from pedalboard.io import AudioFile

with AudioFile("some_path") as f:
    f.read(f.frames)

with open("some_path", "rb") as f:
    with AudioFile(f) as r:
        r.read(r.frames)
