"""Utilities for querying PortAudio devices."""

try:
    import sounddevice as sd  # Might not find PortAudio
except Exception:
    sd = None  # Fail-open in environments without a sound card


def is_device_alive(name: str) -> bool:
    """True: device still available; PortAudio fallback to True when device not available."""
    if sd is None:
        return True
    try:
        return any(d["name"] == name for d in sd.query_devices())
    except Exception:
        return True
