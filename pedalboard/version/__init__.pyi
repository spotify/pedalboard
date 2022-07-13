from __future__ import annotations
import pedalboard.version
import sys
import typing

if sys.version_info < (3, 8):
    from typing_extensions import Literal

    typing.Literal = Literal

__all__ = ["MAJOR", "MINOR", "PATCH"]

MAJOR = 0
MINOR = 5
PATCH = 5
__version__ = "0.5.5"
