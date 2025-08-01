"""Utilities for querying PortAudio devices."""

try:
    import sounddevice as sd  # 可能找不到 PortAudio
except Exception:
    sd = None  # 無聲卡環境 fail-open


def is_device_alive(name: str) -> bool:
    """True ↔ 裝置仍存在；PortAudio 不可用時保守回 True。"""
    if sd is None:
        return True
    try:
        return any(d["name"] == name for d in sd.query_devices())
    except Exception:
        return True
