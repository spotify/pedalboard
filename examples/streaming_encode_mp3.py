"""
Use Pedalboard's AudioFile class to encode audio to MP3 as
a stream in near-real-time, including on-the-fly resampling.

The input audio will be read in chunks and passed to the MP3 encoder,
which will output frames of encoded MP3 as they are encoded using a
BytesIO object.

Use this as a starting point for streaming audio processing tasks
such as live audio encoding, real-time audio processing, or streaming
web services that require audio encoding.

Requirements: `pip install pedalboard`
"""

import argparse
from io import BytesIO

from pedalboard.io import AudioFile, StreamResampler


class BytesIOBuffer:
    """
    A wrapper around a BytesIO that allows a writer to write
    and a reader to read, treating the BytesIO as a queue.
    """

    def __init__(self):
        self.write_buf = BytesIO()
        self.pointer = 0

    def read(self, n=None):
        old_pos = self.write_buf.tell()
        self.write_buf.seek(self.pointer)
        chunk = self.write_buf.read(n)
        new_pos = self.write_buf.tell()
        self.write_buf.seek(old_pos)
        self.pointer = new_pos
        return chunk


def main():
    parser = argparse.ArgumentParser(description="Encode audio to MP3 as a stream.")
    parser.add_argument("input_file", help="The input audio file to encode.")
    parser.add_argument(
        "--output-file",
        help="The name of the output MP3 file to write to.",
        default="output.mp3",
    )
    parser.add_argument(
        "--quality",
        help="The quality setting to use for encoding (i.e.: '320' for 320kbps, 'v2' for V2 quality, etc.)",
        default="V2",
    )
    parser.add_argument(
        "--output-sample-rate",
        help="The sample rate to write the output MP3 file at. Defaults to the input file's sample rate.",
        default=None,
        type=float,
    )
    parser.add_argument(
        "--chunk-size",
        help="The number of samples to read from the input file at a time.",
        default=1024 * 1024,
        type=int,
    )
    args = parser.parse_args()

    buffer = BytesIOBuffer()
    stream_resampler = None

    with open(args.output_file, "wb") as o:
        # Open both the input and output files:
        with AudioFile(args.input_file) as f, AudioFile(
            buffer.write_buf,
            "w",
            args.output_sample_rate or f.samplerate,
            f.num_channels,
            format="mp3",
            quality=args.quality,
        ) as w:
            if f.samplerate != w.samplerate and stream_resampler is None:
                stream_resampler = StreamResampler(f.samplerate, w.samplerate, f.num_channels)
                print(f"Resampling from {f.samplerate} Hz to {w.samplerate} Hz...")

            while True:
                # Read a single chunk from the input file:
                chunk = f.read(args.chunk_size)

                # Exit if we've hit the end of the file:
                if chunk.shape[1] == 0:
                    break

                # Resample the chunk if necessary:
                if stream_resampler:
                    chunk = stream_resampler.process(chunk)
                w.write(chunk)

                # Get the encoded MP3 bytes from the buffer:
                encoded_bytes = buffer.read()
                print(
                    f"Encoded {len(encoded_bytes):,} MP3 bytes from {chunk.shape[1]:,} "
                    f"samples ({chunk.shape[1] / w.samplerate:.2f} seconds)."
                )

                # Write the encoded MP3 bytes to the output file:
                o.write(encoded_bytes)

            if stream_resampler is not None:
                # Flush the stream resampler to ensure all samples are processed:
                w.write(stream_resampler.process(None))
                encoded_bytes = buffer.read()
                print(f"Encoded {len(encoded_bytes):,} MP3 bytes to flush resampler.")
                o.write(encoded_bytes)

        # Capture any final MP3 frames written when the file was closed:
        encoded_bytes = buffer.read()
        print(f"Encoded {len(encoded_bytes):,} MP3 bytes at file close.")
        o.write(encoded_bytes)

        print(f"Done! Wrote {o.tell():,} bytes to {args.output_file}.")


if __name__ == "__main__":
    main()
