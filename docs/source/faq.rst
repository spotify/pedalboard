Frequently Asked Questions
--------------------------

Can Pedalboard be used with live (real-time) audio?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Yes! As of Pedalboard v0.6.9, the `AudioStream <https://spotify.github.io/pedalboard/reference/pedalboard.io.html#pedalboard.io.AudioStream>`
class allows live processing of audio through a Pedalboard object without needing to worry about Python's
`Global Interpreter Lock <https://wiki.python.org/moin/GlobalInterpreterLock>`_.

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

Not yet! The underlying framework (JUCE) supports VST and AU instruments just fine, but Pedalboard itself would have to be modified to support instruments.

Can Pedalboard plugins accept MIDI?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Not yet, either - although the underlying framework (JUCE) supports passing MIDI to plugins, so this would also be possible to add.
