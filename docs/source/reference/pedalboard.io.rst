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
   - Changing the number of channels can be accomplished by calling :py:meth:`pedalboard.io.ReadableAudioFile.mono`, :py:meth:`pedalboard.io.ReadableAudioFile.stereo`, or :py:meth:`pedalboard.io.ReadableAudioFile.with_channels`.

   If you find yourself importing :class:`pedalboard.io.ReadableAudioFile`,
   :class:`pedalboard.io.WriteableAudioFile`, :class:`pedalboard.io.ResampledReadableAudioFile`,
   or :class:`pedalboard.io.ChannelConvertedReadableAudioFile` directly,
   you *probably don't need to do that* - :class:`pedalboard.io.AudioFile` has you covered.

Resampling and Channel Conversion
---------------------------------

Audio files can be resampled and have their channel count converted on-the-fly
using chainable methods. These operations stream audio efficiently without
loading the entire file into memory::

   from pedalboard.io import AudioFile

   # Resample a file to 22,050 Hz:
   with AudioFile("my_file.mp3").resampled_to(22_050) as f:
       audio = f.read(f.frames)  # audio is now at 22,050 Hz

   # Convert a stereo file to mono:
   with AudioFile("stereo_file.wav").mono() as f:
       audio = f.read(f.frames)  # audio is now shape (1, num_samples)

   # Convert a mono file to stereo:
   with AudioFile("mono_file.wav").stereo() as f:
       audio = f.read(f.frames)  # audio is now shape (2, num_samples)

These methods can be chained together in any order::

   # Resample and convert to mono:
   with AudioFile("my_file.mp3").resampled_to(22_050).mono() as f:
       audio = f.read(f.frames)

   # Or convert to mono first, then resample (slightly more efficient):
   with AudioFile("my_file.mp3").mono().resampled_to(22_050) as f:
       audio = f.read(f.frames)

.. note::
   Channel conversion is only well-defined for conversions to and from mono.
   Converting between stereo and multichannel formats (e.g., 5.1 surround)
   is not supported, as the mapping between channels is ambiguous.
   To convert multichannel audio to stereo, first convert to mono::

      # Convert 5.1 surround to stereo via mono:
      with AudioFile("surround.wav").mono().stereo() as f:
          audio = f.read(f.frames)

The following documentation lists all of the available I/O classes.


.. automodule:: pedalboard.io
   :members:
   :imported-members:
   :special-members: __enter__, __exit__
