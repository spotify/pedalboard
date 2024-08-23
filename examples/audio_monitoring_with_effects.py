from pedalboard import Pedalboard, Compressor, Gain, Reverb
from pedalboard.io import AudioStream

# Open up an audio stream:
stream = AudioStream(
  input_device_name=AudioStream.input_device_names[0],
  output_device_name=AudioStream.output_device_names[0],
  num_input_channels=2,
  num_output_channels=2,
  allow_feedback=True,
  buffer_size=128,
  sample_rate=44100,
) 

stream.plugins = Pedalboard([
  Reverb(wet_level=0.2),
  Gain(1.0),
  Compressor(),
])

stream.run()