from __future__ import annotations
import pedalboard_native.io  # type: ignore
import sys
import typing

if sys.version_info < (3, 8):
    from typing_extensions import Literal

    typing.Literal = Literal
import numpy

_Shape = typing.Tuple[int, ...]

__all__ = [
    "AudioFile",
    "ReadableAudioFile",
    "WriteableAudioFile",
    "get_supported_read_formats",
    "get_supported_write_formats",
]

class AudioFile:
    """
    A base class for readable and writeable audio files.
    """

    def __init__(self) -> None: ...
    @staticmethod
    @typing.overload
    def __new__(
        cls: object, file_like: typing.BinaryIO, mode: typing.Literal["r"] = "r"
    ) -> ReadableAudioFile: ...
    @staticmethod
    @typing.overload
    def __new__(
        cls: object,
        file_like: typing.BinaryIO,
        mode: typing.Literal["w"],
        samplerate: typing.Optional[float] = None,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: typing.Optional[typing.Union[str, float]] = None,
        format: typing.Optional[str] = None,
    ) -> WriteableAudioFile: ...
    @staticmethod
    @typing.overload
    def __new__(
        cls: object, filename: str, mode: typing.Literal["r"] = "r"
    ) -> ReadableAudioFile: ...
    @staticmethod
    @typing.overload
    def __new__(
        cls: object,
        filename: str,
        mode: typing.Literal["w"],
        samplerate: typing.Optional[float] = None,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: typing.Optional[typing.Union[str, float]] = None,
    ) -> WriteableAudioFile: ...
    pass

class ReadableAudioFile(AudioFile):
    """
    An audio file reader interface, with native support for Ogg Vorbis, MP3, WAV, FLAC, and AIFF files on all operating systems. On some platforms, other formats may also be readable. (Use pedalboard.io.get_supported_read_formats() to see which formats are supported on the current platform.)
    """

    def __enter__(self) -> ReadableAudioFile: ...
    def __exit__(self, arg0: object, arg1: object, arg2: object) -> None: ...
    @typing.overload
    def __init__(self, file_like: typing.BinaryIO) -> None: ...
    @typing.overload
    def __init__(self, filename: str) -> None: ...
    @staticmethod
    @typing.overload
    def __new__(cls: object, file_like: typing.BinaryIO) -> ReadableAudioFile: ...
    @staticmethod
    @typing.overload
    def __new__(cls: object, filename: str) -> ReadableAudioFile: ...
    def __repr__(self) -> str: ...
    def close(self) -> None:
        """
        Close this file, rendering this object unusable.
        """
    def read(self, num_frames: int = 0) -> numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]:
        """
        Read the given number of frames (samples in each channel) from this audio file at the current position. Audio samples are returned in the shape (channels, samples); i.e.: a stereo audio file will have shape (2, <length>). Returned data is always in float32 format.
        """
    def read_raw(self, num_frames: int = 0) -> numpy.ndarray:
        """
        Read the given number of frames (samples in each channel) from this audio file at the current position. Audio samples are returned in the shape (channels, samples); i.e.: a stereo audio file will have shape (2, <length>). Returned data is in the raw format stored by the underlying file (one of int8, int16, int32, or float32).
        """
    def seek(self, position: int) -> None:
        """
        Seek this file to the provided location in frames.
        """
    def seekable(self) -> bool:
        """
        Returns True if this file is currently open and calls to seek() will work.
        """
    def tell(self) -> int:
        """
        Fetch the position in this audio file, in frames.
        """
    @property
    def closed(self) -> bool:
        """
        If this file has been closed, this property will be True.

        :type: bool
        """
    @property
    def duration(self) -> float:
        """
        The duration of this file (frames divided by sample rate).

        :type: float
        """
    @property
    def file_dtype(self) -> str:
        """
        The data type stored natively by this file. Note that read(...) will always return a float32 array, regardless of the value of this property.

        :type: str
        """
    @property
    def frames(self) -> int:
        """
        The total number of frames (samples per channel) in this file.

        :type: int
        """
    @property
    def name(self) -> str:
        """
        The name of this file.

        :type: str
        """
    @property
    def num_channels(self) -> int:
        """
        The number of channels in this file.

        :type: int
        """
    @property
    def samplerate(self) -> float:
        """
        The sample rate of this file in samples (per channel) per second (Hz).

        :type: float
        """
    pass

