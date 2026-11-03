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
