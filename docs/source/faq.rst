Frequently Asked Questions
--------------------------

Can Pedalboard be used with live (real-time) audio?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As of version 0.7.0, yes! See the :class:`pedalboard.io.AudioStream` class for more details.


Does Pedalboard support changing a plugin's parameters over time?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Yes! While there's no built-in function for this, it is possible to
vary the parameters of a plugin over time manually:

.. code-block:: python

   from pedalboard.io import AudioFile
   from pedalboard import Pedalboard, Compressor, Reverb
   from tqdm import tqdm

   board = Pedalboard([Compressor(), Reverb()])
   reverb = board[1]

   # Smaller step sizes would give a smoother transition,
   # at the expense of processing speed
   step_size_in_samples = 100

   # Manually step through the audio _n_ samples at a time, reading in chunks:
   with AudioFile("sample.wav") as af:

       # Open the output audio file so that we can directly write audio as we process, saving memory:
       with AudioFile(
           "sample-processed-output.wav", "w", af.samplerate, num_channels=af.num_channels
       ) as o:

           # Create a progress bar to show processing speed in real-time:
           with tqdm(total=af.frames, unit=' samples') as pbar:
               for i in range(0, af.frames, step_size_in_samples):
                   chunk = af.read(step_size_in_samples)

                   # Set the reverb's "wet" parameter to be equal to the
                   # percentage through the track (i.e.: make a ramp from 0% to 100%)
                   percentage_through_track = i / af.frames
                   reverb.wet_level = percentage_through_track

                   # Update our progress bar with the number of samples received:
                   pbar.update(chunk.shape[1])

                   # Process this chunk of audio, setting `reset` to `False`
                   # to ensure that reverb tails aren't cut off
                   output = board.process(chunk, af.samplerate, reset=False)
                   o.write(output)

With this technique, it's possible to automate any parameter. Usually, using a step size of somewhere between 100 and 1,000 (2ms to 22ms at a 44.1kHz sample rate) is small enough to avoid hearing any audio artifacts, but big enough to avoid slowing down the code dramatically.

Can Pedalboard be used with VST instruments, instead of effects?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As of version 0.7.4, yes! See the :class:`pedalboard.VST3Plugin` and :class:`pedalboard.AudioUnitPlugin` classed for more details.

Can Pedalboard plugins accept MIDI?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As of version 0.7.4, both :class:`pedalboard.VST3Plugin` and :class:`pedalboard.AudioUnitPlugin` support passing MIDI
messages to instrument plugins for audio rendering. However, effect plugins cannot yet be passed MIDI data.

How do I normalize audio?
^^^^^^^^^^^^^^^^^^^^^^^^^

There's no built-in normalization plugin, but you can measure the level of your audio with NumPy
and then apply the difference with :class:`pedalboard.Gain`. For example, to normalize a file's
peak level to -1 dBFS:

.. code-block:: python

   import numpy as np
   from pedalboard import Pedalboard, Gain
   from pedalboard.io import AudioFile

   target_peak_db = -1.0

   with AudioFile("sample.wav") as f:
       audio = f.read(f.frames)
       samplerate = f.samplerate

   # Use the loudest sample across all channels, so the stereo balance is kept:
   peak = np.max(np.abs(audio))
   if peak > 0:  # Silent audio can't be normalized
       gain_db = target_peak_db - 20 * np.log10(peak)
       audio = Pedalboard([Gain(gain_db=gain_db)])(audio, samplerate)

   with AudioFile("sample-normalized.wav", "w", samplerate, audio.shape[0]) as f:
       f.write(audio)

:class:`pedalboard.io.AudioFile` always returns floating-point samples between -1 and 1, whatever the
file's bit depth, so this works for 16-bit and 24-bit files too.

To normalize by average level (RMS) instead of peak level, measure ``np.sqrt(np.mean(audio ** 2))``
instead of the peak. Raising the RMS level can push peaks above 0 dBFS, so it's a good idea to add a
:class:`pedalboard.BrickwallLimiter` (added in version 0.9.25) after the gain to keep peaks below a ceiling:

.. code-block:: python

   from pedalboard import BrickwallLimiter

   target_rms_db = -14.0

   rms = np.sqrt(np.mean(audio ** 2))
   if rms > 0:
       gain_db = target_rms_db - 20 * np.log10(rms)
       board = Pedalboard([Gain(gain_db=gain_db), BrickwallLimiter(ceiling_db=-1.0)])
       audio = board(audio, samplerate)

Unlike :class:`pedalboard.Limiter`, :class:`pedalboard.BrickwallLimiter` doesn't add makeup gain,
so it won't change the level you just set (other than turning down any peaks above the ceiling).