class WriteableAudioFile(AudioFile):
    """
    An audio file writer interface, with native support for Ogg Vorbis, WAV, FLAC, and AIFF files on all operating systems. (Use pedalboard.io.get_supported_write_formats() to see which additional formats are supported on the current platform.)
    """

    def __enter__(self) -> WriteableAudioFile: ...
    def __exit__(self, arg0: object, arg1: object, arg2: object) -> None: ...
    @typing.overload
    def __init__(
        self,
        file_like: typing.BinaryIO,
        samplerate: float,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: typing.Optional[typing.Union[str, float]] = None,
        format: typing.Optional[str] = None,
    ) -> None: ...
    @typing.overload
    def __init__(
        self,
        filename: str,
        samplerate: float,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: typing.Optional[typing.Union[str, float]] = None,
    ) -> None: ...
    @staticmethod
    @typing.overload
    def __new__(
        cls: object,
        file_like: typing.BinaryIO,
        samplerate: typing.Optional[float] = None,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: typing.Optional[typing.Union[str, float]] = None,
        format: typing.Optional[str] = None,
    ) -> WriteableAudioFile: ...
    @staticmethod
    @typing.overload
    def __new__(
        cls: object,
        filename: str,
        samplerate: typing.Optional[float] = None,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: typing.Optional[typing.Union[str, float]] = None,
    ) -> WriteableAudioFile: ...
    def __repr__(self) -> str: ...
    def close(self) -> None:
        """
        Close this file, flushing its contents to disk and rendering this object unusable for further writing.
        """
    def flush(self) -> None:
        """
        Attempt to flush this audio file's contents to disk. Not all formats support flushing, so this may throw a RuntimeError. (If this happens, closing the file will reliably force a flush to occur.)
        """
    @typing.overload
    def write(self, samples: numpy.ndarray[typing.Any, numpy.dtype[numpy.float32]]) -> None:
        """
        Encode an array of int8 (8-bit signed integer) audio data and write it to this file. The number of channels in the array must match the number of channels used to open the file. The array may contain audio in any shape. If the file's bit depth or format does not match this data type, the audio will be automatically converted.

        Encode an array of int16 (16-bit signed integer) audio data and write it to this file. The number of channels in the array must match the number of channels used to open the file. The array may contain audio in any shape. If the file's bit depth or format does not match this data type, the audio will be automatically converted.

        Encode an array of int32 (32-bit signed integer) audio data and write it to this file. The number of channels in the array must match the number of channels used to open the file. The array may contain audio in any shape. If the file's bit depth or format does not match this data type, the audio will be automatically converted.

        Encode an array of float32 (32-bit floating-point) audio data and write it to this file. The number of channels in the array must match the number of channels used to open the file. The array may contain audio in any shape. If the file's bit depth or format does not match this data type, the audio will be automatically converted.

        Encode an array of float64 (64-bit floating-point) audio data and write it to this file. The number of channels in the array must match the number of channels used to open the file. The array may contain audio in any shape. No supported formats support float64 natively, so the audio will be converted automatically.
        """
    @typing.overload
    def write(self, samples: numpy.ndarray[typing.Any, numpy.dtype[numpy.float64]]) -> None: ...
    @typing.overload
    def write(self, samples: numpy.ndarray[typing.Any, numpy.dtype[numpy.int16]]) -> None: ...
    @typing.overload
    def write(self, samples: numpy.ndarray[typing.Any, numpy.dtype[numpy.int32]]) -> None: ...
    @typing.overload
    def write(self, samples: numpy.ndarray[typing.Any, numpy.dtype[numpy.int8]]) -> None: ...
    @property
    def closed(self) -> bool:
        """
        If this file has been closed, this property will be True.

        :type: bool
        """
    @property
    def file_dtype(self) -> str:
        """
        The data type stored natively by this file. Note that write(...) will accept multiple datatypes, regardless of the value of this property.

        :type: str
        """
    @property
    def frames(self) -> int:
        """
        The total number of frames (samples per channel) written to this file so far.

        :type: int
        """
    @property
    def num_channels(self) -> int:
        """
        The number of channels in this file.

        :type: int
        """
    @property
    def quality(self) -> typing.Optional[str]:
        """
        The quality setting used to write this file. For many formats, this may be None.

        :type: typing.Optional[str]
        """
    @property
    def samplerate(self) -> float:
        """
        The sample rate of this file in samples (per channel) per second (Hz).

        :type: float
        """
    pass

def get_supported_read_formats() -> typing.List[str]:
    pass

def get_supported_write_formats() -> typing.List[str]:
    pass
