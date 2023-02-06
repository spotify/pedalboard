from __future__ import annotations
import pedalboard_native.utils
import typing
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
