from __future__ import annotations
import pedalboard_native.utils

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
import pedalboard_native

_Shape = typing.Tuple[int, ...]

__all__ = ["Chain", "Mix", "time_stretch"]

class Chain(pedalboard_native.PluginContainer, pedalboard_native.Plugin):
    """
    Run zero or more plugins as a plugin. Useful when used with the Mix plugin.
    """

    @typing.overload
    def __init__(self, plugins: typing.List[pedalboard_native.Plugin]) -> None: ...
    @typing.overload
    def __repr__(self) -> str: ...
    pass

class Mix(pedalboard_native.PluginContainer, pedalboard_native.Plugin):
    """
    A utility plugin that allows running other plugins in parallel. All plugins provided will be mixed equally.
    """

    @typing.overload
    def __init__(self, plugins: typing.List[pedalboard_native.Plugin]) -> None: ...
    @typing.overload
    def __repr__(self) -> str: ...
    pass

def time_stretch(
    input_audio: numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]],
    samplerate: float,
    stretch_factor: typing.Union[
        float, numpy.ndarray[typing.Any, numpy.dtype[numpy.float64]]
    ] = 1.0,
    pitch_shift_in_semitones: typing.Union[
        float, numpy.ndarray[typing.Any, numpy.dtype[numpy.float64]]
    ] = 0.0,
    high_quality: bool = True,
    transient_mode: str = "crisp",
    transient_detector: str = "compound",
    retain_phase_continuity: bool = True,
    use_long_fft_window: typing.Optional[bool] = None,
    use_time_domain_smoothing: bool = False,
    preserve_formants: bool = True,
) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
    """
    Time-stretch (and optionally pitch-shift) a buffer of audio, changing its length.

    Using a higher ``stretch_factor`` will shorten the audio - i.e., a ``stretch_factor``
    of ``2.0`` will double the *speed* of the audio and halve the *length* of the audio,
    without changing the pitch of the audio.

    This function allows for changing the pitch of the audio during the time stretching
    operation. The ``stretch_factor`` and ``pitch_shift_in_semitones`` arguments are
    independent and do not affect each other (i.e.: you can change one, the other, or both
    without worrying about how they interact).

    Both ``stretch_factor`` and ``pitch_shift_in_semitones`` can be either floating-point
    numbers or NumPy arrays of double-precision floating point numbers. Providing a NumPy
    array allows the stretch factor and/or pitch shift to vary over the length of the
    output audio.

    .. note::
        If a NumPy array is provided for ``stretch_factor`` or ``pitch_shift_in_semitones``:
          - The length of each array must be the same as the length of the input audio.
          - More frequent changes in the stretch factor or pitch shift will result in
            slower processing, as the audio will be processed in smaller chunks.
          - Changes to the ``stretch_factor`` or ``pitch_shift_in_semitones`` more frequent
            than once every 1,024 samples (23 milliseconds at 44.1kHz) will not have any
            effect.

    The additional arguments provided to this function allow for more fine-grained control
    over the behavior of the time stretcher:

      - ``high_quality`` (the default) enables a higher quality time stretching mode.
        Set this option to ``False`` to use less CPU power.

      - ``transient_mode`` controls the behavior of the stretcher around transients
        (percussive parts of the audio). Valid options are ``"crisp"`` (the default),
        ``"mixed"``, or ``"smooth"``.

      - ``transient_detector`` controls which method is used to detect transients in the
        audio signal. Valid options are ``"compound"`` (the default), ``"percussive"``,
        or ``"soft"``.

      - ``retain_phase_continuity`` ensures that the phases of adjacent frequency bins in
        the audio stream are kept as similar as possible. Set this to ``False`` for a
        softer, phasier sound.

      - ``use_long_fft_window`` controls the size of the fast-Fourier transform window
        used during stretching. The default (``None``) will result in a window size that
        varies based on other parameters and should produce better results in most
        situations. Set this option to ``True`` to result in a smoother sound (at the
        expense of clarity and timing), or ``False`` to result in a crisper sound.

      - ``use_time_domain_smoothing`` can be enabled to produce a softer sound with
        audible artifacts around sharp transients. This option mixes well with
        ``use_long_fft_window=False``.

      - ``preserve_formants`` allows shifting the pitch of notes without substantially
        affecting the pitch profile (formants) of a voice or instrument.

    .. warning::
        This is a function, not a :py:class:`Plugin` instance, and cannot be
        used in :py:class:`Pedalboard` objects, as it changes the duration of
        the audio stream.


    .. note::
        The ability to pass a NumPy array for ``stretch_factor`` and
        ``pitch_shift_in_semitones`` was added in Pedalboard v0.9.8.
    """
