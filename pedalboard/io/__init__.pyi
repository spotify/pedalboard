"""This module provides classes and functions for reading and writing audio files.

*Introduced in v0.5.1.*"""
from __future__ import annotations
import pedalboard_native.io  # type: ignore
import typing
from typing_extensions import Literal
from enum import Enum
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

    :class:`AudioFile` may be used just like a regular Python ``open``
    function call, to open an audio file for reading (with the default ``"r"`` mode)
    or for writing (with the ``"w"`` mode).

    Unlike a typical ``open`` call:
     - :class:`AudioFile` objects can only be created in read (``"r"``) or write (``"w"``) mode.
       All audio files are binary (so a trailing ``b`` would be redundant) and appending to an
       existing audio file is not possible.
     - If opening an audio file in write mode (``"w"``), one additional argument is required:
       the sample rate of the file.
     - A file-like object can be provided to :class:`AudioFile`, allowing for reading and
       writing to in-memory streams or buffers. The provided file-like object must be seekable
       and must be opened in binary mode (i.e.: ``io.BinaryIO`` instead of ``io.StringIO``,
       if using the `io` package).


    Examples
    --------

    Opening an audio file on disk::

       with AudioFile("my_file.mp3") as f:
           first_ten_seconds = f.read(int(f.samplerate * 10))


    Opening a file-like object::

       ogg_buffer: io.BytesIO = get_audio_buffer(...)
       with AudioFile(ogg_buffer) as f:
           first_ten_seconds = f.read(int(f.samplerate * 10))


    Writing an audio file on disk::

       with AudioFile("white_noise.wav", "w", samplerate=44100, num_channels=2) as f:
           f.write(np.random.rand(2, 44100))


    Writing encoded audio to a file-like object::

       wav_buffer = io.BytesIO()
       with AudioFile(wav_buffer, "w", samplerate=44100, num_channels=2) as f:
           f.write(np.random.rand(2, 44100))
       wav_buffer.getvalue()  # do something with the file-like object


    Writing to an audio file while also specifying quality options for the codec::

       with AudioFile(
           "white_noise.mp3",
           "w",
           samplerate=44100,
           num_channels=2,
           quality=160,  # kilobits per second
       ) as f:
           f.write(np.random.rand(2, 44100))


    Re-encoding a WAV file as an MP3 in four lines of Python::

       with AudioFile("input.wav") as i:
           with AudioFile("output.mp3", "w", i.samplerate, i.num_channels) as o:
               while i.tell() < i.frames:
                   o.write(i.read(1024))
    """

    def __init__(self) -> None: ...
    @staticmethod
    @typing.overload
    def __new__(
        cls: object, file_like: typing.BinaryIO, mode: Literal["r"] = "r"
    ) -> ReadableAudioFile:
        """
        Open an audio file for reading.

        Open a file-like object for reading. The provided object must have ``read``, ``seek``, ``tell``, and ``seekable`` methods, and must return binary data (i.e.: ``open(..., "w")`` or ``io.BinaryIO``, etc.).
        """
    @staticmethod
    @typing.overload
    def __new__(
        cls: object,
        file_like: typing.BinaryIO,
        mode: Literal["w"],
        samplerate: typing.Optional[float] = None,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: typing.Optional[typing.Union[str, float]] = None,
        format: typing.Optional[str] = None,
    ) -> WriteableAudioFile: ...
    @staticmethod
    @typing.overload
    def __new__(cls: object, filename: str, mode: Literal["r"] = "r") -> ReadableAudioFile: ...
    @staticmethod
    @typing.overload
    def __new__(
        cls: object,
        filename: str,
        mode: Literal["w"],
        samplerate: typing.Optional[float] = None,
        num_channels: int = 1,
        bit_depth: int = 16,
        quality: typing.Optional[typing.Union[str, float]] = None,
    ) -> WriteableAudioFile: ...
    pass

class ReadableAudioFile(AudioFile):
    """
    A class that wraps an audio file for reading, with native support for Ogg Vorbis,
    MP3, WAV, FLAC, and AIFF files on all operating systems. Other formats may also
    be readable depending on the operating system and installed system libraries:

     - macOS: ``.3g2``, ``.3gp``, ``.aac``, ``.ac3``, ``.adts``, ``.aif``,
       ``.aifc``, ``.aiff``, ``.amr``, ``.au``, ``.bwf``, ``.caf``,
       ``.ec3``, ``.flac``, ``.latm``, ``.loas``, ``.m4a``, ``.m4b``,
       ``.m4r``, ``.mov``, ``.mp1``, ``.mp2``, ``.mp3``, ``.mp4``,
       ``.mpa``, ``.mpeg``, ``.ogg``, ``.qt``, ``.sd2``,
       ``.snd``, ``.w64``, ``.wav``, ``.xhe``
     - Windows: ``.aif``, ``.aiff``, ``.flac``, ``.mp3``, ``.ogg``,
       ``.wav``, ``.wma``
     - Linux: ``.aif``, ``.aiff``, ``.flac``, ``.mp3``, ``.ogg``,
       ``.wav``

    Use :meth:`pedalboard.io.get_supported_read_formats()` to see which
    formats or file extensions are supported on the current platform.

    (Note that although an audio file may have a certain file extension, its
    contents may be encoded with a compression algorithm unsupported by
    Pedalboard.)

    .. note::
        This class will probably not be used directly: the :class:`AudioFile` class's
        ``__new__`` operator will return an instance of :class:`ReadableAudioFile` when
        provided the ``"r"`` mode as its second argument (the default).
    """

    def __enter__(self) -> ReadableAudioFile:
        """
        Use this :class:`ReadableAudioFile` as a context manager, automatically closing the file and releasing resources when the context manager exits.
        """
    def __exit__(self, arg0: object, arg1: object, arg2: object) -> None:
        """
        Stop using this :class:`ReadableAudioFile` as a context manager, close the file, release its resources.
        """
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
        Read the given number of frames (samples in each channel) from this audio file at its current position.

        ``num_frames`` is a required argument, as audio files can be deceptively large. (Consider that an hour-long ``.ogg`` file may be only a handful of megabytes on disk, but may decompress to nearly a gigabyte in memory.) Audio files should be read in chunks, rather than all at once, to avoid hard-to-debug memory problems and out-of-memory crashes.

        Audio samples are returned as a multi-dimensional :class:`numpy.array` with the shape ``(channels, samples)``; i.e.: a stereo audio file will have shape ``(2, <length>)``. Returned data is always in the ``float32`` datatype.

        For most (but not all) audio files, the minimum possible sample value will be ``-1.0f`` and the maximum sample value will be ``+1.0f``.
        """
    def read_raw(self, num_frames: int = 0) -> numpy.ndarray:
        """
        Read the given number of frames (samples in each channel) from this audio file at the current position.

        Audio samples are returned as a multi-dimensional :class:`numpy.array` with the shape ``(channels, samples)``; i.e.: a stereo audio file will have shape ``(2, <length>)``. Returned data is in the raw format stored by the underlying file (one of ``int8``, ``int16``, ``int32``, or ``float32``).
        """
    def seek(self, position: int) -> None:
        """
        Seek this file to the provided location in frames. Future reads will start from this position.
        """
    def seekable(self) -> bool:
        """
        Returns True if this file is currently open and calls to seek() will work.
        """
    def tell(self) -> int:
        """
        Return the current position of the read pointer in this audio file, in frames. This value will increase as :meth:`read` is called, and may decrease if :meth:`seek` is called.
        """
    @property
    def closed(self) -> bool:
        """
        True iff this file is closed (and no longer usable), False otherwise.


        """
    @property
    def duration(self) -> float:
        """
        The duration of this file in seconds (``frames`` divided by ``samplerate``).


        """
    @property
    def file_dtype(self) -> str:
        """
        The data type (``"int16"``, ``"float32"``, etc) stored natively by this file.

        Note that :meth:`read` will always return a ``float32`` array, regardless of the value of this property. Use :meth:`read_raw` to read data from the file in its ``file_dtype``.


        """
    @property
    def frames(self) -> int:
        """
        The total number of frames (samples per channel) in this file.

        For example, if this file contains 10 seconds of stereo audio at sample rate of 44,100 Hz, ``frames`` will return ``441,000``.


        """
    @property
    def name(self) -> typing.Optional[str]:
        """
        The name of this file.

        If this :class:`ReadableAudioFile` was opened from a file-like object, this will be ``None``.


        """
    @property
    def num_channels(self) -> int:
        """
        The number of channels in this file.


        """
    @property
    def samplerate(self) -> float:
        """
        The sample rate of this file in samples (per channel) per second (Hz).


        """
    pass

