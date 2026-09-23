The ``pedalboard`` API
======================

This module provides classes and functions for adding effects to audio.
Most classes in this module are subclasses of :class:`Plugin`, each of
which allows applying effects to an audio buffer or stream.

.. note::
   For audio I/O functionality (i.e.: reading and writing audio files), see
   the :py:mod:`pedalboard.io` module.

The :py:mod:`pedalboard` module is named after
`the concept of a guitar pedalboard <https://en.wikipedia.org/wiki/Guitar_pedalboard>`_,
in which musicians will chain various effects pedals together to give them
complete control over their sound. The :py:mod:`pedalboard` module implements
this concept with its main :class:`Pedalboard` class::

   from pedalboard import Pedalboard, Chorus, Distortion, Reverb

   # Create an empty Pedalboard object:
   my_pedalboard = Pedalboard()

   # Treat this object like a Python list:
   my_pedalboard.append(Chorus())
   my_pedalboard.append(Distortion())
   my_pedalboard.append(Reverb())

   # Pass audio through this pedalboard:
   output_audio = my_pedalboard(input_audio, input_audio_samplerate)


:class:`Pedalboard` objects are lists of zero or more :class:`Plugin` objects,
and :class:`Pedalboard` objects themselves are subclasses of :class:`Plugin` -
which allows for nesting and composition.

The following documentation lists all of the available types of :class:`Plugin`.


.. automodule:: pedalboard
   :members:
   :imported-members:
   :inherited-members:
   :special-members: __call__
