#!/usr/bin/env python3
"""Run issue #372 audio reads in isolated Python processes."""
import argparse
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import time

IMPORTS = {
    "audio_only": "from pedalboard.io import AudioFile",
    "qt_first": "from PyQt6.QtCore import QCoreApplication, pyqtSignal, QObject\nfrom pedalboard.io import AudioFile",
    "pedalboard_first": "import pedalboard\nfrom PyQt6.QtCore import QCoreApplication, pyqtSignal, QObject\nfrom pedalboard.io import AudioFile",
}


def child_code(imports):
    return """import sys, json, importlib.metadata
print('starting imports', flush=True)
""" + imports + """
print('imports completed', flush=True)
versions = {}
for package in ('pedalboard', 'PyQt6', 'PyQt6-Qt6', 'numpy'):
    try:
        versions[package] = importlib.metadata.version(package)
    except importlib.metadata.PackageNotFoundError:
        versions[package] = None
print(json.dumps({'versions': versions}), flush=True)
print('opening audio', flush=True)
with AudioFile(sys.argv[1], 'r') as audio:
    print('reading audio', flush=True)
    samples = audio.read(44100)
    print(json.dumps({'shape': list(samples.shape), 'samplerate': audio.samplerate}), flush=True)
print('audio closed', flush=True)
"""


def as_text(value):
    return value.decode(errors='replace') if isinstance(value, bytes) else (value or '')


def run_case(name, command, timeout, cwd=None):
    started = time.monotonic()
    environment = dict(os.environ, PYTHONFAULTHANDLER='1', PYTHONUNBUFFERED='1')
    # Prevent the caller's checkout/PYTHONPATH from shadowing installed wheels.
    environment.pop('PYTHONPATH', None)
    result = {'name': name, 'command': command, 'cwd': str(cwd) if cwd else None}
    try:
        process = subprocess.run(command, cwd=cwd, env=environment, capture_output=True,
                                 text=True, timeout=timeout)
        result.update(returncode=process.returncode, stdout=process.stdout, stderr=process.stderr,
                      status='passed' if process.returncode == 0 else 'failed')
        if process.returncode < 0 and os.name != 'nt':
            try:
                result['signal'] = signal.Signals(-process.returncode).name
            except ValueError:
                result['signal'] = str(-process.returncode)
        if process.returncode:
            result['exit_code_hex'] = hex(process.returncode & 0xffffffff)
    except subprocess.TimeoutExpired as error:
        result.update(returncode=None, stdout=as_text(error.stdout), stderr=as_text(error.stderr),
                      status='timeout')
    result['elapsed_seconds'] = round(time.monotonic() - started, 3)
    print(json.dumps(result, indent=2), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flac', type=Path, help='Path to the supplied PN.flac')
    parser.add_argument('--original', type=Path, help='Run the unmodified pbcrash.py in its directory')
    parser.add_argument('--output', type=Path, required=True, help='JSON result path')
    parser.add_argument('--timeout', type=float, default=30)
    args = parser.parse_args()
    flac = args.flac.resolve(strict=True)
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    cases = [run_case(name, [sys.executable, '-I', '-X', 'faulthandler', '-u', '-c',
                             child_code(imports), str(flac)], args.timeout, flac.parent)
             for name, imports in IMPORTS.items()]
    if args.original:
        original = args.original.resolve(strict=True)
        cases.append(run_case('original', [sys.executable, '-I', '-X', 'faulthandler', '-u',
                                          str(original)], args.timeout, original.parent))
    report = {'python': sys.version, 'executable': sys.executable,
              'platform': platform.platform(), 'machine': platform.machine(),
              'flac': str(flac), 'cases': cases}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    return 0 if all(case['status'] == 'passed' for case in cases) else 1


if __name__ == '__main__':
    raise SystemExit(main())
