"""Utilities for querying PortAudio devices."""

# PortAudio 可能根本沒裝（CI 或雲端機器）；因此包在 try/except。
try:
    import sounddevice as sd  # type: ignore
except Exception:  # PortAudio 不在 → 匯入失敗
    sd = None  # 讓下游程式 fail-open


def is_device_alive(name: str) -> bool:
    """
    回傳 True 代表 PortAudio 仍能找到指定 `name` 裝置。
    若 sounddevice/PortAudio 不可用，保守回 True 以避免在無聲卡環境崩潰。
    """
    if sd is None:
        return True

    try:
        return any(d["name"] == name for d in sd.query_devices())
    except Exception:
        return True
