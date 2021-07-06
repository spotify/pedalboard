#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "../JucePlugin.h"

namespace Pedalboard {
template <typename SampleType>
class NoiseGate : public JucePlugin<juce::dsp::NoiseGate<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Threshold, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Ratio, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Attack, {});
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, Release, {});
};

inline void init_noisegate(py::module &m) {

  py::class_<NoiseGate<float>, Plugin>(
      m, "NoiseGate",
      "A simple noise gate with standard threshold, ratio, attack time and "
      "release time controls. Can be used as an expander if the ratio is low.")
      .def(py::init([](float thresholddB, float ratio, float attackMs,
                       float releaseMs) {
             auto plugin = new NoiseGate<float>();
             plugin->getDSP().setThreshold(thresholddB);
             plugin->getDSP().setRatio(ratio);
             plugin->getDSP().setAttack(attackMs);
             plugin->getDSP().setRelease(releaseMs);
             return plugin;
           }),
           py::arg("threshold_db") = -100.0, py::arg("ratio") = 10,
           py::arg("attack_ms") = 1.0, py::arg("release_ms") = 100.0)
      .def("__repr__",
           [](const NoiseGate<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.NoiseGate";
             ss << " threshold_db=" << plugin.getThreshold();
             ss << " ratio=" << plugin.getRatio();
             ss << " attack_ms=" << plugin.getAttack();
             ss << " release_ms=" << plugin.getRelease();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("threshold_db", &NoiseGate<float>::getThreshold,
                    &NoiseGate<float>::setThreshold)
      .def_property("ratio", &NoiseGate<float>::getRatio,
                    &NoiseGate<float>::setRatio)
      .def_property("attack_ms", &NoiseGate<float>::getAttack,
                    &NoiseGate<float>::setAttack)
      .def_property("release_ms", &NoiseGate<float>::getRelease,
                    &NoiseGate<float>::setRelease);
}
}; // namespace Pedalboard