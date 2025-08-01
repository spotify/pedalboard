try:
    import sounddevice as sd
except Exception:  # PortAudio 不在 → 匯入失敗
    sd = None  # 設為 None，後面走「fail-open」邏輯

_original_write = _AudioStream.write

def is_device_alive(name: str) -> bool:
    """
    若 `sounddevice` / PortAudio 不可用，就直接回 True，
    讓程式在無聲卡環境仍可運行與測試。
    """
    if sd is None:
        return True

    try:
        return any(d["name"] == name for d in sd.query_devices())
    except Exception:
        return True

_AudioStream.write = _patched_write
