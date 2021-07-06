#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "../JucePlugin.h"

namespace Pedalboard {
template <typename SampleType>
class Limiter : public JucePlugin<juce::dsp::Limiter<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Threshold, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Release, {});
};

inline void init_limiter(py::module &m) {
  py::class_<Limiter<float>, Plugin>(
      m, "Limiter",
      "A simple limiter with standard threshold and release time controls, "
      "featuring two compressors and a hard clipper at 0 dB.")
      .def(py::init([](float thresholdDb, float releaseMs) {
             auto plugin = new Limiter<float>();
             plugin->setThreshold(thresholdDb);
             plugin->setRelease(releaseMs);
             return plugin;
           }),
           py::arg("threshold_db") = -10.0, py::arg("release_ms") = 100.0)
      .def("__repr__",
           [](const Limiter<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Limiter";
             ss << " threshold_db=" << plugin.getThreshold();
             ss << " release_ms=" << plugin.getRelease();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("threshold_db", &Limiter<float>::getThreshold,
                    &Limiter<float>::setThreshold)
      .def_property("release_ms", &Limiter<float>::getRelease,
                    &Limiter<float>::setRelease);
}
}; // namespace Pedalboard