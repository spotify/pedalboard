"""Typed public audio API for pedalboard.jax."""

from typing import Any
from jax import Array as _Array
import pedalboard as _native
from ._framework_types import (
    AudioFile as _AudioFile,
    Processor as _Processor,
    ExternalPlugin as _ExternalPlugin,
)

# Processing returns framework arrays, intentionally overriding native NumPy return types.
__all__: list[str]

class AudioUnitPlugin(_Processor[_Array], _native.AudioUnitPlugin): ...  # type: ignore[override]
class Bitcrush(_Processor[_Array], _native.Bitcrush): ...  # type: ignore[override]
class BrickwallLimiter(_Processor[_Array], _native.BrickwallLimiter): ...  # type: ignore[override]
class Chain(_Processor[_Array], _native.Chain): ...  # type: ignore[override]
class Chorus(_Processor[_Array], _native.Chorus): ...  # type: ignore[override]
class Clipping(_Processor[_Array], _native.Clipping): ...  # type: ignore[override]
class Compressor(_Processor[_Array], _native.Compressor): ...  # type: ignore[override]
class Convolution(_Processor[_Array], _native.Convolution): ...  # type: ignore[override]
class Delay(_Processor[_Array], _native.Delay): ...  # type: ignore[override]
class Distortion(_Processor[_Array], _native.Distortion): ...  # type: ignore[override]
class GSMFullRateCompressor(_Processor[_Array], _native.GSMFullRateCompressor): ...  # type: ignore[override]
class Gain(_Processor[_Array], _native.Gain): ...  # type: ignore[override]
class HighShelfFilter(_Processor[_Array], _native.HighShelfFilter): ...  # type: ignore[override]
class HighpassFilter(_Processor[_Array], _native.HighpassFilter): ...  # type: ignore[override]
class IIRFilter(_Processor[_Array], _native.IIRFilter): ...  # type: ignore[override]
class Invert(_Processor[_Array], _native.Invert): ...  # type: ignore[override]
class LadderFilter(_Processor[_Array], _native.LadderFilter): ...  # type: ignore[override]
class Limiter(_Processor[_Array], _native.Limiter): ...  # type: ignore[override]
class LowShelfFilter(_Processor[_Array], _native.LowShelfFilter): ...  # type: ignore[override]
class LowpassFilter(_Processor[_Array], _native.LowpassFilter): ...  # type: ignore[override]
class MP3Compressor(_Processor[_Array], _native.MP3Compressor): ...  # type: ignore[override]
class Mix(_Processor[_Array], _native.Mix): ...  # type: ignore[override]
class NoiseGate(_Processor[_Array], _native.NoiseGate): ...  # type: ignore[override]
class PeakFilter(_Processor[_Array], _native.PeakFilter): ...  # type: ignore[override]
class Pedalboard(_Processor[_Array], _native.Pedalboard): ...  # type: ignore[override]
class Phaser(_Processor[_Array], _native.Phaser): ...  # type: ignore[override]
class PitchShift(_Processor[_Array], _native.PitchShift): ...  # type: ignore[override]
class Resample(_Processor[_Array], _native.Resample): ...  # type: ignore[override]
class Reverb(_Processor[_Array], _native.Reverb): ...  # type: ignore[override]
class VST3Plugin(_Processor[_Array], _native.VST3Plugin): ...  # type: ignore[override]
class AudioFile(_AudioFile[_Array]): ...

def process(
    input_array: Any,
    sample_rate: float,
    plugins: list[_native.Plugin],
    buffer_size: int = 8192,
    reset: bool = True,
) -> _Array: ...
def time_stretch(
    input_audio: Any,
    samplerate: float,
    stretch_factor: Any = 1.0,
    pitch_shift_in_semitones: Any = 0.0,
    high_quality: bool = True,
    transient_mode: str = "crisp",
    transient_detector: str = "compound",
    retain_phase_continuity: bool = True,
    use_long_fft_window: bool | None = None,
    use_time_domain_smoothing: bool = False,
    preserve_formants: bool = True,
) -> _Array: ...
def load_plugin(
    path_to_plugin_file: str,
    parameter_values: dict[str, str | int | float | bool] = ...,
    plugin_name: str | None = None,
    initialization_timeout: float = 10.0,
) -> _ExternalPlugin[_Array]: ...
