"""
Add reverb to an audio file using Pedalboard.

The audio file is read in chunks, using nearly no memory.
This should be one of the fastest possible ways to add reverb to a file.
On my laptop, this runs about 32x faster than real-time (i.e.: processes a 60-second file in <2 seconds.)

Requirements: `pip install pysoundfile tqdm pedalboard`
"""

import sys
import warnings
import argparse
import numpy as np
from tqdm import tqdm
import soundfile as sf
from tqdm.std import TqdmWarning
from pedalboard import Reverb


BUFFER_SIZE_SAMPLES = 1024
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
            "The name of the output file to write to. If not provided, {input_file}.reverb.wav will"
            " be used."
        ),
        default=None,
    )

    # Instantiate the Reverb object early so we can read its defaults for the argparser --help:
    reverb = Reverb()

    parser.add_argument("--room-size", type=float, default=reverb.room_size)
    parser.add_argument("--damping", type=float, default=reverb.damping)
    parser.add_argument("--wet-level", type=float, default=reverb.wet_level)
    parser.add_argument("--dry-level", type=float, default=reverb.dry_level)
    parser.add_argument("--width", type=float, default=reverb.width)
    parser.add_argument("--freeze-mode", type=float, default=reverb.freeze_mode)

    parser.add_argument(
        "--cut-reverb-tail",
        action="store_true",
        help=(
            "If passed, remove the reverb tail to the end of the file. "
            "The output file will be identical in length to the input file."
        ),
    )
    args = parser.parse_args()

    for arg in ('room_size', 'damping', 'wet_level', 'dry_level', 'width', 'freeze_mode'):
        setattr(reverb, arg, getattr(args, arg))

    if not args.output_file:
        args.output_file = args.input_file + ".reverb.wav"

    sys.stderr.write(f"Opening {args.input_file}...\n")

    with sf.SoundFile(args.input_file) as input_file:
        sys.stderr.write(f"Writing to {args.output_file}...\n")
        with sf.SoundFile(
            args.output_file,
            'w',
            samplerate=input_file.samplerate,
            channels=input_file.channels,
        ) as output_file:
            length = get_num_frames(input_file)
            length_seconds = length / input_file.samplerate
            sys.stderr.write(f"Adding reverb to {length_seconds:.2f} seconds of audio...\n")
            with tqdm(
                total=length_seconds,
                desc="Adding reverb...",
                bar_format=(
                    "{percentage:.0f}%|{bar}| {n:.2f}/{total:.2f} seconds processed"
                    " [{elapsed}<{remaining}, {rate:.2f}x]"
                ),
                # Avoid a formatting error that occurs if
                # TQDM tries to print before we've processed a block
                delay=1000,
            ) as t:
                for dry_chunk in input_file.blocks(BUFFER_SIZE_SAMPLES, frames=length):
                    # Actually call Pedalboard here:
                    # (reset=False is necessary to allow the reverb tail to
                    # continue from one chunk to the next.)
                    effected_chunk = reverb.process(
                        dry_chunk, sample_rate=input_file.samplerate, reset=False
                    )
                    # print(effected_chunk.shape, np.amax(np.abs(effected_chunk)))
                    output_file.write(effected_chunk)
                    t.update(len(dry_chunk) / input_file.samplerate)
                    t.refresh()
            if not args.cut_reverb_tail:
                while True:
                    # Pull audio from the effect until there's nothing left:
                    effected_chunk = reverb.process(
                        np.zeros((BUFFER_SIZE_SAMPLES, input_file.channels), np.float32),
                        sample_rate=input_file.samplerate,
                        reset=False,
                    )
                    if np.amax(np.abs(effected_chunk)) < NOISE_FLOOR:
                        break
                    output_file.write(effected_chunk)
    sys.stderr.write("Done!\n")


if __name__ == "__main__":
    main()
