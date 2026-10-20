The ``pedalboard.io`` API
=========================

This module provides classes and functions for reading and writing audio files or streams.

.. note::
   For audio effects, see the :py:mod:`pedalboard` module.

:py:mod:`pedalboard.io` allows for reading and writing to audio files
just like working with other types of files in Python. Audio encoding
and decoding is handled automatically behind the scenes in C++, providing
high quality, high performance, thread-safe access to audio files.

The goal of :py:mod:`pedalboard.io` is to direct access to audio like
a regular file in Python::

   from pedalboard.io import AudioFile

   with AudioFile("my_filename.mp3") as f:
      print(f.duration) # => 30.0
      print(f.samplerate) # => 44100
      print(f.num_channels) # => 2
      print(f.read(f.samplerate * 10))
      # => returns a NumPy array of shape (2, 441_000):
      #    [[ 0.  0.  0. ... -0. -0. -0.]
      #     [ 0.  0.  0. ... -0. -0. -0.]]

.. note::
   The :class:`pedalboard.io.AudioFile` class should be all that's necessary for most use cases.

   - Writing to a file can be accomplished by passing ``"w"`` as a second argument, just like with regular files in Python.
   - Changing the sample rate of a file can be accomplished by calling :py:meth:`pedalboard.io.ReadableAudioFile.resampled_to`.

   If you find yourself importing :class:`pedalboard.io.ReadableAudioFile`,
   :class:`pedalboard.io.WriteableAudioFile`, or :class:`pedalboard.io.ResampledReadableAudioFile` directly,
   you *probably don't need to do that* - :class:`pedalboard.io.AudioFile` has you covered.

.. note::
   For finite, seekable WAV and RF64 files containing PCM, 32-bit IEEE float,
   or supported WAVE_FORMAT_EXTENSIBLE audio, oversized data-chunk lengths
   are limited to the complete frames physically available when the file is
   opened. Reported duration, reads, and resampling use this recovered length.
   Ambiguous repeated format or data chunks may cause the file to be rejected.

   Trailing metadata swallowed by an oversized data chunk may still be decoded
   as audio. This recovery does not cover every WAV codec or streams of unknown
   length, and it does not add support for growing files.

The following documentation lists all of the available I/O classes.


.. automodule:: pedalboard.io
   :members:
   :imported-members:
   :special-members: __enter__, __exit__
