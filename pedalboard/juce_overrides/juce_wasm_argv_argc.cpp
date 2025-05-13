#ifdef JUCE_WASM
#include "../JUCE/modules/juce_core/juce_core.h"

namespace juce {
const char *const *juce_argv = nullptr;
int juce_argc = 0;

File File::getSpecialLocation(const SpecialLocationType type) { return {}; }
} // namespace juce
#endif