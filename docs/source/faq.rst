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

Can Pedalboard objects be pickled, copied, or sent to other processes?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

As of version 0.9.27, yes! All of Pedalboard's built-in plugins, as well as :class:`pedalboard.Pedalboard`,
:class:`pedalboard.Chain`, and :class:`pedalboard.Mix` objects, support :py:mod:`pickle` and :py:mod:`copy`.
This allows plugins to be passed to other processes (i.e.: with :py:mod:`multiprocessing`,
:py:mod:`concurrent.futures`, a PyTorch ``DataLoader`` with ``num_workers > 0``, Ray, or Dask),
or to be saved alongside a dataset to record exactly which effects were applied:

.. code-block:: python

   import pickle
   from concurrent.futures import ProcessPoolExecutor
   from pedalboard import Pedalboard, Compressor, Reverb

   board = Pedalboard([Compressor(threshold_db=-20, ratio=4), Reverb(room_size=0.8)])

   # Save and restore a board, including all of its parameters:
   restored = pickle.loads(pickle.dumps(board))

   # ...or send it to a pool of worker processes:
   with ProcessPoolExecutor() as executor:
       outputs = list(executor.map(board, audio_clips, [sample_rate] * len(audio_clips)))

A few things to keep in mind:

- Only a plugin's parameters are pickled, not its internal state. An unpickled plugin
  behaves as if it had just been reset (i.e.: a :class:`pedalboard.Reverb` will not
  include the tail of any audio processed before it was pickled).
- A :class:`pedalboard.Convolution` loaded from a file stores the path to that file, which must also
  exist wherever the plugin is unpickled. (Impulse responses passed as NumPy arrays are pickled by value.)
- A :class:`pedalboard.VST3Plugin` or :class:`pedalboard.AudioUnitPlugin` stores its path and its
  :py:attr:`raw_state`, and is reloaded from that path when unpickled, so the same plugin must be
  installed at the same location on the machine that unpickles it.
- As with any pickled data, only unpickle data from sources you trust. Unpickling an external plugin
  loads (and runs) the plugin found at the stored path.

Note that Pedalboard releases the GIL while processing audio, so threads are often enough to use
multiple CPU cores within a single process.
