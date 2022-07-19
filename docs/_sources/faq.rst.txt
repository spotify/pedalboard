Frequently Asked Questions
--------------------------

Can Pedalboard be used with live (real-time) audio?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Technically, yes, Pedalboard could be used with live audio input/output. See:


* `Pull request #98 <https://github.com/spotify/pedalboard/pull/98>`_\ , which contains an experimental live audio interface written in C++.
* `@stefanobazzi <https://github.com/stefanobazzi>`_\ 's `guitarboard <https://github.com/stefanobazzi/guitarboard>`_ project for an example that uses the ``python-sounddevice`` library.

However, there are a couple big caveats when talking about using Pedalboard in a live context. Python, as a language, is `garbage-collected <https://devguide.python.org/garbage_collector/>`_\ , meaning that your code randomly pauses on a regular interval to clean up unused objects. In most programs, this is not an issue at all. However, for live audio, garbage collection can result in random pops, clicks, or audio drop-outs that are very difficult to prevent. Python's `Global Interpreter Lock <https://wiki.python.org/moin/GlobalInterpreterLock>`_ also adds potentially-unbounded delays when switching threads, and most operating systems use a separate high-priority thread for audio processing; meaning that Python could block this thread and cause stuttering if *any* Python objects are accessed or mutated in the audio thread.

Note that if your application processes audio in a streaming fashion, but allows for large buffer sizes (multiple seconds of audio) or soft real-time requirements, Pedalboard can be used there without issue. Examples of this use case include streaming audio processing over the network, or processing data offline but chunk-by-chunk.

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
