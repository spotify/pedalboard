![Pedalboard Logo](https://user-images.githubusercontent.com/213293/131147303-4805181a-c7d5-4afe-afb2-f591a4b8e586.png)


[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://github.com/spotify/pedalboard/blob/master/LICENSE)
[![Documentation](https://img.shields.io/badge/Documentation-on%20github.io-brightgreen)](https://spotify.github.io/pedalboard)
[![PyPI - Python Version](https://img.shields.io/pypi/pyversions/pedalboard)](https://pypi.org/project/pedalboard)
[![Supported Platforms](https://img.shields.io/badge/platforms-macOS%20%7C%20Windows%20%7C%20Linux-green)](https://pypi.org/project/pedalboard)
[![Apple Silicon support for macOS and Linux (Docker)](https://img.shields.io/badge/Apple%20Silicon-macOS%20and%20Linux-brightgreen)](https://pypi.org/project/pedalboard)
[![PyPI - Wheel](https://img.shields.io/pypi/wheel/pedalboard)](https://pypi.org/project/pedalboard)
[![Test Badge](https://github.com/spotify/pedalboard/actions/workflows/all.yml/badge.svg)](https://github.com/spotify/pedalboard/actions/workflows/all.yml)
[![Coverage Badge](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/psobot/8736467e9952991ef44a67915ee7c762/raw/coverage.json)](https://gist.githubusercontent.com/psobot/8736467e9952991ef44a67915ee7c762/raw/coverage.json)
[![PyPI - Downloads](https://img.shields.io/pypi/dm/pedalboard)](https://pypistats.org/packages/pedalboard)
[![GitHub Repo stars](https://img.shields.io/github/stars/spotify/pedalboard?style=social)](https://github.com/spotify/pedalboard/stargazers)

`pedalboard` is a Python library for working with audio: reading, writing, adding effects, and more. It supports most popular audio file formats and a number of common audio effects out of the box, and also allows the use of [VST3®](https://www.steinberg.net/en/company/technologies/vst3.html) and [Audio Unit](https://en.wikipedia.org/wiki/Audio_Units) formats for third-party plugins.

`pedalboard` was built by [Spotify's Audio Intelligence Lab](https://research.atspotify.com/audio-intelligence/) to enable using studio-quality audio effects from within Python and TensorFlow. Internally at Spotify, `pedalboard` is used for [data augmentation](https://en.wikipedia.org/wiki/Data_augmentation) to improve machine learning models. `pedalboard` also helps in the process of content creation, making it possible to add effects to audio without using a Digital Audio Workstation.

[![Documentation](https://img.shields.io/badge/Documentation-on%20github.io-brightgreen)](https://spotify.github.io/pedalboard)

## Features 

 - Built-in audio I/O utilities ([pedalboard.io](https://spotify.github.io/pedalboard/reference/pedalboard.io.html))
   - Support for reading and writing AIFF, FLAC, MP3, OGG, and WAV files on all platforms with no dependencies
   - Additional support for reading AAC, AC3, WMA, and other formats depending on platform
   - Support for on-the-fly resampling of audio files and streams with `O(1)` memory usage
 - Built-in support for a number of basic audio transformations, including:
   - Guitar-style effects: `Chorus`, `Distortion`, `Phaser`, `Clipping`
   - Loudness and dynamic range effects: `Compressor`, `Gain`, `Limiter`
   - Equalizers and filters: `HighpassFilter`, `LadderFilter`, `LowpassFilter`
   - Spatial effects: `Convolution`, `Delay`, `Reverb`
   - Pitch effects: `PitchShift`
   - Lossy compression: `GSMFullRateCompressor`, `MP3Compressor`
   - Quality reduction: `Resample`, `Bitcrush`
 - Supports VST3® plugins on macOS, Windows, and Linux ([pedalboard.load_plugin](https://spotify.github.io/pedalboard/reference/pedalboard.html#pedalboard.load_plugin))
 - Supports Audio Units on macOS
 - Strong thread-safety, memory usage, and speed guarantees
   - Releases Python's Global Interpreter Lock (GIL) to allow use of multiple CPU cores
     - No need to use `multiprocessing`!
   - Even when only using one thread:
     - Processes audio up to **300x** faster than [pySoX](https://github.com/rabitt/pysox) for single transforms, and 2-5x faster than [SoxBindings](https://github.com/pseeth/soxbindings) (via [iCorv](https://github.com/iCorv/pedalboard_with_tfdata))
     - Reads audio files up to **4x** faster than [librosa.load](https://librosa.org/doc/main/generated/librosa.load.html) (in many cases)
 - Tested compatibility with TensorFlow - can be used in `tf.data` pipelines!

## Installation

`pedalboard` is available via PyPI (via [Platform Wheels](https://packaging.python.org/guides/distributing-packages-using-setuptools/#platform-wheels)):
```
pip install pedalboard  # That's it! No other dependencies required.
```

If you are new to Python, follow [INSTALLATION.md](https://github.com/spotify/pedalboard/blob/master/INSTALLATION.md) for a robust guide.

### Compatibility

`pedalboard` is thoroughly tested with Python 3.6, 3.7, 3.8, 3.9, 3.10, and 3.11 as well as experimental support for PyPy 3.7, 3.8, and 3.9.

- Linux
  - Tested heavily in production use cases at Spotify
  - Tested automatically on GitHub with VSTs
  - Platform `manylinux` wheels built for `x86_64` (Intel/AMD) and `aarch64` (ARM/Apple Silicon)
  - Most Linux VSTs require a relatively modern Linux installation (with glibc > 2.27)
- macOS
  - Tested manually with VSTs and Audio Units
  - Tested automatically on GitHub with VSTs
  - Platform wheels available for both Intel and Apple Silicon
  - Compatible with a wide range of VSTs and Audio Units
- Windows
  - Tested automatically on GitHub with VSTs
  - Platform wheels available for `amd64` (x86-64, Intel/AMD)

## Examples

### Quick start

```python
from pedalboard import Pedalboard, Chorus, Reverb
from pedalboard.io import AudioFile

# Read in a whole audio file:
with AudioFile('some-file.wav') as f:
  audio = f.read(f.frames)
  samplerate = f.samplerate

# Make a Pedalboard object, containing multiple plugins:
board = Pedalboard([Chorus(), Reverb(room_size=0.25)])

# Run the audio through this pedalboard!
effected = board(audio, samplerate)

# Write the audio back as a wav file:
with AudioFile('processed-output.wav', 'w', samplerate, effected.shape[0]) as f:
  f.write(effected)
```

### Making a guitar-style pedalboard

```python
# Don't do import *! (It just makes this example smaller)
from pedalboard import *
from pedalboard.io import AudioFile

# Read in a whole file, resampling to our desired sample rate:
samplerate = 44100.0
with AudioFile('guitar-input.wav').resampled_to(samplerate) as f:
  audio = f.read(f.frames)

# Make a pretty interesting sounding guitar pedalboard:
board = Pedalboard([
    Compressor(threshold_db=-50, ratio=25),
    Gain(gain_db=30),
    Chorus(),
    LadderFilter(mode=LadderFilter.Mode.HPF12, cutoff_hz=900),
    Phaser(),
    Convolution("./guitar_amp.wav", 1.0),
    Reverb(room_size=0.25),
])

# Pedalboard objects behave like lists, so you can add plugins:
board.append(Compressor(threshold_db=-25, ratio=10))
board.append(Gain(gain_db=10))
board.append(Limiter())

# ... or change parameters easily:
board[0].threshold_db = -40

# Run the audio through this pedalboard!
effected = board(audio, samplerate)

# Write the audio back as a wav file:
with AudioFile('processed-output.wav', 'w', samplerate, effected.shape[0]) as f:
  f.write(effected)
```

### Using VST3® or Audio Unit plugins

```python
from pedalboard import Pedalboard, Reverb, load_plugin
from pedalboard.io import AudioFile

# Load a VST3 or Audio Unit plugin from a known path on disk:
vst = load_plugin("./VSTs/RoughRider3.vst3")

print(vst.parameters.keys())
# dict_keys([
#   'sc_hpf_hz', 'input_lvl_db', 'sensitivity_db',
#   'ratio', 'attack_ms', 'release_ms', 'makeup_db',
#   'mix', 'output_lvl_db', 'sc_active',
#   'full_bandwidth', 'bypass', 'program',
# ])

# Set the "ratio" parameter to 15
vst.ratio = 15

# Use this VST to process some audio:
with AudioFile('some-file.wav', 'r') as f:
  audio = f.read(f.frames)
  samplerate = f.samplerate
effected = vst(audio, samplerate)

# ...or put this VST into a chain with other plugins:
board = Pedalboard([vst, Reverb()])
# ...and run that pedalboard with the same VST instance!
effected = board(audio, samplerate)
```

### Creating parallel effects chains

This example creates a delayed pitch-shift effect by running
multiple Pedalboards in parallel on the same audio. `Pedalboard`
objects are themselves `Plugin` objects, so you can nest them
as much as you like:

```python
from pedalboard import Pedalboard, Compressor, Delay, Distortion, Gain, PitchShift, Reverb, Mix

passthrough = Gain(gain_db=0)

delay_and_pitch_shift = Pedalboard([
  Delay(delay_seconds=0.25, mix=1.0),
  PitchShift(semitones=7),
  Gain(gain_db=-3),
])

delay_longer_and_more_pitch_shift = Pedalboard([
  Delay(delay_seconds=0.5, mix=1.0),
  PitchShift(semitones=12),
  Gain(gain_db=-6),
])

board = Pedalboard([
  # Put a compressor at the front of the chain:
  Compressor(),
  # Run all of these pedalboards simultaneously with the Mix plugin:
  Mix([
    passthrough,
    delay_and_pitch_shift,
    delay_longer_and_more_pitch_shift,
  ]),
  # Add a reverb on the final mix:
  Reverb()
])
```

For more examples, see:
 - [the "examples" folder of this repository](https://github.com/spotify/pedalboard/tree/master/examples)
 - [the "Pedalboard Demo" Colab notebook](https://colab.research.google.com/drive/1bHjhJj1aCoOlXKl_lOfG99Xs3qWVrhch)
 - [an interactive web demo on Hugging Face Spaces and Gradio](https://huggingface.co/spaces/akhaliq/pedalboard) (via [@AK391](https://github.com/AK391)) 

## Contributing

Contributions to `pedalboard` are welcomed! See [CONTRIBUTING.md](https://github.com/spotify/pedalboard/blob/master/CONTRIBUTING.md) for details.

## License
`pedalboard` is Copyright 2021-2022 Spotify AB.

`pedalboard` is licensed under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.en.html). `pedalboard` includes a number of libraries that are statically compiled, and which carry the following licenses:

 - The core audio processing code is pulled from [JUCE 6](https://juce.com/), which is [dual-licensed under a commercial license and the GPLv3](https://juce.com/juce-6-licence).
 - The [VST3 SDK](https://github.com/steinbergmedia/vst3sdk), bundled with JUCE, is owned by [Steinberg® Media Technologies GmbH](https://www.steinberg.net/en/home.html) and licensed under the GPLv3.
 - The `PitchShift` plugin uses [the Rubber Band Library](https://github.com/breakfastquay/rubberband), which is [dual-licensed under a commercial license](https://breakfastquay.com/technology/license.html) and the GPLv2 (or newer).
 - The `MP3Compressor` plugin uses [libmp3lame from the LAME project](https://lame.sourceforge.io/), which is [licensed under the LGPLv2](https://github.com/lameproject/lame/blob/master/README) and [upgraded to the GPLv3 for inclusion in this project (as permitted by the LGPLv2)](https://www.gnu.org/licenses/gpl-faq.html#AllCompatibility).
 - The `GSMFullRateCompressor` plugin uses [libgsm](http://quut.com/gsm/), which is [licensed under the ISC license](https://github.com/timothytylee/libgsm/blob/master/COPYRIGHT) and [compatible with the GPLv3](https://www.gnu.org/licenses/license-list.en.html#ISC).

_VST is a registered trademark of Steinberg Media Technologies GmbH._
