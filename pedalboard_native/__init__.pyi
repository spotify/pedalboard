"""This module provides classes and functions for generating and adding effects to audio. Most classes in this module are subclasses of ``Plugin``, each of which allows applying effects to an audio buffer or stream.

For audio I/O classes (i.e.: reading and writing audio files), see ``pedalboard.io``."""

from __future__ import annotations
import pedalboard_native

import typing

original_overload = typing.overload
__OVERLOADED_DOCSTRINGS = {}

def patch_overload(func):
    original_overload(func)
    if func.__doc__:
        __OVERLOADED_DOCSTRINGS[func.__qualname__] = func.__doc__
    else:
        func.__doc__ = __OVERLOADED_DOCSTRINGS.get(func.__qualname__)
    if func.__doc__:
        # Work around the fact that pybind11-stubgen generates
        # duplicate docstrings sometimes, once for each overload:
        docstring = func.__doc__
        if docstring[len(docstring) // 2 :].strip() == docstring[: -len(docstring) // 2].strip():
            func.__doc__ = docstring[len(docstring) // 2 :].strip()
    return func

typing.overload = patch_overload

from typing_extensions import Literal
from enum import Enum
import threading
import numpy

_Shape = typing.Tuple[int, ...]

__all__ = [
    "AudioUnitPlugin",
    "Bitcrush",
    "Chorus",
    "Clipping",
    "Compressor",
    "Convolution",
    "Delay",
    "Distortion",
    "ExternalPlugin",
    "GSMFullRateCompressor",
    "Gain",
    "HighShelfFilter",
    "HighpassFilter",
    "IIRFilter",
    "Invert",
    "LadderFilter",
    "Limiter",
    "LowShelfFilter",
    "LowpassFilter",
    "MP3Compressor",
    "NoiseGate",
    "PeakFilter",
    "Phaser",
    "PitchShift",
    "Plugin",
    "PluginContainer",
    "Resample",
    "Reverb",
    "VST3Plugin",
    "io",
    "process",
    "utils",
]

class Plugin:
    """
    A generic audio processing plugin. Base class of all Pedalboard plugins.
    """

    def __call__(
        self,
        input_array: numpy.ndarray,
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Run an audio buffer through this plugin. Alias for :py:meth:`process`.
        """

    def process(
        self,
        input_array: numpy.ndarray,
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Run a 32-bit or 64-bit floating point audio buffer through this plugin.
        (If calling this multiple times with multiple plugins, consider creating a
        :class:`pedalboard.Pedalboard` object instead.)

        The returned array may contain up to (but not more than) the same number of
        samples as were provided. If fewer samples were returned than expected, the
        plugin has likely buffered audio inside itself. To receive the remaining
        audio, pass another audio buffer into ``process`` with ``reset`` set to
        ``True``.

        If the provided buffer uses a 64-bit datatype, it will be converted to 32-bit
        for processing.

        The provided ``buffer_size`` argument will be used to control the size of
        each chunk of audio provided to the plugin. Higher buffer sizes may speed up
        processing at the expense of memory usage.

        The ``reset`` flag determines if all of the plugins should be reset before
        processing begins, clearing any state from previous calls to ``process``.
        If calling ``process`` multiple times while processing the same audio file
        or buffer, set ``reset`` to ``False``.

        The layout of the provided ``input_array`` will be automatically detected,
        assuming that the smaller dimension corresponds with the number of channels.
        If the number of samples and the number of channels are the same, each
        :py:class:`Plugin` object will use the last-detected channel layout until
        :py:meth:`reset` is explicitly called (as of v0.9.9).

        .. note::
            The :py:meth:`process` method can also be used via :py:meth:`__call__`;
            i.e.: just calling this object like a function (``my_plugin(...)``) will
            automatically invoke :py:meth:`process` with the same arguments.
        """

    def reset(self) -> None:
        """
        Clear any internal state stored by this plugin (e.g.: reverb tails, delay lines, LFO state, etc). The values of plugin parameters will remain unchanged.
        """

    @property
    def is_effect(self) -> bool:
        """
        True iff this plugin is an audio effect and accepts audio as input.

        *Introduced in v0.7.4.*


        """

    @property
    def is_instrument(self) -> bool:
        """
        True iff this plugin is not an audio effect and accepts only MIDI input, not audio.

        *Introduced in v0.7.4.*


        """
    pass

class Bitcrush(Plugin):
    """
    A plugin that reduces the signal to a given bit depth, giving the audio a lo-fi, digitized sound. Floating-point bit depths are supported.

    Bitcrushing changes the amount of "vertical" resolution used for an audio signal (i.e.: how many unique values could be used to represent each sample). For an effect that changes the "horizontal" resolution (i.e.: how many samples are available per second), see :class:`pedalboard.Resample`.
    """

    def __init__(self, bit_depth: float = 8) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def bit_depth(self) -> float:
        """
        The bit depth to quantize the signal to. Must be between 0 and 32 bits. May be an integer, decimal, or floating-point value. Each audio sample will be quantized onto ``2 ** bit_depth`` values.


        """

    @bit_depth.setter
    def bit_depth(self, arg1: float) -> None:
        """
        The bit depth to quantize the signal to. Must be between 0 and 32 bits. May be an integer, decimal, or floating-point value. Each audio sample will be quantized onto ``2 ** bit_depth`` values.
        """
    pass

class Chorus(Plugin):
    """
    A basic chorus effect.

    This audio effect can be controlled via the speed and depth of the LFO controlling the frequency response, a mix control, a feedback control, and the centre delay of the modulation.

    Note: To get classic chorus sounds try to use a centre delay time around 7-8 ms with a low feeback volume and a low depth. This effect can also be used as a flanger with a lower centre delay time and a lot of feedback, and as a vibrato effect if the mix value is 1.
    """

    def __init__(
        self,
        rate_hz: float = 1.0,
        depth: float = 0.25,
        centre_delay_ms: float = 7.0,
        feedback: float = 0.0,
        mix: float = 0.5,
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def centre_delay_ms(self) -> float:
        """ """

    @centre_delay_ms.setter
    def centre_delay_ms(self, arg1: float) -> None:
        pass

    @property
    def depth(self) -> float:
        """ """

    @depth.setter
    def depth(self, arg1: float) -> None:
        pass

    @property
    def feedback(self) -> float:
        """ """

    @feedback.setter
    def feedback(self, arg1: float) -> None:
        pass

    @property
    def mix(self) -> float:
        """ """

    @mix.setter
    def mix(self, arg1: float) -> None:
        pass

    @property
    def rate_hz(self) -> float:
        """
        The speed of the chorus effect's low-frequency oscillator (LFO), in Hertz. This value must be between 0 Hz and 100 Hz.


        """

    @rate_hz.setter
    def rate_hz(self, arg1: float) -> None:
        """
        The speed of the chorus effect's low-frequency oscillator (LFO), in Hertz. This value must be between 0 Hz and 100 Hz.
        """
    pass

class Clipping(Plugin):
    """
    A distortion plugin that adds hard distortion to the signal by clipping the signal at the provided threshold (in decibels).
    """

    def __init__(self, threshold_db: float = -6.0) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def threshold_db(self) -> float:
        """ """

    @threshold_db.setter
    def threshold_db(self, arg1: float) -> None:
        pass
    pass

class Compressor(Plugin):
    """
    A dynamic range compressor, used to reduce the volume of loud sounds and "compress" the loudness of the signal.

    For a lossy compression algorithm that introduces noise or artifacts, see ``pedalboard.MP3Compressor`` or ``pedalboard.GSMCompressor``.
    """

    def __init__(
        self,
        threshold_db: float = 0,
        ratio: float = 1,
        attack_ms: float = 1.0,
        release_ms: float = 100,
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def attack_ms(self) -> float:
        """ """

    @attack_ms.setter
    def attack_ms(self, arg1: float) -> None:
        pass

    @property
    def ratio(self) -> float:
        """ """

    @ratio.setter
    def ratio(self, arg1: float) -> None:
        pass

    @property
    def release_ms(self) -> float:
        """ """

    @release_ms.setter
    def release_ms(self, arg1: float) -> None:
        pass

    @property
    def threshold_db(self) -> float:
        """ """

    @threshold_db.setter
    def threshold_db(self, arg1: float) -> None:
        pass
    pass

class Convolution(Plugin):
    """
    An audio convolution, suitable for things like speaker simulation or reverb modeling.

    The convolution impulse response can be specified either by filename or as a 32-bit floating point NumPy array. If a NumPy array is provided, the ``sample_rate`` argument must also be provided to indicate the sample rate of the impulse response.

    *Support for passing NumPy arrays as impulse responses introduced in v0.9.10.*
    """

    def __init__(
        self,
        impulse_response_filename: typing.Union[
            str, numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]
        ],
        mix: float = 1.0,
        sample_rate: typing.Optional[float] = None,
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def impulse_response(
        self,
    ) -> typing.Optional[numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]]:
        """ """

    @property
    def impulse_response_filename(self) -> typing.Optional[str]:
        """ """

    @property
    def mix(self) -> float:
        """ """

    @mix.setter
    def mix(self, arg1: float) -> None:
        pass
    pass

class Delay(Plugin):
    """
    A digital delay plugin with controllable delay time, feedback percentage, and dry/wet mix.
    """

    def __init__(
        self, delay_seconds: float = 0.5, feedback: float = 0.0, mix: float = 0.5
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def delay_seconds(self) -> float:
        """ """

    @delay_seconds.setter
    def delay_seconds(self, arg1: float) -> None:
        pass

    @property
    def feedback(self) -> float:
        """ """

    @feedback.setter
    def feedback(self, arg1: float) -> None:
        pass

    @property
    def mix(self) -> float:
        """ """

    @mix.setter
    def mix(self, arg1: float) -> None:
        pass
    pass

class Distortion(Plugin):
    """
    A distortion effect, which applies a non-linear (``tanh``, or hyperbolic tangent) waveshaping function to apply harmonically pleasing distortion to a signal.

    This plugin produces a signal that is roughly equivalent to running: ``def distortion(x): return tanh(x * db_to_gain(drive_db))``
    """

    def __init__(self, drive_db: float = 25) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def drive_db(self) -> float:
        """ """

    @drive_db.setter
    def drive_db(self, arg1: float) -> None:
        pass
    pass

class ExternalPlugin(Plugin):
    """
    A wrapper around a third-party effect plugin.

    Don't use this directly; use one of :class:`pedalboard.VST3Plugin` or :class:`pedalboard.AudioUnitPlugin` instead.
    """

    @typing.overload
    def __call__(
        self,
        input_array: numpy.ndarray,
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Run an audio or MIDI buffer through this plugin, returning audio. Alias for :py:meth:`process`.

        Run an audio or MIDI buffer through this plugin, returning audio. Alias for :py:meth:`process`.
        """

    @typing.overload
    def __call__(
        self,
        midi_messages: object,
        duration: float,
        sample_rate: float,
        num_channels: int = 2,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]: ...
    @typing.overload
    def process(
        self,
        midi_messages: object,
        duration: float,
        sample_rate: float,
        num_channels: int = 2,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Pass a buffer of audio (as a 32- or 64-bit NumPy array) *or* a list of
        MIDI messages to this plugin, returning audio.

        (If calling this multiple times with multiple effect plugins, consider
        creating a :class:`pedalboard.Pedalboard` object instead.)

        When provided audio as input, the returned array may contain up to (but not
        more than) the same number of samples as were provided. If fewer samples
        were returned than expected, the plugin has likely buffered audio inside
        itself. To receive the remaining audio, pass another audio buffer into
        ``process`` with ``reset`` set to ``True``.

        If the provided buffer uses a 64-bit datatype, it will be converted to 32-bit
        for processing.

        If provided MIDI messages as input, the provided ``midi_messages`` must be
        a Python ``List`` containing one of the following types:

         - Objects with a ``bytes()`` method and ``time`` property (such as :doc:`mido:messages`
           from :doc:`mido:index`, not included with Pedalboard)
         - Tuples that look like: ``(midi_bytes: bytes, timestamp_in_seconds: float)``
         - Tuples that look like: ``(midi_bytes: List[int], timestamp_in_seconds: float)``

        The returned array will contain ``duration`` seconds worth of audio at the
        provided ``sample_rate``.

        Each MIDI message will be sent to the plugin at its
        timestamp, where a timestamp of ``0`` indicates the start of the buffer, and
        a timestamp equal to ``duration`` indicates the end of the buffer. (Any MIDI
        messages whose timestamps are greater than ``duration`` will be ignored.)

        The provided ``buffer_size`` argument will be used to control the size of
        each chunk of audio returned by the plugin at once. Higher buffer sizes may
        speed up processing, but may cause increased memory usage.

        The ``reset`` flag determines if this plugin should be reset before
        processing begins, clearing any state from previous calls to ``process``.
        If calling ``process`` multiple times while processing the same audio or
        MIDI stream, set ``reset`` to ``False``.

        .. note::
            The :py:meth:`process` method can also be used via :py:meth:`__call__`;
            i.e.: just calling this object like a function (``my_plugin(...)``) will
            automatically invoke :py:meth:`process` with the same arguments.


        Examples
        --------

        Running audio through an external effect plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_effect
           with AudioFile("input-audio.wav") as f:
               output_audio = plugin(f.read(), f.samplerate)


        Rendering MIDI via an external instrument plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile
           from mido import Message # not part of Pedalboard, but convenient!

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_instrument

           sample_rate = 44100
           num_channels = 2
           with AudioFile("output-audio.wav", "w", sample_rate, num_channels) as f:
               f.write(plugin(
                   [Message("note_on", note=60), Message("note_off", note=60, time=4)],
                   sample_rate=sample_rate,
                   duration=5,
                   num_channels=num_channels
               ))


        *Support for instrument plugins introduced in v0.7.4.*



        Pass a buffer of audio (as a 32- or 64-bit NumPy array) *or* a list of
        MIDI messages to this plugin, returning audio.

        (If calling this multiple times with multiple effect plugins, consider
        creating a :class:`pedalboard.Pedalboard` object instead.)

        When provided audio as input, the returned array may contain up to (but not
        more than) the same number of samples as were provided. If fewer samples
        were returned than expected, the plugin has likely buffered audio inside
        itself. To receive the remaining audio, pass another audio buffer into
        ``process`` with ``reset`` set to ``True``.

        If the provided buffer uses a 64-bit datatype, it will be converted to 32-bit
        for processing.

        If provided MIDI messages as input, the provided ``midi_messages`` must be
        a Python ``List`` containing one of the following types:

         - Objects with a ``bytes()`` method and ``time`` property (such as :doc:`mido:messages`
           from :doc:`mido:index`, not included with Pedalboard)
         - Tuples that look like: ``(midi_bytes: bytes, timestamp_in_seconds: float)``
         - Tuples that look like: ``(midi_bytes: List[int], timestamp_in_seconds: float)``

        The returned array will contain ``duration`` seconds worth of audio at the
        provided ``sample_rate``.

        Each MIDI message will be sent to the plugin at its
        timestamp, where a timestamp of ``0`` indicates the start of the buffer, and
        a timestamp equal to ``duration`` indicates the end of the buffer. (Any MIDI
        messages whose timestamps are greater than ``duration`` will be ignored.)

        The provided ``buffer_size`` argument will be used to control the size of
        each chunk of audio returned by the plugin at once. Higher buffer sizes may
        speed up processing, but may cause increased memory usage.

        The ``reset`` flag determines if this plugin should be reset before
        processing begins, clearing any state from previous calls to ``process``.
        If calling ``process`` multiple times while processing the same audio or
        MIDI stream, set ``reset`` to ``False``.

        .. note::
            The :py:meth:`process` method can also be used via :py:meth:`__call__`;
            i.e.: just calling this object like a function (``my_plugin(...)``) will
            automatically invoke :py:meth:`process` with the same arguments.


        Examples
        --------

        Running audio through an external effect plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_effect
           with AudioFile("input-audio.wav") as f:
               output_audio = plugin(f.read(), f.samplerate)


        Rendering MIDI via an external instrument plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile
           from mido import Message # not part of Pedalboard, but convenient!

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_instrument

           sample_rate = 44100
           num_channels = 2
           with AudioFile("output-audio.wav", "w", sample_rate, num_channels) as f:
               f.write(plugin(
                   [Message("note_on", note=60), Message("note_off", note=60, time=4)],
                   sample_rate=sample_rate,
                   duration=5,
                   num_channels=num_channels
               ))


        *Support for instrument plugins introduced in v0.7.4.*

        """

    @typing.overload
    def process(
        self,
        input_array: numpy.ndarray,
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]: ...
    pass

class Gain(Plugin):
    """
    A gain plugin that increases or decreases the volume of a signal by amplifying or attenuating it by the provided value (in decibels). No distortion or other effects are applied.

    Think of this as a volume control.
    """

    def __init__(self, gain_db: float = 1.0) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def gain_db(self) -> float:
        """ """

    @gain_db.setter
    def gain_db(self, arg1: float) -> None:
        pass
    pass

class IIRFilter(Plugin):
    """
    An abstract class that implements various kinds of infinite impulse response (IIR) filter designs. This should not be used directly; use :class:`HighShelfFilter`, :class:`LowShelfFilter`, or :class:`PeakFilter` directly instead.
    """

    pass

class HighpassFilter(Plugin):
    """
    Apply a first-order high-pass filter with a roll-off of 6dB/octave. The cutoff frequency will be attenuated by -3dB (i.e.: :math:`\\frac{1}{\\sqrt{2}}` as loud, expressed as a gain factor) and lower frequencies will be attenuated by a further 6dB per octave.)
    """

    def __init__(self, cutoff_frequency_hz: float = 50) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def cutoff_frequency_hz(self) -> float:
        """ """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass
    pass

class HighShelfFilter(IIRFilter, Plugin):
    """
    A high shelf filter plugin with variable Q and gain, as would be used in an equalizer. Frequencies above the cutoff frequency will be boosted (or cut) by the provided gain (in decibels).
    """

    def __init__(
        self, cutoff_frequency_hz: float = 440, gain_db: float = 0.0, q: float = 0.7071067690849304
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def cutoff_frequency_hz(self) -> float:
        """ """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass

    @property
    def gain_db(self) -> float:
        """ """

    @gain_db.setter
    def gain_db(self, arg1: float) -> None:
        pass

    @property
    def q(self) -> float:
        """ """

    @q.setter
    def q(self, arg1: float) -> None:
        pass
    pass

class Invert(Plugin):
    """
    Flip the polarity of the signal. This effect is not audible on its own and takes no parameters. This effect is mathematically identical to ``def invert(x): return -x``.

    Inverting a signal may be useful to cancel out signals in many cases; for instance, ``Invert`` can be used with the ``Mix`` plugin to remove the original signal from an effects chain that contains multiple signals.
    """

    def __repr__(self) -> str: ...
    pass

class LadderFilter(Plugin):
    """
    A multi-mode audio filter based on the classic Moog synthesizer ladder filter, invented by Dr. Bob Moog in 1968.

    Depending on the filter's mode, frequencies above, below, or on both sides of the cutoff frequency will be attenuated. Higher values for the ``resonance`` parameter may cause peaks in the frequency response around the cutoff frequency.
    """

    class Mode(Enum):
        """
        The type of filter architecture to use.
        """

        LPF12 = 0  # fmt: skip
        """
        A low-pass filter with 12 dB of attenuation per octave above the cutoff frequency.
        """
        HPF12 = 1  # fmt: skip
        """
        A high-pass filter with 12 dB of attenuation per octave below the cutoff frequency.
        """
        BPF12 = 2  # fmt: skip
        """
        A band-pass filter with 12 dB of attenuation per octave on both sides of the cutoff frequency.
        """
        LPF24 = 3  # fmt: skip
        """
        A low-pass filter with 24 dB of attenuation per octave above the cutoff frequency.
        """
        HPF24 = 4  # fmt: skip
        """
        A high-pass filter with 24 dB of attenuation per octave below the cutoff frequency.
        """
        BPF24 = 5  # fmt: skip
        """
        A band-pass filter with 24 dB of attenuation per octave on both sides of the cutoff frequency.
        """

    def __init__(
        self,
        mode: LadderFilter.Mode = Mode.LPF12,
        cutoff_hz: float = 200,
        resonance: float = 0,
        drive: float = 1.0,
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def cutoff_hz(self) -> float:
        """ """

    @cutoff_hz.setter
    def cutoff_hz(self, arg1: float) -> None:
        pass

    @property
    def drive(self) -> float:
        """ """

    @drive.setter
    def drive(self, arg1: float) -> None:
        pass

    @property
    def mode(self) -> LadderFilter.Mode:
        """ """

    @mode.setter
    def mode(self, arg1: LadderFilter.Mode) -> None:
        pass

    @property
    def resonance(self) -> float:
        """ """

    @resonance.setter
    def resonance(self, arg1: float) -> None:
        pass
    BPF12: pedalboard_native.LadderFilter.Mode  # value = <Mode.BPF12: 2>
    BPF24: pedalboard_native.LadderFilter.Mode  # value = <Mode.BPF24: 5>
    HPF12: pedalboard_native.LadderFilter.Mode  # value = <Mode.HPF12: 1>
    HPF24: pedalboard_native.LadderFilter.Mode  # value = <Mode.HPF24: 4>
    LPF12: pedalboard_native.LadderFilter.Mode  # value = <Mode.LPF12: 0>
    LPF24: pedalboard_native.LadderFilter.Mode  # value = <Mode.LPF24: 3>
    pass

class Limiter(Plugin):
    """
    A simple limiter with standard threshold and release time controls, featuring two compressors and a hard clipper at 0 dB.
    """

    def __init__(self, threshold_db: float = -10.0, release_ms: float = 100.0) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def release_ms(self) -> float:
        """ """

    @release_ms.setter
    def release_ms(self, arg1: float) -> None:
        pass

    @property
    def threshold_db(self) -> float:
        """ """

    @threshold_db.setter
    def threshold_db(self, arg1: float) -> None:
        pass
    pass

class LowShelfFilter(IIRFilter, Plugin):
    """
    A low shelf filter with variable Q and gain, as would be used in an equalizer. Frequencies below the cutoff frequency will be boosted (or cut) by the provided gain value.
    """

    def __init__(
        self, cutoff_frequency_hz: float = 440, gain_db: float = 0.0, q: float = 0.7071067690849304
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def cutoff_frequency_hz(self) -> float:
        """ """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass

    @property
    def gain_db(self) -> float:
        """ """

    @gain_db.setter
    def gain_db(self, arg1: float) -> None:
        pass

    @property
    def q(self) -> float:
        """ """

    @q.setter
    def q(self, arg1: float) -> None:
        pass
    pass

class LowpassFilter(Plugin):
    """
    Apply a first-order low-pass filter with a roll-off of 6dB/octave. The cutoff frequency will be attenuated by -3dB (i.e.: 0.707x as loud).
    """

    def __init__(self, cutoff_frequency_hz: float = 50) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def cutoff_frequency_hz(self) -> float:
        """ """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass
    pass

class MP3Compressor(Plugin):
    """
    An MP3 compressor plugin that runs the LAME MP3 encoder in real-time to add compression artifacts to the audio stream.

    Currently only supports variable bit-rate mode (VBR) and accepts a floating-point VBR quality value (between 0.0 and 10.0; lower is better).

    Note that the MP3 format only supports 8kHz, 11025Hz, 12kHz, 16kHz, 22050Hz, 24kHz, 32kHz, 44.1kHz, and 48kHz audio; if an unsupported sample rate is provided, an exception will be thrown at processing time.
    """

    def __init__(self, vbr_quality: float = 2.0) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def vbr_quality(self) -> float:
        """ """

    @vbr_quality.setter
    def vbr_quality(self, arg1: float) -> None:
        pass
    pass

class NoiseGate(Plugin):
    """
    A simple noise gate with standard threshold, ratio, attack time and release time controls. Can be used as an expander if the ratio is low.
    """

    def __init__(
        self,
        threshold_db: float = -100.0,
        ratio: float = 10,
        attack_ms: float = 1.0,
        release_ms: float = 100.0,
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def attack_ms(self) -> float:
        """ """

    @attack_ms.setter
    def attack_ms(self, arg1: float) -> None:
        pass

    @property
    def ratio(self) -> float:
        """ """

    @ratio.setter
    def ratio(self, arg1: float) -> None:
        pass

    @property
    def release_ms(self) -> float:
        """ """

    @release_ms.setter
    def release_ms(self, arg1: float) -> None:
        pass

    @property
    def threshold_db(self) -> float:
        """ """

    @threshold_db.setter
    def threshold_db(self, arg1: float) -> None:
        pass
    pass

class PeakFilter(IIRFilter, Plugin):
    """
    A peak (or notch) filter with variable Q and gain, as would be used in an equalizer. Frequencies around the cutoff frequency will be boosted (or cut) by the provided gain value.
    """

    def __init__(
        self, cutoff_frequency_hz: float = 440, gain_db: float = 0.0, q: float = 0.7071067690849304
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def cutoff_frequency_hz(self) -> float:
        """ """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass

    @property
    def gain_db(self) -> float:
        """ """

    @gain_db.setter
    def gain_db(self, arg1: float) -> None:
        pass

    @property
    def q(self) -> float:
        """ """

    @q.setter
    def q(self, arg1: float) -> None:
        pass
    pass

class Phaser(Plugin):
    """
    A 6 stage phaser that modulates first order all-pass filters to create sweeping notches in the magnitude frequency response. This audio effect can be controlled with standard phaser parameters: the speed and depth of the LFO controlling the frequency response, a mix control, a feedback control, and the centre frequency of the modulation.
    """

    def __init__(
        self,
        rate_hz: float = 1.0,
        depth: float = 0.5,
        centre_frequency_hz: float = 1300.0,
        feedback: float = 0.0,
        mix: float = 0.5,
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def centre_frequency_hz(self) -> float:
        """ """

    @centre_frequency_hz.setter
    def centre_frequency_hz(self, arg1: float) -> None:
        pass

    @property
    def depth(self) -> float:
        """ """

    @depth.setter
    def depth(self, arg1: float) -> None:
        pass

    @property
    def feedback(self) -> float:
        """ """

    @feedback.setter
    def feedback(self, arg1: float) -> None:
        pass

    @property
    def mix(self) -> float:
        """ """

    @mix.setter
    def mix(self, arg1: float) -> None:
        pass

    @property
    def rate_hz(self) -> float:
        """ """

    @rate_hz.setter
    def rate_hz(self, arg1: float) -> None:
        pass
    pass

class PitchShift(Plugin):
    """
    A pitch shifting effect that can change the pitch of audio without affecting its duration.

    This effect uses `Chris Cannam's wonderful *Rubber Band* library <https://breakfastquay.com/rubberband/>`_ audio stretching library.
    """

    def __init__(self, semitones: float = 0.0) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def semitones(self) -> float:
        """ """

    @semitones.setter
    def semitones(self, arg1: float) -> None:
        pass
    pass

class AudioUnitPlugin(ExternalPlugin):
    """
    A wrapper around third-party, audio effect or instrument
    plugins in `Apple's Audio Unit <https://en.wikipedia.org/wiki/Audio_Units>`_
    format.

    Audio Unit plugins are only supported on macOS. This class will be
    unavailable on non-macOS platforms. Plugin files must be installed
    in the appropriate system-wide path for them to be
    loadable (usually ``/Library/Audio/Plug-Ins/Components/`` or
    ``~/Library/Audio/Plug-Ins/Components/``).

    For a plugin wrapper that works on Windows and Linux as well,
    see :class:`pedalboard.VST3Plugin`.)

    .. warning::
        Some Audio Unit plugins may throw errors, hang, generate incorrect output, or
        outright crash if called from background threads. If you find that a Audio Unit
        plugin is not working as expected, try calling it from the main thread
        instead and `open a GitHub Issue to track the incompatibility
        <https://github.com/spotify/pedalboard/issues/new>`_.

    *Support for instrument plugins introduced in v0.7.4.*

    *Support for running Audio Unit plugins on background threads introduced in v0.8.8.*

    *Support for loading AUv3 plugins (* ``.appex`` *bundles) introduced in v0.9.5.*
    """

    @typing.overload
    def __call__(
        self,
        input_array: numpy.ndarray,
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Run an audio or MIDI buffer through this plugin, returning audio. Alias for :py:meth:`process`.

        Run an audio or MIDI buffer through this plugin, returning audio. Alias for :py:meth:`process`.
        """

    @typing.overload
    def __call__(
        self,
        midi_messages: object,
        duration: float,
        sample_rate: float,
        num_channels: int = 2,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]: ...
    def __init__(
        self,
        path_to_plugin_file: str,
        parameter_values: object = None,
        plugin_name: typing.Optional[str] = None,
        initialization_timeout: float = 10.0,
    ) -> None: ...
    def __repr__(self) -> str: ...
    def _get_parameter(self, arg0: str) -> _AudioProcessorParameter: ...
    @staticmethod
    def get_plugin_names_for_file(filename: str) -> typing.List[str]:
        """
        Return a list of plugin names contained within a given Audio Unit bundle (i.e.: a ``.component`` or ``.appex`` file). If the provided file cannot be scanned, an ``ImportError`` will be raised.

        Note that most Audio Units have a single plugin inside, but this method can be useful to determine if multiple plugins are present in one bundle, and if so, what their names are.
        """

    @typing.overload
    def process(
        self,
        input_array: numpy.ndarray,
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Pass a buffer of audio (as a 32- or 64-bit NumPy array) *or* a list of
        MIDI messages to this plugin, returning audio.

        (If calling this multiple times with multiple effect plugins, consider
        creating a :class:`pedalboard.Pedalboard` object instead.)

        When provided audio as input, the returned array may contain up to (but not
        more than) the same number of samples as were provided. If fewer samples
        were returned than expected, the plugin has likely buffered audio inside
        itself. To receive the remaining audio, pass another audio buffer into
        ``process`` with ``reset`` set to ``True``.

        If the provided buffer uses a 64-bit datatype, it will be converted to 32-bit
        for processing.

        If provided MIDI messages as input, the provided ``midi_messages`` must be
        a Python ``List`` containing one of the following types:

         - Objects with a ``bytes()`` method and ``time`` property (such as :doc:`mido:messages`
           from :doc:`mido:index`, not included with Pedalboard)
         - Tuples that look like: ``(midi_bytes: bytes, timestamp_in_seconds: float)``
         - Tuples that look like: ``(midi_bytes: List[int], timestamp_in_seconds: float)``

        The returned array will contain ``duration`` seconds worth of audio at the
        provided ``sample_rate``.

        Each MIDI message will be sent to the plugin at its
        timestamp, where a timestamp of ``0`` indicates the start of the buffer, and
        a timestamp equal to ``duration`` indicates the end of the buffer. (Any MIDI
        messages whose timestamps are greater than ``duration`` will be ignored.)

        The provided ``buffer_size`` argument will be used to control the size of
        each chunk of audio returned by the plugin at once. Higher buffer sizes may
        speed up processing, but may cause increased memory usage.

        The ``reset`` flag determines if this plugin should be reset before
        processing begins, clearing any state from previous calls to ``process``.
        If calling ``process`` multiple times while processing the same audio or
        MIDI stream, set ``reset`` to ``False``.

        .. note::
            The :py:meth:`process` method can also be used via :py:meth:`__call__`;
            i.e.: just calling this object like a function (``my_plugin(...)``) will
            automatically invoke :py:meth:`process` with the same arguments.


        Examples
        --------

        Running audio through an external effect plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_effect
           with AudioFile("input-audio.wav") as f:
               output_audio = plugin(f.read(), f.samplerate)


        Rendering MIDI via an external instrument plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile
           from mido import Message # not part of Pedalboard, but convenient!

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_instrument

           sample_rate = 44100
           num_channels = 2
           with AudioFile("output-audio.wav", "w", sample_rate, num_channels) as f:
               f.write(plugin(
                   [Message("note_on", note=60), Message("note_off", note=60, time=4)],
                   sample_rate=sample_rate,
                   duration=5,
                   num_channels=num_channels
               ))


        *Support for instrument plugins introduced in v0.7.4.*



        Pass a buffer of audio (as a 32- or 64-bit NumPy array) *or* a list of
        MIDI messages to this plugin, returning audio.

        (If calling this multiple times with multiple effect plugins, consider
        creating a :class:`pedalboard.Pedalboard` object instead.)

        When provided audio as input, the returned array may contain up to (but not
        more than) the same number of samples as were provided. If fewer samples
        were returned than expected, the plugin has likely buffered audio inside
        itself. To receive the remaining audio, pass another audio buffer into
        ``process`` with ``reset`` set to ``True``.

        If the provided buffer uses a 64-bit datatype, it will be converted to 32-bit
        for processing.

        If provided MIDI messages as input, the provided ``midi_messages`` must be
        a Python ``List`` containing one of the following types:

         - Objects with a ``bytes()`` method and ``time`` property (such as :doc:`mido:messages`
           from :doc:`mido:index`, not included with Pedalboard)
         - Tuples that look like: ``(midi_bytes: bytes, timestamp_in_seconds: float)``
         - Tuples that look like: ``(midi_bytes: List[int], timestamp_in_seconds: float)``

        The returned array will contain ``duration`` seconds worth of audio at the
        provided ``sample_rate``.

        Each MIDI message will be sent to the plugin at its
        timestamp, where a timestamp of ``0`` indicates the start of the buffer, and
        a timestamp equal to ``duration`` indicates the end of the buffer. (Any MIDI
        messages whose timestamps are greater than ``duration`` will be ignored.)

        The provided ``buffer_size`` argument will be used to control the size of
        each chunk of audio returned by the plugin at once. Higher buffer sizes may
        speed up processing, but may cause increased memory usage.

        The ``reset`` flag determines if this plugin should be reset before
        processing begins, clearing any state from previous calls to ``process``.
        If calling ``process`` multiple times while processing the same audio or
        MIDI stream, set ``reset`` to ``False``.

        .. note::
            The :py:meth:`process` method can also be used via :py:meth:`__call__`;
            i.e.: just calling this object like a function (``my_plugin(...)``) will
            automatically invoke :py:meth:`process` with the same arguments.


        Examples
        --------

        Running audio through an external effect plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_effect
           with AudioFile("input-audio.wav") as f:
               output_audio = plugin(f.read(), f.samplerate)


        Rendering MIDI via an external instrument plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile
           from mido import Message # not part of Pedalboard, but convenient!

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_instrument

           sample_rate = 44100
           num_channels = 2
           with AudioFile("output-audio.wav", "w", sample_rate, num_channels) as f:
               f.write(plugin(
                   [Message("note_on", note=60), Message("note_off", note=60, time=4)],
                   sample_rate=sample_rate,
                   duration=5,
                   num_channels=num_channels
               ))


        *Support for instrument plugins introduced in v0.7.4.*

        """

    @typing.overload
    def process(
        self,
        midi_messages: object,
        duration: float,
        sample_rate: float,
        num_channels: int = 2,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]: ...
    def show_editor(self, close_event: typing.Optional[threading.Event] = None) -> None:
        """
        Show the UI of this plugin as a native window.

        This method may only be called on the main thread, and will block
        the main thread until any of the following things happens:

         - the window is closed by clicking the close button
         - the window is closed by pressing the appropriate (OS-specific) keyboard shortcut
         - a KeyboardInterrupt (Ctrl-C) is sent to the program
         - the :py:meth:`threading.Event.set` method is called (by another thread)
           on a provided :py:class:`threading.Event` object

        An example of how to programmatically close an editor window::

           import pedalboard
           from threading import Event, Thread

           plugin = pedalboard.load_plugin("../path-to-my-plugin-file")
           close_window_event = Event()

           def other_thread():
               # do something to determine when to close the window
               if should_close_window:
                   close_window_event.set()

           thread = Thread(target=other_thread)
           thread.start()

           # This will block until the other thread calls .set():
           plugin.show_editor(close_window_event)
        """

    @property
    def _parameters(self) -> typing.List[_AudioProcessorParameter]:
        """ """

    @property
    def _reload_type(self) -> ExternalPluginReloadType:
        """
        The behavior that this plugin exhibits when .reset() is called. This is an internal attribute which gets set on plugin instantiation and should only be accessed for debugging and testing.


        """

    @_reload_type.setter
    def _reload_type(self, arg0: ExternalPluginReloadType) -> None:
        """
        The behavior that this plugin exhibits when .reset() is called. This is an internal attribute which gets set on plugin instantiation and should only be accessed for debugging and testing.
        """

    @property
    def category(self) -> str:
        """
        A category that this plugin falls into, such as "Dynamics", "Reverbs", etc.

        *Introduced in v0.9.4.*


        """

    @property
    def descriptive_name(self) -> str:
        """
        A more descriptive name for this plugin. This may be the same as the 'name' field, but some plugins may provide an alternative name.

        *Introduced in v0.9.4.*


        """

    @property
    def has_shared_container(self) -> bool:
        """
        True iff this plugin is part of a multi-plugin container.

        *Introduced in v0.9.4.*


        """

    @property
    def identifier(self) -> str:
        """
        A string that can be saved and used to uniquely identify this plugin (and version) again.

        *Introduced in v0.9.4.*


        """

    @property
    def is_instrument(self) -> bool:
        """
        True iff this plugin identifies itself as an instrument (generator, synthesizer, etc) plugin.

        *Introduced in v0.9.4.*


        """

    @property
    def manufacturer_name(self) -> str:
        """
        The name of the manufacturer of this plugin, as reported by the plugin itself.

        *Introduced in v0.9.4.*


        """

    @property
    def name(self) -> str:
        """
        The name of this plugin.


        """

    @property
    def raw_state(self) -> bytes:
        """
        A :py:class:`bytes` object representing the plugin's internal state.

        For the Audio Unit format, this is usually a binary property list that can be decoded or encoded with the built-in :py:mod:`plistlib` package.

        .. warning::
            This property can be set to change the plugin's internal state, but providing invalid data may cause the plugin to crash, taking the entire Python process down with it.


        """

    @raw_state.setter
    def raw_state(self, arg1: bytes) -> None:
        """
        A :py:class:`bytes` object representing the plugin's internal state.

        For the Audio Unit format, this is usually a binary property list that can be decoded or encoded with the built-in :py:mod:`plistlib` package.

        .. warning::
            This property can be set to change the plugin's internal state, but providing invalid data may cause the plugin to crash, taking the entire Python process down with it.
        """

    @property
    def reported_latency_samples(self) -> int:
        """
        The number of samples of latency (delay) that this plugin reports to introduce into the audio signal due to internal buffering and processing. Pedalboard automatically compensates for this latency during processing, so this property is present for informational purposes. Note that not all plugins correctly report the latency that they introduce, so this value may be inaccurate (especially if the plugin reports 0).

        *Introduced in v0.9.12.*


        """

    @property
    def version(self) -> str:
        """
        The version string for this plugin, as reported by the plugin itself.

        *Introduced in v0.9.4.*


        """
    pass

class PluginContainer(Plugin):
    """
    A generic audio processing plugin that contains zero or more other plugins. Not intended for direct use.
    """

    def __contains__(self, plugin: Plugin) -> bool: ...
    def __delitem__(self, index: int) -> None:
        """
        Delete a plugin by its index. Index may be negative. If the index is out of range, an IndexError will be thrown.
        """

    def __getitem__(self, index: int) -> Plugin:
        """
        Get a plugin by its index. Index may be negative. If the index is out of range, an IndexError will be thrown.
        """

    def __init__(self, plugins: list[Plugin]) -> None: ...
    def __iter__(self) -> typing.Iterator[Plugin]: ...
    def __len__(self) -> int:
        """
        Get the number of plugins in this container.
        """

    def __setitem__(self, index: int, plugin: Plugin) -> None:
        """
        Replace a plugin at the specified index. Index may be negative. If the index is out of range, an IndexError will be thrown.
        """

    def append(self, plugin: Plugin) -> None:
        """
        Append a plugin to the end of this container.
        """

    def insert(self, index: int, plugin: Plugin) -> None:
        """
        Insert a plugin at the specified index.
        """

    def remove(self, plugin: Plugin) -> None:
        """
        Remove a plugin by its value.
        """
    pass

class Resample(Plugin):
    """
    A plugin that downsamples the input audio to the given sample rate, then upsamples it back to the original sample rate. Various quality settings will produce audible distortion and aliasing effects.
    """

    class Quality(Enum):
        """
        Indicates a specific resampling algorithm to use.
        """

        ZeroOrderHold = 0  # fmt: skip
        """
        The lowest quality and fastest resampling method, with lots of audible artifacts.
        """
        Linear = 1  # fmt: skip
        """
        A resampling method slightly less noisy than the simplest method, but not by much.
        """
        CatmullRom = 2  # fmt: skip
        """
        A moderately good-sounding resampling method which is fast to run.
        """
        Lagrange = 3  # fmt: skip
        """
        A moderately good-sounding resampling method which is slow to run.
        """
        WindowedSinc = 4  # fmt: skip
        """
        The highest quality and slowest resampling method, with no audible artifacts.
        """

    def __init__(
        self, target_sample_rate: float = 8000.0, quality: Quality = Quality.WindowedSinc
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def quality(self) -> Resample.Quality:
        """
        The resampling algorithm used to resample the audio.


        """

    @quality.setter
    def quality(self, arg1: Resample.Quality) -> None:
        """
        The resampling algorithm used to resample the audio.
        """

    @property
    def target_sample_rate(self) -> float:
        """
        The sample rate to resample the input audio to. This value may be a floating-point number, in which case a floating-point sampling rate will be used. Note that the output of this plugin will still be at the original sample rate; this is merely the sample rate used for quality reduction.


        """

    @target_sample_rate.setter
    def target_sample_rate(self, arg1: float) -> None:
        """
        The sample rate to resample the input audio to. This value may be a floating-point number, in which case a floating-point sampling rate will be used. Note that the output of this plugin will still be at the original sample rate; this is merely the sample rate used for quality reduction.
        """
    CatmullRom: pedalboard_native.Resample.Quality  # value = <Quality.CatmullRom: 2>
    Lagrange: pedalboard_native.Resample.Quality  # value = <Quality.Lagrange: 3>
    Linear: pedalboard_native.Resample.Quality  # value = <Quality.Linear: 1>
    WindowedSinc: pedalboard_native.Resample.Quality  # value = <Quality.WindowedSinc: 4>
    ZeroOrderHold: pedalboard_native.Resample.Quality  # value = <Quality.ZeroOrderHold: 0>
    pass

class Reverb(Plugin):
    """
    A simple reverb effect. Uses a simple stereo reverb algorithm, based on the technique and tunings used in `FreeVerb <https://ccrma.stanford.edu/~jos/pasp/Freeverb.html>_`.
    """

    def __init__(
        self,
        room_size: float = 0.5,
        damping: float = 0.5,
        wet_level: float = 0.33,
        dry_level: float = 0.4,
        width: float = 1.0,
        freeze_mode: float = 0.0,
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def damping(self) -> float:
        """ """

    @damping.setter
    def damping(self, arg1: float) -> None:
        pass

    @property
    def dry_level(self) -> float:
        """ """

    @dry_level.setter
    def dry_level(self, arg1: float) -> None:
        pass

    @property
    def freeze_mode(self) -> float:
        """ """

    @freeze_mode.setter
    def freeze_mode(self, arg1: float) -> None:
        pass

    @property
    def room_size(self) -> float:
        """ """

    @room_size.setter
    def room_size(self, arg1: float) -> None:
        pass

    @property
    def wet_level(self) -> float:
        """ """

    @wet_level.setter
    def wet_level(self, arg1: float) -> None:
        pass

    @property
    def width(self) -> float:
        """ """

    @width.setter
    def width(self, arg1: float) -> None:
        pass
    pass

class VST3Plugin(ExternalPlugin):
    """
    A wrapper around third-party, audio effect or instrument plugins in
    `Steinberg GmbH's VST3® <https://en.wikipedia.org/wiki/Virtual_Studio_Technology>`_
    format.

    VST3® plugins are supported on macOS, Windows, and Linux. However, VST3® plugin
    files are not cross-compatible with different operating systems; a platform-specific
    build of each plugin is required to load that plugin on a given platform. (For
    example: a Windows VST3 plugin bundle will not load on Linux or macOS.)

    .. warning::
        Some VST3® plugins may throw errors, hang, generate incorrect output, or
        outright crash if called from background threads. If you find that a VST3®
        plugin is not working as expected, try calling it from the main thread
        instead and `open a GitHub Issue to track the incompatibility
        <https://github.com/spotify/pedalboard/issues/new>`_.


    *Support for instrument plugins introduced in v0.7.4.*

    *Support for running VST3® plugins on background threads introduced in v0.8.8.*
    """

    @typing.overload
    def __call__(
        self,
        input_array: numpy.ndarray,
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Run an audio or MIDI buffer through this plugin, returning audio. Alias for :py:meth:`process`.

        Run an audio or MIDI buffer through this plugin, returning audio. Alias for :py:meth:`process`.
        """

    @typing.overload
    def __call__(
        self,
        midi_messages: object,
        duration: float,
        sample_rate: float,
        num_channels: int = 2,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]: ...
    def __init__(
        self,
        path_to_plugin_file: str,
        parameter_values: object = None,
        plugin_name: typing.Optional[str] = None,
        initialization_timeout: float = 10.0,
    ) -> None: ...
    def __repr__(self) -> str: ...
    def _get_parameter(self, arg0: str) -> _AudioProcessorParameter: ...
    @staticmethod
    def get_plugin_names_for_file(arg0: str) -> typing.List[str]:
        """
        Return a list of plugin names contained within a given VST3 plugin (i.e.: a ".vst3"). If the provided file cannot be scanned, an ImportError will be raised.
        """

    def load_preset(self, preset_file_path: str) -> None:
        """
        Load a VST3 preset file in .vstpreset format.
        """

    @typing.overload
    def process(
        self,
        input_array: numpy.ndarray,
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Pass a buffer of audio (as a 32- or 64-bit NumPy array) *or* a list of
        MIDI messages to this plugin, returning audio.

        (If calling this multiple times with multiple effect plugins, consider
        creating a :class:`pedalboard.Pedalboard` object instead.)

        When provided audio as input, the returned array may contain up to (but not
        more than) the same number of samples as were provided. If fewer samples
        were returned than expected, the plugin has likely buffered audio inside
        itself. To receive the remaining audio, pass another audio buffer into
        ``process`` with ``reset`` set to ``True``.

        If the provided buffer uses a 64-bit datatype, it will be converted to 32-bit
        for processing.

        If provided MIDI messages as input, the provided ``midi_messages`` must be
        a Python ``List`` containing one of the following types:

         - Objects with a ``bytes()`` method and ``time`` property (such as :doc:`mido:messages`
           from :doc:`mido:index`, not included with Pedalboard)
         - Tuples that look like: ``(midi_bytes: bytes, timestamp_in_seconds: float)``
         - Tuples that look like: ``(midi_bytes: List[int], timestamp_in_seconds: float)``

        The returned array will contain ``duration`` seconds worth of audio at the
        provided ``sample_rate``.

        Each MIDI message will be sent to the plugin at its
        timestamp, where a timestamp of ``0`` indicates the start of the buffer, and
        a timestamp equal to ``duration`` indicates the end of the buffer. (Any MIDI
        messages whose timestamps are greater than ``duration`` will be ignored.)

        The provided ``buffer_size`` argument will be used to control the size of
        each chunk of audio returned by the plugin at once. Higher buffer sizes may
        speed up processing, but may cause increased memory usage.

        The ``reset`` flag determines if this plugin should be reset before
        processing begins, clearing any state from previous calls to ``process``.
        If calling ``process`` multiple times while processing the same audio or
        MIDI stream, set ``reset`` to ``False``.

        .. note::
            The :py:meth:`process` method can also be used via :py:meth:`__call__`;
            i.e.: just calling this object like a function (``my_plugin(...)``) will
            automatically invoke :py:meth:`process` with the same arguments.


        Examples
        --------

        Running audio through an external effect plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_effect
           with AudioFile("input-audio.wav") as f:
               output_audio = plugin(f.read(), f.samplerate)


        Rendering MIDI via an external instrument plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile
           from mido import Message # not part of Pedalboard, but convenient!

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_instrument

           sample_rate = 44100
           num_channels = 2
           with AudioFile("output-audio.wav", "w", sample_rate, num_channels) as f:
               f.write(plugin(
                   [Message("note_on", note=60), Message("note_off", note=60, time=4)],
                   sample_rate=sample_rate,
                   duration=5,
                   num_channels=num_channels
               ))


        *Support for instrument plugins introduced in v0.7.4.*



        Pass a buffer of audio (as a 32- or 64-bit NumPy array) *or* a list of
        MIDI messages to this plugin, returning audio.

        (If calling this multiple times with multiple effect plugins, consider
        creating a :class:`pedalboard.Pedalboard` object instead.)

        When provided audio as input, the returned array may contain up to (but not
        more than) the same number of samples as were provided. If fewer samples
        were returned than expected, the plugin has likely buffered audio inside
        itself. To receive the remaining audio, pass another audio buffer into
        ``process`` with ``reset`` set to ``True``.

        If the provided buffer uses a 64-bit datatype, it will be converted to 32-bit
        for processing.

        If provided MIDI messages as input, the provided ``midi_messages`` must be
        a Python ``List`` containing one of the following types:

         - Objects with a ``bytes()`` method and ``time`` property (such as :doc:`mido:messages`
           from :doc:`mido:index`, not included with Pedalboard)
         - Tuples that look like: ``(midi_bytes: bytes, timestamp_in_seconds: float)``
         - Tuples that look like: ``(midi_bytes: List[int], timestamp_in_seconds: float)``

        The returned array will contain ``duration`` seconds worth of audio at the
        provided ``sample_rate``.

        Each MIDI message will be sent to the plugin at its
        timestamp, where a timestamp of ``0`` indicates the start of the buffer, and
        a timestamp equal to ``duration`` indicates the end of the buffer. (Any MIDI
        messages whose timestamps are greater than ``duration`` will be ignored.)

        The provided ``buffer_size`` argument will be used to control the size of
        each chunk of audio returned by the plugin at once. Higher buffer sizes may
        speed up processing, but may cause increased memory usage.

        The ``reset`` flag determines if this plugin should be reset before
        processing begins, clearing any state from previous calls to ``process``.
        If calling ``process`` multiple times while processing the same audio or
        MIDI stream, set ``reset`` to ``False``.

        .. note::
            The :py:meth:`process` method can also be used via :py:meth:`__call__`;
            i.e.: just calling this object like a function (``my_plugin(...)``) will
            automatically invoke :py:meth:`process` with the same arguments.


        Examples
        --------

        Running audio through an external effect plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_effect
           with AudioFile("input-audio.wav") as f:
               output_audio = plugin(f.read(), f.samplerate)


        Rendering MIDI via an external instrument plugin::

           from pedalboard import load_plugin
           from pedalboard.io import AudioFile
           from mido import Message # not part of Pedalboard, but convenient!

           plugin = load_plugin("../path-to-my-plugin-file")
           assert plugin.is_instrument

           sample_rate = 44100
           num_channels = 2
           with AudioFile("output-audio.wav", "w", sample_rate, num_channels) as f:
               f.write(plugin(
                   [Message("note_on", note=60), Message("note_off", note=60, time=4)],
                   sample_rate=sample_rate,
                   duration=5,
                   num_channels=num_channels
               ))


        *Support for instrument plugins introduced in v0.7.4.*

        """

    @typing.overload
    def process(
        self,
        midi_messages: object,
        duration: float,
        sample_rate: float,
        num_channels: int = 2,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]: ...
    def show_editor(self, close_event: typing.Optional[threading.Event] = None) -> None:
        """
        Show the UI of this plugin as a native window.

        This method may only be called on the main thread, and will block
        the main thread until any of the following things happens:

         - the window is closed by clicking the close button
         - the window is closed by pressing the appropriate (OS-specific) keyboard shortcut
         - a KeyboardInterrupt (Ctrl-C) is sent to the program
         - the :py:meth:`threading.Event.set` method is called (by another thread)
           on a provided :py:class:`threading.Event` object

        An example of how to programmatically close an editor window::

           import pedalboard
           from threading import Event, Thread

           plugin = pedalboard.load_plugin("../path-to-my-plugin-file")
           close_window_event = Event()

           def other_thread():
               # do something to determine when to close the window
               if should_close_window:
                   close_window_event.set()

           thread = Thread(target=other_thread)
           thread.start()

           # This will block until the other thread calls .set():
           plugin.show_editor(close_window_event)
        """

    @property
    def _parameters(self) -> typing.List[_AudioProcessorParameter]:
        """ """

    @property
    def _reload_type(self) -> ExternalPluginReloadType:
        """
        The behavior that this plugin exhibits when .reset() is called. This is an internal attribute which gets set on plugin instantiation and should only be accessed for debugging and testing.


        """

    @_reload_type.setter
    def _reload_type(self, arg0: ExternalPluginReloadType) -> None:
        """
        The behavior that this plugin exhibits when .reset() is called. This is an internal attribute which gets set on plugin instantiation and should only be accessed for debugging and testing.
        """

    @property
    def category(self) -> str:
        """
        A category that this plugin falls into, such as "Dynamics", "Reverbs", etc.

        *Introduced in v0.9.4.*


        """

    @property
    def descriptive_name(self) -> str:
        """
        A more descriptive name for this plugin. This may be the same as the 'name' field, but some plugins may provide an alternative name.

        *Introduced in v0.9.4.*


        """

    @property
    def has_shared_container(self) -> bool:
        """
        True iff this plugin is part of a multi-plugin container.

        *Introduced in v0.9.4.*


        """

    @property
    def identifier(self) -> str:
        """
        A string that can be saved and used to uniquely identify this plugin (and version) again.

        *Introduced in v0.9.4.*


        """

    @property
    def is_instrument(self) -> bool:
        """
        True iff this plugin identifies itself as an instrument (generator, synthesizer, etc) plugin.

        *Introduced in v0.9.4.*


        """

    @property
    def manufacturer_name(self) -> str:
        """
        The name of the manufacturer of this plugin, as reported by the plugin itself.

        *Introduced in v0.9.4.*


        """

    @property
    def name(self) -> str:
        """
        The name of this plugin.


        """

    @property
    def preset_data(self) -> bytes:
        """
        Get or set the current plugin state as bytes in .vstpreset format.

        .. warning::
            This property can be set to change the plugin's internal state, but providing invalid data may cause the plugin to crash, taking the entire Python process down with it.


        """

    @preset_data.setter
    def preset_data(self, arg1: bytes) -> None:
        """
        Get or set the current plugin state as bytes in .vstpreset format.

        .. warning::
            This property can be set to change the plugin's internal state, but providing invalid data may cause the plugin to crash, taking the entire Python process down with it.
        """

    @property
    def raw_state(self) -> bytes:
        """
        A :py:class:`bytes` object representing the plugin's internal state.

        For the VST3 format, this is usually an XML-encoded string prefixed with an 8-byte header and suffixed with a single null byte.

        .. warning::
            This property can be set to change the plugin's internal state, but providing invalid data may cause the plugin to crash, taking the entire Python process down with it.


        """

    @raw_state.setter
    def raw_state(self, arg1: bytes) -> None:
        """
        A :py:class:`bytes` object representing the plugin's internal state.

        For the VST3 format, this is usually an XML-encoded string prefixed with an 8-byte header and suffixed with a single null byte.

        .. warning::
            This property can be set to change the plugin's internal state, but providing invalid data may cause the plugin to crash, taking the entire Python process down with it.
        """

    @property
    def reported_latency_samples(self) -> int:
        """
        The number of samples of latency (delay) that this plugin reports to introduce into the audio signal due to internal buffering and processing. Pedalboard automatically compensates for this latency during processing, so this property is present for informational purposes. Note that not all plugins correctly report the latency that they introduce, so this value may be inaccurate (especially if the plugin reports 0).

        *Introduced in v0.9.12.*


        """

    @property
    def version(self) -> str:
        """
        The version string for this plugin, as reported by the plugin itself.

        *Introduced in v0.9.4.*


        """
    pass

class _AudioProcessorParameter:
    """
    An abstract base class for parameter objects that can be added to an AudioProcessor.
    """

    def __repr__(self) -> str: ...
    def get_name(self, maximum_string_length: int) -> str:
        """
        Returns the name to display for this parameter, which is made to fit within the given string length
        """

    def get_raw_value_for_text(self, string_value: str) -> float:
        """
        Returns the raw value of the supplied text. Plugins may handle errors however they see fit, but will likely not raise exceptions.
        """

    def get_text_for_raw_value(self, raw_value: float, maximum_string_length: int = 512) -> str:
        """
        Returns a textual version of the supplied normalised parameter value.
        """

    @property
    def default_raw_value(self) -> float:
        """
        The default internal value of this parameter. Convention is that this parameter should be between 0 and 1.0. This may or may not correspond with the value shown to the user.


        """

    @property
    def index(self) -> int:
        """
        The index of this parameter in its plugin's parameter list.


        """

    @property
    def is_automatable(self) -> bool:
        """
        Returns true if this parameter can be automated (i.e.: scheduled to change over time, in real-time, in a DAW).


        """

    @property
    def is_boolean(self) -> bool:
        """
        Returns whether the parameter represents a boolean switch, typically with "On" and "Off" states.


        """

    @property
    def is_discrete(self) -> bool:
        """
        Returns whether the parameter uses discrete values, based on the result of getNumSteps, or allows the host to select values continuously.


        """

    @property
    def is_meta_parameter(self) -> bool:
        """
        A meta-parameter is a parameter that changes other parameters.


        """

    @property
    def is_orientation_inverted(self) -> bool:
        """
        If true, this parameter operates in the reverse direction. (Not all plugin formats will actually use this information).


        """

    @property
    def label(self) -> str:
        """
        Some parameters may be able to return a label string for their units. For example "Hz" or "%".


        """

    @property
    def name(self) -> str:
        """
        Returns the name to display for this parameter at its longest.


        """

    @property
    def num_steps(self) -> int:
        """
        Returns the number of steps that this parameter's range should be quantised into. See also: is_discrete, is_boolean.


        """

    @property
    def raw_value(self) -> float:
        """
        The internal value of this parameter. Convention is that this parameter should be between 0 and 1.0. This may or may not correspond with the value shown to the user.


        """

    @raw_value.setter
    def raw_value(self, arg1: float) -> None:
        """
        The internal value of this parameter. Convention is that this parameter should be between 0 and 1.0. This may or may not correspond with the value shown to the user.
        """

    @property
    def string_value(self) -> str:
        """
        Returns the current value of the parameter as a string.


        """
    pass

def process(
    input_array: numpy.ndarray,
    sample_rate: float,
    plugins: typing.List[Plugin],
    buffer_size: int = 8192,
    reset: bool = True,
) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
    """
    Run a 32-bit or 64-bit floating point audio buffer through a
    list of Pedalboard plugins. If the provided buffer uses a 64-bit datatype,
    it will be converted to 32-bit for processing.

    The provided ``buffer_size`` argument will be used to control the size of
    each chunk of audio provided into the plugins. Higher buffer sizes may speed up
    processing at the expense of memory usage.

    The ``reset`` flag determines if all of the plugins should be reset before
    processing begins, clearing any state from previous calls to ``process``.
    If calling ``process`` multiple times while processing the same audio file
    or buffer, set ``reset`` to ``False``.

    :meta private:
    """

class GSMFullRateCompressor(Plugin):
    """
    An audio degradation/compression plugin that applies the GSM "Full Rate" compression algorithm to emulate the sound of a 2G cellular phone connection. This plugin internally resamples the input audio to a fixed sample rate of 8kHz (required by the GSM Full Rate codec), although the quality of the resampling algorithm can be specified.
    """

    def __init__(self, quality: Resample.Quality = Resample.Quality.WindowedSinc) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def quality(self) -> Resample.Quality:
        """ """

    @quality.setter
    def quality(self, arg1: Resample.Quality) -> None:
        pass
    pass
