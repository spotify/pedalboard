"""Shared array-return types for the framework module stubs."""

from types import TracebackType
from typing import Any, Generic, TypeVar

import pedalboard as _native

_Array = TypeVar("_Array")
_File = TypeVar("_File", bound="AudioFile[Any]")

class Processor(Generic[_Array]):
    def process(self, *args: Any, **kwargs: Any) -> _Array: ...
    def __call__(self, *args: Any, **kwargs: Any) -> _Array: ...

# These adapters intentionally return framework arrays instead of native NumPy arrays.
class ExternalPlugin(Processor[_Array], _native.ExternalPlugin):  # type: ignore[override]
    ...

class AudioFile(Generic[_Array]):
    def __init__(self, *args: Any, **kwargs: Any) -> None: ...
    def read(self, num_frames: int | float = 0) -> _Array: ...
    def read_raw(self, num_frames: int | float = 0) -> _Array: ...
    def write(self, samples: Any) -> None: ...
    def resampled_to(
        self: _File,
        target_sample_rate: float,
        quality: _native.Resample.Quality = ...,
    ) -> _File: ...
    def __enter__(self: _File) -> _File: ...
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> None: ...
    def __getattr__(self, name: str) -> Any: ...
    @staticmethod
    def encode(
        samples: Any,
        samplerate: float,
        format: str,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: str | float | None = None,
        codec_options: dict[Any, Any] = ...,
        num_threads: int = 1,
    ) -> bytes: ...
