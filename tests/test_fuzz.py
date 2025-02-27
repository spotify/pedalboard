import pytest
import numpy as np
from pedalboard import Fuzz, Pedalboard

NUM_SECONDS = 1
MAX_SAMPLE_RATE = 96000
NOISE = np.random.rand(int(NUM_SECONDS * MAX_SAMPLE_RATE)).astype(np.float32)


def test_default_values():
    # Creates the Fuzz instance with default values ​​(drive_db=25, tone_hz=800)
    fuzz = Fuzz()
    assert fuzz.drive_db == 25, "The default drive_db value should be 25"
    assert fuzz.tone_hz == 800, "The default tone_hz value should be 800"


def test_custom_initialization():
    # Create an instance with custom parameters and verify that they are set correctly
    drive_db = 30
    tone_hz = 1200
    fuzz = Fuzz(drive_db, tone_hz)
    assert fuzz.drive_db == drive_db, "The value of drive_db does not match the initialized one"
    assert fuzz.tone_hz == tone_hz, "The value of tone_hz does not match the initialized one"


def test_property_setters():
    # Edit the properties and check that the setters are working
    fuzz = Fuzz()
    fuzz.drive_db = 40
    fuzz.tone_hz = 900
    assert fuzz.drive_db == 40, "The setter for drive_db did not update the value correctly"
    assert fuzz.tone_hz == 900, "The setter for tone_hz did not update the value correctly"


def test_repr():
    # Check if __repr__ method returns a string containing the expected information
    fuzz = Fuzz(35, 950)
    rep = repr(fuzz)
    assert "drive_db=35" in rep, "__repr__ must contains drive_db value"
    assert "tone_hz=950" in rep, "__repr__ must contains tone_hz value"
    assert "pedalboard.Fuzz" in rep, "__repr__ must indicate plugin type"


def test_fuzz_process_silence():
    fuzz = Fuzz()
    sample_rate = 44100
    num_samples = sample_rate  # mono audio - 1 sec
    audio_in = np.zeros((num_samples, 1), dtype=np.float32)

    audio_out = fuzz.process(audio_in, sample_rate)
    np.testing.assert_array_almost_equal(audio_out, audio_in, decimal=5)


def test_fuzz_process_signal():
    drive_db = 30
    tone_hz = 800
    sample_rate = 44100
    _input = NOISE[: int(NUM_SECONDS * sample_rate)]
    fuzz = Fuzz(drive_db, tone_hz)
    audio_in = np.column_stack((_input, _input))

    audio_out = fuzz.process(audio_in, sample_rate)
    assert audio_out.shape == audio_in.shape, "The form of the output must be the same as the input"
    with pytest.raises(AssertionError):
        np.testing.assert_array_almost_equal(audio_out, audio_in, decimal=5)


@pytest.mark.parametrize("drive_db", [25., 35., 45.])
@pytest.mark.parametrize("tone_hz", [800, 1000, 4000, 12000])
def test_fuzz_in_pedalboard(drive_db, tone_hz):
    fuzz_effect = Fuzz(drive_db=drive_db, tone_hz=tone_hz)
    pb = Pedalboard([fuzz_effect])

    assert pb[0] == fuzz_effect