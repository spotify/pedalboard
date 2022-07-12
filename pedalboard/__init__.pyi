"""
      Pedalboard
      ----------

      .. currentmodule:: pedalboard

      .. autosummary::
         :toctree: _generate
  """
from __future__ import annotations
import pedalboard_native
import typing
import numpy

_Shape = typing.Tuple[int, ...]

__all__ = [
    "Bitcrush",
    "Chorus",
    "Compressor",
    "Convolution",
    "Delay",
    "Distortion",
    "GSMFullRateCompressor",
    "Gain",
    "HighShelfFilter",
    "HighpassFilter",
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
    "io",
    "process",
    "utils",
]


class Plugin:
    """
    A generic audio processing plugin. Base class of all Pedalboard plugins.
    """

    def __init__(self) -> None:
        ...

    @typing.overload
    def process(
        self,
        input_array: numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]],
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Run a 32-bit floating point audio buffer through this plugin.(Note: if calling this multiple times with multiple plugins, consider using pedalboard.process(...) instead.)

        Run a 64-bit floating point audio buffer through this plugin.(Note: if calling this multiple times with multiple plugins, consider using pedalboard.process(...) instead.) The buffer will be converted to 32-bit for processing.
        """

    @typing.overload
    def process(
        self,
        input_array: numpy.ndarray[typing.Any, numpy.dtype[numpy.float64]],
        sample_rate: float,
        buffer_size: int = 8192,
        reset: bool = True,
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        ...

    def reset(self) -> None:
        """
        Clear any internal state kept by this plugin (e.g.: reverb tails). The values of plugin parameters will remain unchanged. For most plugins, this is a fast operation; for some, this will cause a full re-instantiation of the plugin.
        """

    pass


class Chorus(Plugin):
    """
    A basic chorus effect. This audio effect can be controlled via the speed and depth of the LFO controlling the frequency response, a mix control, a feedback control, and the centre delay of the modulation.
    Note: To get classic chorus sounds try to use a centre delay time around 7-8 ms with a low feeback volume and a low depth. This effect can also be used as a flanger with a lower centre delay time and a lot of feedback, and as a vibrato effect if the mix value is 1.
    """

    def __init__(
        self,
        rate_hz: float = 1.0,
        depth: float = 0.25,
        centre_delay_ms: float = 7.0,
        feedback: float = 0.0,
        mix: float = 0.5,
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def centre_delay_ms(self) -> float:
        """
        :type: float
        """

    @centre_delay_ms.setter
    def centre_delay_ms(self, arg1: float) -> None:
        pass

    @property
    def depth(self) -> float:
        """
        :type: float
        """

    @depth.setter
    def depth(self, arg1: float) -> None:
        pass

    @property
    def feedback(self) -> float:
        """
        :type: float
        """

    @feedback.setter
    def feedback(self, arg1: float) -> None:
        pass

    @property
    def mix(self) -> float:
        """
        :type: float
        """

    @mix.setter
    def mix(self, arg1: float) -> None:
        pass

    @property
    def rate_hz(self) -> float:
        """
        :type: float
        """

    @rate_hz.setter
    def rate_hz(self, arg1: float) -> None:
        pass

    pass


class Compressor(Plugin):
    """
    A dynamic range compressor, used to amplify quiet sounds and reduce the volume of loud sounds.
    """

    def __init__(
        self,
        threshold_db: float = 0,
        ratio: float = 1,
        attack_ms: float = 1.0,
        release_ms: float = 100,
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def attack_ms(self) -> float:
        """
        :type: float
        """

    @attack_ms.setter
    def attack_ms(self, arg1: float) -> None:
        pass

    @property
    def ratio(self) -> float:
        """
        :type: float
        """

    @ratio.setter
    def ratio(self, arg1: float) -> None:
        pass

    @property
    def release_ms(self) -> float:
        """
        :type: float
        """

    @release_ms.setter
    def release_ms(self, arg1: float) -> None:
        pass

    @property
    def threshold_db(self) -> float:
        """
        :type: float
        """

    @threshold_db.setter
    def threshold_db(self, arg1: float) -> None:
        pass

    pass


class Convolution(Plugin):
    """
    An audio convolution, suitable for things like speaker simulation or reverb modeling.
    """

    def __init__(self, impulse_response_filename: str, mix: float = 1.0) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def impulse_response_filename(self) -> str:
        """
        :type: str
        """

    @property
    def mix(self) -> float:
        """
        :type: float
        """

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
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def delay_seconds(self) -> float:
        """
        :type: float
        """

    @delay_seconds.setter
    def delay_seconds(self, arg1: float) -> None:
        pass

    @property
    def feedback(self) -> float:
        """
        :type: float
        """

    @feedback.setter
    def feedback(self, arg1: float) -> None:
        pass

    @property
    def mix(self) -> float:
        """
        :type: float
        """

    @mix.setter
    def mix(self, arg1: float) -> None:
        pass

    pass


class Distortion(Plugin):
    """
    Apply soft distortion with a tanh waveshaper.
    """

    def __init__(self, drive_db: float = 25) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def drive_db(self) -> float:
        """
        :type: float
        """

    @drive_db.setter
    def drive_db(self, arg1: float) -> None:
        pass

    pass


class GSMFullRateCompressor(Plugin):
    """
    Apply an GSM Full Rate compressor to emulate the sound of a GSM Full Rate ("2G") cellular phone connection. This plugin internally resamples the input audio to 8kHz.
    """

    def __init__(
        self, quality: Resample.Quality = Resample.Quality.WindowedSinc
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def quality(self) -> Resample.Quality:
        """
        :type: Resample.Quality
        """

    @quality.setter
    def quality(self, arg1: Resample.Quality) -> None:
        pass

    pass


class Gain(Plugin):
    """
    Increase or decrease the volume of a signal by applying a gain value (in decibels). No distortion or other effects are applied.
    """

    def __init__(self, gain_db: float = 1.0) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def gain_db(self) -> float:
        """
        :type: float
        """

    @gain_db.setter
    def gain_db(self, arg1: float) -> None:
        pass

    pass


class HighShelfFilter(Plugin):
    """
    Apply a high shelf filter with variable Q and gain. Frequencies above the cutoff frequency will be boosted (or cut) by the provided gain value.
    """

    def __init__(
        self,
        cutoff_frequency_hz: float = 440,
        gain_db: float = 0.0,
        q: float = 0.7071067690849304,
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def cutoff_frequency_hz(self) -> float:
        """
        :type: float
        """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass

    @property
    def gain_db(self) -> float:
        """
        :type: float
        """

    @gain_db.setter
    def gain_db(self, arg1: float) -> None:
        pass

    @property
    def q(self) -> float:
        """
        :type: float
        """

    @q.setter
    def q(self, arg1: float) -> None:
        pass

    pass


class HighpassFilter(Plugin):
    """
    Apply a first-order high-pass filter with a roll-off of 6dB/octave. The cutoff frequency will be attenuated by -3dB (i.e.: 0.707x as loud).
    """

    def __init__(self, cutoff_frequency_hz: float = 50) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def cutoff_frequency_hz(self) -> float:
        """
        :type: float
        """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass

    pass


class Invert(Plugin):
    """
    Flip the polarity of the signal. This effect is not audible on its own.
    """

    def __init__(self) -> None:
        ...

    def __repr__(self) -> str:
        ...

    pass


class LadderFilter(Plugin):
    """
    Multi-mode audio filter based on the classic Moog synthesizer ladder filter.
    """

    class Mode:
        """
        Members:

          LPF12 : low-pass 12 dB/octave

          HPF12 : high-pass 12 dB/octave

          BPF12 : band-pass 12 dB/octave

          LPF24 : low-pass 24 dB/octave

          HPF24 : high-pass 24 dB/octave

          BPF24 : band-pass 24 dB/octave
        """

        def __eq__(self, other: object) -> bool:
            ...

        def __getstate__(self) -> int:
            ...

        def __hash__(self) -> int:
            ...

        def __index__(self) -> int:
            ...

        def __init__(self, value: int) -> None:
            ...

        def __int__(self) -> int:
            ...

        def __ne__(self, other: object) -> bool:
            ...

        def __repr__(self) -> str:
            ...

        def __setstate__(self, state: int) -> None:
            ...

        @property
        def name(self) -> str:
            """
            :type: str
            """

        @property
        def value(self) -> int:
            """
            :type: int
            """

        BPF12: LadderFilter.Mode  # value = <Mode.BPF12: 2>
        BPF24: LadderFilter.Mode  # value = <Mode.BPF24: 5>
        HPF12: LadderFilter.Mode  # value = <Mode.HPF12: 1>
        HPF24: LadderFilter.Mode  # value = <Mode.HPF24: 4>
        LPF12: LadderFilter.Mode  # value = <Mode.LPF12: 0>
        LPF24: LadderFilter.Mode  # value = <Mode.LPF24: 3>
        __members__: dict  # value = {'LPF12': <Mode.LPF12: 0>, 'HPF12': <Mode.HPF12: 1>, 'BPF12': <Mode.BPF12: 2>, 'LPF24': <Mode.LPF24: 3>, 'HPF24': <Mode.HPF24: 4>, 'BPF24': <Mode.BPF24: 5>}
        pass

    def __init__(
        self,
        mode: LadderFilter.Mode = Mode.LPF12,
        cutoff_hz: float = 200,
        resonance: float = 0,
        drive: float = 1.0,
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def cutoff_hz(self) -> float:
        """
        :type: float
        """

    @cutoff_hz.setter
    def cutoff_hz(self, arg1: float) -> None:
        pass

    @property
    def drive(self) -> float:
        """
        :type: float
        """

    @drive.setter
    def drive(self, arg1: float) -> None:
        pass

    @property
    def mode(self) -> LadderFilter.Mode:
        """
        :type: LadderFilter.Mode
        """

    @mode.setter
    def mode(self, arg1: LadderFilter.Mode) -> None:
        pass

    @property
    def resonance(self) -> float:
        """
        :type: float
        """

    @resonance.setter
    def resonance(self, arg1: float) -> None:
        pass

    BPF12: LadderFilter.Mode  # value = <Mode.BPF12: 2>
    BPF24: LadderFilter.Mode  # value = <Mode.BPF24: 5>
    HPF12: LadderFilter.Mode  # value = <Mode.HPF12: 1>
    HPF24: LadderFilter.Mode  # value = <Mode.HPF24: 4>
    LPF12: LadderFilter.Mode  # value = <Mode.LPF12: 0>
    LPF24: LadderFilter.Mode  # value = <Mode.LPF24: 3>
    pass


class Limiter(Plugin):
    """
    A simple limiter with standard threshold and release time controls, featuring two compressors and a hard clipper at 0 dB.
    """

    def __init__(self, threshold_db: float = -10.0, release_ms: float = 100.0) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def release_ms(self) -> float:
        """
        :type: float
        """

    @release_ms.setter
    def release_ms(self, arg1: float) -> None:
        pass

    @property
    def threshold_db(self) -> float:
        """
        :type: float
        """

    @threshold_db.setter
    def threshold_db(self, arg1: float) -> None:
        pass

    pass


class LowShelfFilter(Plugin):
    """
    Apply a low shelf filter with variable Q and gain. Frequencies below the cutoff frequency will be boosted (or cut) by the provided gain value.
    """

    def __init__(
        self,
        cutoff_frequency_hz: float = 440,
        gain_db: float = 0.0,
        q: float = 0.7071067690849304,
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def cutoff_frequency_hz(self) -> float:
        """
        :type: float
        """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass

    @property
    def gain_db(self) -> float:
        """
        :type: float
        """

    @gain_db.setter
    def gain_db(self, arg1: float) -> None:
        pass

    @property
    def q(self) -> float:
        """
        :type: float
        """

    @q.setter
    def q(self, arg1: float) -> None:
        pass

    pass


class LowpassFilter(Plugin):
    """
    Apply a first-order low-pass filter with a roll-off of 6dB/octave. The cutoff frequency will be attenuated by -3dB (i.e.: 0.707x as loud).
    """

    def __init__(self, cutoff_frequency_hz: float = 50) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def cutoff_frequency_hz(self) -> float:
        """
        :type: float
        """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass

    pass


class MP3Compressor(Plugin):
    """
    Apply an MP3 compressor to the audio to reduce its quality.
    """

    def __init__(self, vbr_quality: float = 2.0) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def vbr_quality(self) -> float:
        """
        :type: float
        """

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
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def attack_ms(self) -> float:
        """
        :type: float
        """

    @attack_ms.setter
    def attack_ms(self, arg1: float) -> None:
        pass

    @property
    def ratio(self) -> float:
        """
        :type: float
        """

    @ratio.setter
    def ratio(self, arg1: float) -> None:
        pass

    @property
    def release_ms(self) -> float:
        """
        :type: float
        """

    @release_ms.setter
    def release_ms(self, arg1: float) -> None:
        pass

    @property
    def threshold_db(self) -> float:
        """
        :type: float
        """

    @threshold_db.setter
    def threshold_db(self, arg1: float) -> None:
        pass

    pass


class PeakFilter(Plugin):
    """
    Apply a peak (or notch) filter with variable Q and gain. Frequencies around the cutoff frequency will be boosted (or cut) by the provided gain value.
    """

    def __init__(
        self,
        cutoff_frequency_hz: float = 440,
        gain_db: float = 0.0,
        q: float = 0.7071067690849304,
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def cutoff_frequency_hz(self) -> float:
        """
        :type: float
        """

    @cutoff_frequency_hz.setter
    def cutoff_frequency_hz(self, arg1: float) -> None:
        pass

    @property
    def gain_db(self) -> float:
        """
        :type: float
        """

    @gain_db.setter
    def gain_db(self, arg1: float) -> None:
        pass

    @property
    def q(self) -> float:
        """
        :type: float
        """

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
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def centre_frequency_hz(self) -> float:
        """
        :type: float
        """

    @centre_frequency_hz.setter
    def centre_frequency_hz(self, arg1: float) -> None:
        pass

    @property
    def depth(self) -> float:
        """
        :type: float
        """

    @depth.setter
    def depth(self, arg1: float) -> None:
        pass

    @property
    def feedback(self) -> float:
        """
        :type: float
        """

    @feedback.setter
    def feedback(self, arg1: float) -> None:
        pass

    @property
    def mix(self) -> float:
        """
        :type: float
        """

    @mix.setter
    def mix(self, arg1: float) -> None:
        pass

    @property
    def rate_hz(self) -> float:
        """
        :type: float
        """

    @rate_hz.setter
    def rate_hz(self, arg1: float) -> None:
        pass

    pass


class PitchShift(Plugin):
    """
    Shift pitch without affecting audio duration.
    """

    def __init__(self, semitones: float = 0.0) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def semitones(self) -> float:
        """
        :type: float
        """

    @semitones.setter
    def semitones(self, arg1: float) -> None:
        pass

    pass


class Bitcrush(Plugin):
    """
    A plugin that reduces the signal to a given bit depth, giving the audio a lo-fi, digitized sound. Floating-point bit depths are supported.
    """

    def __init__(self, bit_depth: float = 8) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def bit_depth(self) -> float:
        """
        :type: float
        """

    @bit_depth.setter
    def bit_depth(self, arg1: float) -> None:
        pass

    pass


class PluginContainer(Plugin):
    """
    A generic audio processing plugin that contains zero or more other plugins. Not intended for direct use.
    """

    def __contains__(self, plugin: Plugin) -> bool:
        ...

    def __delitem__(self, index: int) -> None:
        ...

    def __getitem__(self, index: int) -> Plugin:
        ...

    def __init__(self, plugins: typing.List[Plugin]) -> None:
        ...

    def __iter__(self) -> typing.Iterator:
        ...

    def __len__(self) -> int:
        ...

    def __setitem__(self, index: int, plugin: Plugin) -> None:
        ...

    def append(self, arg0: Plugin) -> None:
        ...

    def insert(self, index: int, plugin: Plugin) -> None:
        ...

    def remove(self, plugin: Plugin) -> None:
        ...

    pass


class Resample(Plugin):
    """
    A plugin that downsamples the input audio to the given sample rate, then upsamples it back to the original sample rate. Various quality settings will produce audible distortion and aliasing effects.
    """

    class Quality:
        """
        Members:

          ZeroOrderHold : The lowest quality and fastest resampling method, with lots of audible artifacts.

          Linear : A resampling method slightly less noisy than the simplest method, but not by much.

          CatmullRom : A moderately good-sounding resampling method which is fast to run.

          Lagrange : A moderately good-sounding resampling method which is slow to run.

          WindowedSinc : The highest quality and slowest resampling method, with no audible artifacts.
        """

        def __eq__(self, other: object) -> bool:
            ...

        def __getstate__(self) -> int:
            ...

        def __hash__(self) -> int:
            ...

        def __index__(self) -> int:
            ...

        def __init__(self, value: int) -> None:
            ...

        def __int__(self) -> int:
            ...

        def __ne__(self, other: object) -> bool:
            ...

        def __repr__(self) -> str:
            ...

        def __setstate__(self, state: int) -> None:
            ...

        @property
        def name(self) -> str:
            """
            :type: str
            """

        @property
        def value(self) -> int:
            """
            :type: int
            """

        CatmullRom: Resample.Quality  # value = <Quality.CatmullRom: 2>
        Lagrange: Resample.Quality  # value = <Quality.Lagrange: 3>
        Linear: Resample.Quality  # value = <Quality.Linear: 1>
        WindowedSinc: Resample.Quality  # value = <Quality.WindowedSinc: 4>
        ZeroOrderHold: Resample.Quality  # value = <Quality.ZeroOrderHold: 0>
        __members__: dict  # value = {'ZeroOrderHold': <Quality.ZeroOrderHold: 0>, 'Linear': <Quality.Linear: 1>, 'CatmullRom': <Quality.CatmullRom: 2>, 'Lagrange': <Quality.Lagrange: 3>, 'WindowedSinc': <Quality.WindowedSinc: 4>}
        pass

    def __init__(
        self,
        target_sample_rate: float = 8000.0,
        quality: Resample.Quality = Resample.Quality.WindowedSinc,
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def quality(self) -> Resample.Quality:
        """
        :type: Resample.Quality
        """

    @quality.setter
    def quality(self, arg1: Resample.Quality) -> None:
        pass

    @property
    def target_sample_rate(self) -> float:
        """
        :type: float
        """

    @target_sample_rate.setter
    def target_sample_rate(self, arg1: float) -> None:
        pass

    CatmullRom: Resample.Quality  # value = <Quality.CatmullRom: 2>
    Lagrange: Resample.Quality  # value = <Quality.Lagrange: 3>
    Linear: Resample.Quality  # value = <Quality.Linear: 1>
    WindowedSinc: Resample.Quality  # value = <Quality.WindowedSinc: 4>
    ZeroOrderHold: Resample.Quality  # value = <Quality.ZeroOrderHold: 0>
    pass


class Reverb(Plugin):
    """
    Performs a simple reverb effect on a stream of audio data. This is a simple stereo reverb, based on the technique and tunings used in FreeVerb.
    """

    def __init__(
        self,
        room_size: float = 0.5,
        damping: float = 0.5,
        wet_level: float = 0.33,
        dry_level: float = 0.4,
        width: float = 1.0,
        freeze_mode: float = 0.0,
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    @property
    def damping(self) -> float:
        """
        :type: float
        """

    @damping.setter
    def damping(self, arg1: float) -> None:
        pass

    @property
    def dry_level(self) -> float:
        """
        :type: float
        """

    @dry_level.setter
    def dry_level(self, arg1: float) -> None:
        pass

    @property
    def freeze_mode(self) -> float:
        """
        :type: float
        """

    @freeze_mode.setter
    def freeze_mode(self, arg1: float) -> None:
        pass

    @property
    def room_size(self) -> float:
        """
        :type: float
        """

    @room_size.setter
    def room_size(self, arg1: float) -> None:
        pass

    @property
    def wet_level(self) -> float:
        """
        :type: float
        """

    @wet_level.setter
    def wet_level(self, arg1: float) -> None:
        pass

    @property
    def width(self) -> float:
        """
        :type: float
        """

    @width.setter
    def width(self, arg1: float) -> None:
        pass

    pass


class _AudioProcessorParameter:
    """
    An abstract base class for parameter objects that can be added to an AudioProcessor.
    """

    def __repr__(self) -> str:
        ...

    def get_name(self, maximum_string_length: int) -> str:
        """
        Returns the name to display for this parameter, which is made to fit within the given string length
        """

    def get_raw_value_for_text(self, string_value: str) -> float:
        """
        Returns the raw value of the supplied text. Plugins may handle errors however they see fit, but will likely not raise exceptions.
        """

    def get_text_for_raw_value(
        self, raw_value: float, maximum_string_length: int = 512
    ) -> str:
        """
        Returns a textual version of the supplied normalised parameter value.
        """

    @property
    def default_raw_value(self) -> float:
        """
        The default internal value of this parameter. Convention is that this parameter should be between 0 and 1.0. This may or may not correspond with the value shown to the user.

        :type: float
        """

    @property
    def index(self) -> int:
        """
        The index of this parameter in its plugin's parameter list.

        :type: int
        """

    @property
    def is_automatable(self) -> bool:
        """
        Returns true if this parameter can be automated (i.e.: scheduled to change over time, in real-time, in a DAW).

        :type: bool
        """

    @property
    def is_boolean(self) -> bool:
        """
        Returns whether the parameter represents a boolean switch, typically with "On" and "Off" states.

        :type: bool
        """

    @property
    def is_discrete(self) -> bool:
        """
        Returns whether the parameter uses discrete values, based on the result of getNumSteps, or allows the host to select values continuously.

        :type: bool
        """

    @property
    def is_meta_parameter(self) -> bool:
        """
        A meta-parameter is a parameter that changes other parameters.

        :type: bool
        """

    @property
    def is_orientation_inverted(self) -> bool:
        """
        If true, this parameter operates in the reverse direction. (Not all plugin formats will actually use this information).

        :type: bool
        """

    @property
    def label(self) -> str:
        """
        Some parameters may be able to return a label string for their units. For example "Hz" or "%".

        :type: str
        """

    @property
    def name(self) -> str:
        """
        Returns the name to display for this parameter at its longest.

        :type: str
        """

    @property
    def num_steps(self) -> int:
        """
        Returns the number of steps that this parameter's range should be quantised into. See also: is_discrete, is_boolean.

        :type: int
        """

    @property
    def raw_value(self) -> float:
        """
        The internal value of this parameter. Convention is that this parameter should be between 0 and 1.0. This may or may not correspond with the value shown to the user.

        :type: float
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

        :type: str
        """

    pass


class _AudioUnitPlugin(Plugin):
    """
    A wrapper around any Apple Audio Unit audio effect plugin. Only available on macOS.
    """

    def __init__(
        self, path_to_plugin_file: str, plugin_name: typing.Optional[str] = None
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    def _get_parameter(self, arg0: str) -> _AudioProcessorParameter:
        ...

    @staticmethod
    def get_plugin_names_for_file(arg0: str) -> typing.List[str]:
        """
        Return a list of plugin names contained within a given Audio Unit bundle (i.e.: a ".component"). If the provided file cannot be scanned, an ImportError will be raised.
        """

    def show_editor(self) -> None:
        """
        Show the UI of this plugin as a native window. This method will block until the window is closed or a KeyboardInterrupt is received.
        """

    @property
    def _parameters(self) -> typing.List[_AudioProcessorParameter]:
        """
        :type: typing.List[_AudioProcessorParameter]
        """

    @property
    def name(self) -> str:
        """
        The name of this plugin.

        :type: str
        """

    pass


class _VST3Plugin(Plugin):
    """
    A wrapper around any Steinberg® VST3 audio effect plugin. Note that plugins must already support the operating system currently in use (i.e.: if you're running Linux but trying to open a VST that does not support Linux, this will fail).
    """

    def __init__(
        self, path_to_plugin_file: str, plugin_name: typing.Optional[str] = None
    ) -> None:
        ...

    def __repr__(self) -> str:
        ...

    def _get_parameter(self, arg0: str) -> _AudioProcessorParameter:
        ...

    @staticmethod
    def get_plugin_names_for_file(arg0: str) -> typing.List[str]:
        """
        Return a list of plugin names contained within a given VST3 plugin (i.e.: a ".vst3"). If the provided file cannot be scanned, an ImportError will be raised.
        """

    def load_preset(self, preset_file_path: str) -> None:
        """
        Load a VST3 preset file in .vstpreset format.
        """

    def show_editor(self) -> None:
        """
        Show the UI of this plugin as a native window. This method will block until the window is closed or a KeyboardInterrupt is received.
        """

    @property
    def _parameters(self) -> typing.List[_AudioProcessorParameter]:
        """
        :type: typing.List[_AudioProcessorParameter]
        """

    @property
    def name(self) -> str:
        """
        The name of this plugin.

        :type: str
        """

    pass
