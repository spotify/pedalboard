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
import pedalboard_native

__all__ = ["Chain", "Mix"]

class Chain(pedalboard_native.PluginContainer, pedalboard_native.Plugin):
    """
    Run zero or more plugins as a plugin. Useful when used with the Mix plugin.
    """

    @typing.overload
    @typing.overload
    def __init__(self, plugins: typing.List[pedalboard_native.Plugin]) -> None: ...
    def __repr__(self) -> str: ...
    pass

class Mix(pedalboard_native.PluginContainer, pedalboard_native.Plugin):
    """
    A utility plugin that allows running other plugins in parallel. All plugins provided will be mixed equally.
    """

    @typing.overload
    @typing.overload
    def __init__(self, plugins: typing.List[pedalboard_native.Plugin]) -> None: ...
    def __repr__(self) -> str: ...
    pass
