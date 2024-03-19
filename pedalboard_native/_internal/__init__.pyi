from __future__ import annotations
import pedalboard_native._internal

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
import pedalboard_native

__all__ = [
    "AddLatency",
    "FixedSizeBlockTestPlugin",
    "ForceMonoTestPlugin",
    "PrimeWithSilenceTestPlugin",
    "ResampleWithLatency",
]

class AddLatency(pedalboard_native.Plugin):
    """
    A dummy plugin that delays input audio for the given number of samples before passing it back to the output. Used internally to test Pedalboard's automatic latency compensation. Probably not useful as a real effect.
    """

    def __init__(self, samples: int = 44100) -> None: ...
    pass

class FixedSizeBlockTestPlugin(pedalboard_native.Plugin):
    def __init__(self, expected_block_size: int = 160) -> None: ...
    def __repr__(self) -> str: ...
    pass

class ForceMonoTestPlugin(pedalboard_native.Plugin):
    def __repr__(self) -> str: ...
    pass

class PrimeWithSilenceTestPlugin(pedalboard_native.Plugin):
    def __init__(self, expected_silent_samples: int = 160) -> None: ...
    def __repr__(self) -> str: ...
    pass

class ResampleWithLatency(pedalboard_native.Plugin):
    def __init__(
        self,
        target_sample_rate: float = 8000.0,
        internal_latency: int = 1024,
        quality: pedalboard_native.Resample.Quality = pedalboard_native.Resample.Quality.WindowedSinc,
    ) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def quality(self) -> pedalboard_native.Resample.Quality:
        """ """

    @quality.setter
    def quality(self, arg1: pedalboard_native.Resample.Quality) -> None:
        pass

    @property
    def target_sample_rate(self) -> float:
        """ """

    @target_sample_rate.setter
    def target_sample_rate(self, arg1: float) -> None:
        pass
    pass
