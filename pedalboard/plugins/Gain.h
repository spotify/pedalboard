#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "../JucePlugin.h"

namespace Pedalboard {
template <typename SampleType>
class Gain : public JucePlugin<juce::dsp::Gain<SampleType>> {
  DEFINE_DSP_SETTER_AND_GETTER(SampleType, GainDecibels, {});
};

inline void init_gain(py::module &m) {
  py::class_<Gain<float>, Plugin>(
      m, "Gain",
      "Increase or decrease the volume of a signal by applying a gain value "
      "(in decibels). No distortion or other effects are applied.")
      .def(py::init([](float gaindB) {
             auto plugin = new Gain<float>();
             plugin->setGainDecibels(gaindB);
             return plugin;
           }),
           py::arg("gain_db") = 1.0)
      .def("__repr__",
           [](const Gain<float> &plugin) {
             std::ostringstream ss;
             ss << "<pedalboard.Gain";
             ss << " gain_db=" << plugin.getGainDecibels();
             ss << " at " << &plugin;
             ss << ">";
             return ss.str();
           })
      .def_property("gain_db", &Gain<float>::getGainDecibels,
                    &Gain<float>::setGainDecibels);
}
}; // namespace Pedalboard