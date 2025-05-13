#ifdef JUCE_WASM
size_t wcsftime(wchar_t *, size_t, const wchar_t *, const struct tm *) {
  return 0;
}
#endif