#include "RubberbandPlugin.h"

// This is a static global mutex that is used to protect the call to FFTW's
// planning functions, which are called when a RubberBandStretcher is created.

std::mutex Pedalboard::rubberbandFFTMutex;