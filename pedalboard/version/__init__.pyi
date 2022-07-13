from __future__ import annotations
import pedalboard.version
import sys
import typing

if sys.version_info < (3, 8):
    import typing_extensions

    _Literal = typing_extensions.Literal
else:
    _Literal = typing.Literal

__all__ = ["MAJOR", "MINOR", "PATCH"]

MAJOR = 0
MINOR = 5
PATCH = 5
__version__ = "0.5.5"