class WriteableAudioFile(AudioFile):
    """
    A class that wraps an audio file for writing, with native support for Ogg Vorbis,
    MP3, WAV, FLAC, and AIFF files on all operating systems.

    Use :meth:`pedalboard.io.get_supported_write_formats()` to see which
    formats or file extensions are supported on the current platform.

    Args:
        filename_or_file_like:
            The path to an output file to write to, or a seekable file-like
            binary object (like ``io.BytesIO``) to write to.

        samplerate:
            The sample rate of the audio that will be written to this file.
            All calls to the :meth:`write` method will assume this sample rate
            is used.

        num_channels:
            The number of channels in the audio that will be written to this file.
            All calls to the :meth:`write` method will expect audio with this many
            channels, and will throw an exception if the audio does not contain
            this number of channels.

        bit_depth:
            The bit depth (number of bits per sample) that will be written
            to this file. Used for raw formats like WAV and AIFF. Will have no effect
            on compressed formats like MP3 or Ogg Vorbis.

        quality:
            An optional string or number that indicates the quality level to use
            for the given audio compression codec. Different codecs have different
            compression quality values; numeric values like ``128`` and ``256`` will
            usually indicate the number of kilobits per second used by the codec.
            Some formats, like MP3, support more advanced options like ``V2`` (as
            specified by `the LAME encoder <https://lame.sourceforge.io/>`_) which
            may be passed as a string. The strings ``"best"``, ``"worst"``,
            ``"fastest"``, and ``"smallest"`` will also work for any codec.

    .. note::
        This class will probably not be used directly: the :class:`AudioFile` class's
        ``__new__`` operator will return an instance of :class:`WriteableAudioFile` when
        provided the ``"w"`` mode as its second argument. All of the parameters accepted
        by the :class:`WriteableAudioFile` constructor will be accepted by
        :class:`AudioFile` as well.
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
    def write(self, samples: numpy.ndarray) -> None:
        """
        Encode an array of audio data and write it to this file. The number of channels in the array must match the number of channels used to open the file. The array may contain audio in any shape. If the file's bit depth or format does not match the provided data type, the audio will be automatically converted.

        Arrays of type int8, int16, int32, float32, and float64 are supported. If an array of an unsupported ``dtype`` is provided, a ``TypeError`` will be raised.
        """
    @property
    def closed(self) -> bool:
        """
        If this file has been closed, this property will be True.


        """
    @property
    def file_dtype(self) -> str:
        """
        The data type stored natively by this file. Note that write(...) will accept multiple datatypes, regardless of the value of this property.


        """
    @property
    def frames(self) -> int:
        """
        The total number of frames (samples per channel) written to this file so far.


        """
    @property
    def num_channels(self) -> int:
        """
        The number of channels in this file.


        """
    @property
    def quality(self) -> typing.Optional[str]:
        """
        The quality setting used to write this file. For many formats, this may be ``None``.

        Quality options differ based on the audio codec used in the file. Most codecs specify a number of bits per second in 16- or 32-bit-per-second increments (128 kbps, 160 kbps, etc). Some codecs provide string-like options for variable bit-rate encoding (i.e. "V0" through "V9" for MP3). The strings ``"best"``, ``"worst"``, ``"fastest"``, and ``"smallest"`` will also work for any codec.


        """
    @property
    def samplerate(self) -> float:
        """
        The sample rate of this file in samples (per channel) per second (Hz).


        """
    pass

def get_supported_read_formats() -> typing.List[str]:
    pass

def get_supported_write_formats() -> typing.List[str]:
    pass
