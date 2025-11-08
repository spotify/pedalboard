"""Unit tests for positional MIDI argument ordering in ``process`` calls."""

import numpy as np
import mido
from pedalboard import Pedalboard
from pedalboard_native._internal import MidiMonitor


def test_process_midi_before_buffer_size():
    """Ensure original positional order (MIDI before buffer_size) works."""
    sample_rate = 44100
    noise = np.zeros(sample_rate, dtype=np.float32)
    notes = [mido.Message("note_on", note=60, velocity=100, time=0.0)]
    monitor = MidiMonitor()
    
    # MIDI argument provided before buffer_size
    Pedalboard([monitor])(noise, sample_rate, notes, 128)
    assert monitor.get_last_event_count() == len(notes)


def test_process_midi_after_buffer_size():
    """Ensure optional order with MIDI after buffer_size works."""
    sample_rate = 44100
    noise = np.zeros(sample_rate, dtype=np.float32)
    notes = [mido.Message("note_on", note=60, velocity=100, time=0.0)]
    monitor = MidiMonitor()

    # MIDI argument provided after buffer_size
    Pedalboard([monitor])(noise, sample_rate, 128, notes)
    assert monitor.get_last_event_count() == len(notes)