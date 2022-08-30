from __future__ import annotations
import pedalboard_native.utils  # type: ignore  # type: ignore
import pedalboard  # type: ignore
import typing
from typing_extensions import Literal
from enum import Enum
import numpy
import pedalboard_native  # type: ignore  # type: ignore
import pedalboard  # type: ignore

_Shape = typing.Tuple[int, ...]

__all__ = ["Chain", "Mix"]

class Chain(pedalboard_native.PluginContainer, pedalboard_native.Plugin):
    """
    Run zero or more plugins as a plugin. Useful when used with the Mix plugin.
    """

    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(self, plugins: typing.List[pedalboard_native.Plugin]) -> None: ...
    def __repr__(self) -> str: ...
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
    ) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]: ...
    pass

class Mix(pedalboard_native.PluginContainer, pedalboard_native.Plugin):
    """
    A utility plugin that allows running other plugins in parallel. All plugins provided will be mixed equally.
    """

    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(self, plugins: typing.List[pedalboard_native.Plugin]) -> None: ...
    def __repr__(self) -> str: ...
    pass
