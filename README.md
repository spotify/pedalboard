![Pedalboard Logo](https://user-images.githubusercontent.com/213293/125090786-bd93cf00-e09d-11eb-9c7f-a1363c0e733c.png)


[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
![PyPI - Python Version](https://img.shields.io/pypi/pyversions/pedalboard)
![Supported Platforms](https://img.shields.io/badge/platforms-macOS%20%7C%20Windows%20%7C%20Linux-green)
![Apple Silicon supported](https://img.shields.io/badge/Apple%20Silicon-supported-brightgreen)
![PyPI - Wheel](https://img.shields.io/pypi/wheel/pedalboard)
[![Test Badge](https://github.com/spotify/pedalboard/actions/workflows/all.yml/badge.svg)](https://github.com/spotify/pedalboard/actions/workflows/all.yml)
![Coverage Badge](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/psobot/8736467e9952991ef44a67915ee7c762/raw/coverage.json)
![GitHub Repo stars](https://img.shields.io/github/stars/spotify/pedalboard?style=social)

`pedalboard` is a Python library for adding effects to audio. It supports a number of common audio effects out of the box, and also allows the use of [VST3](https://www.steinberg.net/en/company/technologies/vst3.html) and [Audio Unit](https://en.wikipedia.org/wiki/Audio_Units) plugin formats for third-party effects. It was built by [Spotify's Audio Intelligence Lab](https://research.atspotify.com/audio-intelligence/) to enable using studio-quality audio effects from within Python and TensorFlow.

## Usage 

 - Built-in support for a number of basic audio transformations: 
   - `Convolution`
   - `Compressor`
   - `Chorus`
   - `Gain`
   - `HighpassFilter`
   - `Reverb`
   - `LadderFilter`
   - `Limiter`
   - `LowpassFilter`
   - `Phaser`
 - Supports VST3 plugins on macOS, Windows, and Linux
 - Supports Audio Units on macOS
 - Strong thread-safety, memory usage, and speed guarantees
   - Releases Python's Global Interpreter Lock (GIL) to allow use of multiple CPU cores
     - No need to use `multiprocessing`!
   - Even when only using one thread:
     - Processes audio roughly **300x** faster than [pySoX](https://github.com/rabitt/pysox)
 - Tested compatibility with TensorFlow - can be used in `tf.data` pipelines!

## Installation

`pedalboard` is available via PyPI (via [Platform Wheels](https://packaging.python.org/guides/distributing-packages-using-setuptools/#platform-wheels)):
```
pip install pedalboard
```

### Compatibility

`pedalboard` is thoroughly tested with Python 3.6, 3.7, 3.8, and 3.9, as well as experimental support for PyPy 7.3.

- Linux
  - Tested heavily in production use cases at Spotify
  - Tested automatically on GitHub with VSTs
  - Platform `manylinux` wheels built for `x86_64`
  - Most Linux VSTs require a relatively modern Linux installation (with glibc > 2.27)
- macOS
  - Tested manually with VSTs and Audio Units
  - Tested automatically on GitHub with VSTs
  - Platform wheels available for both Intel and Apple Silicon
  - Compatible with a wide range of VSTs and Audio Units
- Windows
  - Tested automatically on GitHub with VSTs
  - Platform wheels available for `amd64` (Intel/AMD)

## Examples

A very basic example of how to use `pedalboard`'s built-in plugins:

```python
import soundfile as sf
from pedalboard import (
    Pedalboard,
    Convolution,
    Compressor,
    Chorus,
    Gain,
    Reverb,
    Limiter,
    LadderFilter,
    Phaser,
)

audio, sample_rate = soundfile.read('some-file.wav')

# Make a Pedalboard object, containing multiple plugins:
board = Pedalboard([
    Compressor(threshold_db=-50, ratio=25),
    Gain(gain_db=30),
    Chorus(),
    LadderFilter(mode=LadderFilter.Mode.HPF12, cutoff_hz=900),
    Phaser(),
    Convolution("./guitar_amp.wav", 1.0),
    Reverb(room_size=0.25),
], sample_rate=sr)

# Pedalboard objects behave like lists, so you can add plugins:
board.append(Compressor(threshold_db=-25, ratio=10))
board.append(Gain(gain_db=10))
board.append(Limiter())

# Run the audio through this pedalboard!
effected = board(audio)

# Write the audio back as a wav file:
with sf.SoundFile('./processed-output-stereo.wav', 'w', samplerate=sr, channels=effected.shape[1]) as f:
    f.write(effected)

```

### Loading a VST3 plugin and manipulating its parameters

```python
import soundfile as sf
from pedalboard import Pedalboard, Reverb, load_plugin

# Load a VST3 package from a known path on disk:
vst = load_plugin("./VSTs/RoughRider3.vst3")

print(vst.parameters.keys())
# dict_keys([
#   'sc_hpf_hz',
#   'input_lvl_db',
#   'sensitivity_db',
#   'ratio',
#   'attack_ms',
#   'release_ms',
#   'makeup_db',
#   'mix',
#   'output_lvl_db',
#   'sc_active',
#   'full_bandwidth',
#   'bypass',
#   'program',
# ])

# Set the "ratio" parameter to 15
vst.ratio = 15

# Use this VST to process some audio:
audio, sample_rate = soundfile.read('some-file.wav')
effected = vst(audio, sample_rate=sample_rate)

# ...or put this VST into a chain with other plugins:
board = Pedalboard([vst, Reverb()], sample_rate=sample_rate)
# ...and run that pedalboard with the same VST instance!
effected = board(audio)
```

For more examples, see [the _Pedalboard Demo_ Jupyter notebook at `pedalboard-demo.ipynb`](https://github.com/spotify/pedalboard/blob/master/pedalboard-demo.ipynb).

## Contributing

Contributions to `pedalboard` are welcomed! See [CONTRIBUTING.md](https://github.com/spotify/pedalboard/blob/master/CONTRIBUTING.md) for details.

## License
`pedalboard` is Copyright 2021 Spotify AB.

`pedalboard` is licensed under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.en.html), because:
 - The core audio processing code is pulled from JUCE 6, which is [dual-licensed under a commercial license and the GPLv3](https://juce.com/juce-6-licence)
 - The VST3 SDK, bundled with JUCE, is owned by [Steinberg® Media Technologies GmbH](https://www.steinberg.net/en/home.html) and licensed under the GPLv3.

`pedalboard`'s logo contains artwork called
["guitar pedals" by Jino from the Noun Project](https://thenounproject.com/term/guitar-pedals/3605562), and the wordmark uses modified glyphs from [Victor Mono](https://github.com/rubjo/victor-mono).
