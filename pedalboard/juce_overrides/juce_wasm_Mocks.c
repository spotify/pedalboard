#ifdef JUCE_WASM
size_t wcsftime(wchar_t *__restrict __maxlen, size_t __maxlen,
                const wchar_t *__restrict, const struct tm *__restrict) {
  return 0;
}
#endif