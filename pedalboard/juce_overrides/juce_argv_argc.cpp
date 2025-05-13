#ifdef JUCE_WASM
namespace juce {
const char *const *juce_argv = nullptr;
int juce_argc = 0;

File File::getSpecialLocation(const SpecialLocationType type) { return {}; }
} // namespace juce
#endif