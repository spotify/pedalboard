"""
Add reverb to an audio file using Pedalboard.

The audio file is read in chunks, using nearly no memory.
This should be one of the fastest possible ways to add reverb to a file
while also using as little memory as possible.

On my laptop, this runs about 58x faster than real-time
(i.e.: processes a 60-second file in ~1 second.)

Requirements: `pip install PySoundFile tqdm pedalboard`
Note that PySoundFile requires a working libsndfile installation.
"""

import argparse
import os
import sys
import warnings

import numpy as np
import soundfile as sf
from tqdm import tqdm
from tqdm.std import TqdmWarning

from pedalboard import PitchShift

BUFFER_SIZE_SAMPLES = 1024 * 16
NOISE_FLOOR = 1e-4


def get_num_frames(f: sf.SoundFile) -> int:
    # On some platforms and formats, f.frames == -1L.
    # Check for this bug and work around it:
    if f.frames > 2 ** 32:
        f.seek(0)
        last_position = f.tell()
        while True:
            # Seek through the file in chunks, returning
            # if the file pointer stops advancing.
            f.seek(1024 * 1024 * 1024, sf.SEEK_CUR)
            new_position = f.tell()
            if new_position == last_position:
                f.seek(0)
                return new_position
            else:
                last_position = new_position
    else:
        return f.frames


def main():
    warnings.filterwarnings("ignore", category=TqdmWarning)

    parser = argparse.ArgumentParser(description="Add reverb to an audio file.")
    parser.add_argument("input_file", help="The input file to add reverb to.")
    parser.add_argument(
        "--output-file",
        help=(
            "The name of the output file to write to. If not provided, {input_file}.shifted.wav will"
            " be used."
        ),
        default=None,
    )

    plugin = PitchShift()

    parser.add_argument("--pitch_scale", type=float, default=plugin.pitch_scale)
    parser.add_argument(
        "-y",
        "--overwrite",
        action="store_true",
        help="If passed, overwrite the output file if it already exists.",
    )

    args = parser.parse_args()

    for arg in ['pitch_scale']:
        setattr(plugin, arg, getattr(args, arg))

    if not args.output_file:
        args.output_file = args.input_file + ".shifted.wav"

    sys.stderr.write(f"Opening {args.input_file}...\n")

    with sf.SoundFile(args.input_file) as input_file:
        sys.stderr.write(f"Writing to {args.output_file}...\n")
        if os.path.isfile(args.output_file) and not args.overwrite:
            raise ValueError(
                f"Output file {args.output_file} already exists! (Pass -y to overwrite.)"
            )
        with sf.SoundFile(
            args.output_file,
            'w',
            samplerate=input_file.samplerate,
            channels=input_file.channels,
        ) as output_file:
            length = get_num_frames(input_file)
            length_seconds = length / input_file.samplerate
            sys.stderr.write(f"Adding shift to {length_seconds:.2f} seconds of audio...\n")
            with tqdm(
                total=length_seconds,
                desc="Adding shift...",
                bar_format=(
                    "{percentage:.0f}%|{bar}| {n:.2f}/{total:.2f} seconds processed"
                    " [{elapsed}<{remaining}, {rate:.2f}x]"
                ),
                # Avoid a formatting error that occurs if
                # TQDM tries to print before we've processed a block
                delay=1000,
            ) as t:
                for i, dry_chunk in enumerate(input_file.blocks(BUFFER_SIZE_SAMPLES, frames=length)):
                    print("Dry chunk", i)
                    effected_chunk = plugin.process(
                        dry_chunk, sample_rate=input_file.samplerate, reset=False
                    )
                    # print(effected_chunk.shape, np.amax(np.abs(effected_chunk)))
                    output_file.write(effected_chunk)
                    t.update(len(dry_chunk) / input_file.samplerate)
                    t.refresh()

    sys.stderr.write("Done!\n")


if __name__ == "__main__":
    main()
