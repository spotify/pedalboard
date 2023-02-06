# This file is not intended to be executed - it's intended to be used to ensure MyPy compatibility!

from pedalboard import Pedalboard, Compressor, Delay, Gain, PitchShift, Reverb, Mix

passthrough = Gain(gain_db=0)

delay_and_pitch_shift = Pedalboard(
    [
        Delay(delay_seconds=0.25, mix=1.0),
        PitchShift(semitones=7),
        Gain(gain_db=-3),
    ]
)

delay_longer_and_more_pitch_shift = Pedalboard(
    [
        Delay(delay_seconds=0.5, mix=1.0),
        PitchShift(semitones=12),
        Gain(gain_db=-6),
    ]
)

board = Pedalboard(
    [
        # Put a compressor at the front of the chain:
        Compressor(),
        # Run all of these pedalboards simultaneously with the Mix plugin:
        Mix(
            [
                passthrough,
                delay_and_pitch_shift,
                delay_longer_and_more_pitch_shift,
            ]
        ),
        # Add a reverb on the final mix:
        Reverb(),
    ]
)
