/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/
// #define debugprintf printf
#define debugprintf                                                            \
  if (false)                                                                   \
  printf

#include "juce_SIMDGenericInterpolator.h"

namespace juce {

template <int k> struct LagrangeResampleHelper {
  static forcedinline void calc(float &a, float b) noexcept {
    a *= b * (1.0f / k);
  }
};

template <> struct LagrangeResampleHelper<0> {
  static forcedinline void calc(float &, float) noexcept {}
};

template <int k>
static float calcCoefficient(float input, float offset) noexcept {
  LagrangeResampleHelper<0 - k>::calc(input, -2.0f - offset);
  LagrangeResampleHelper<1 - k>::calc(input, -1.0f - offset);
  LagrangeResampleHelper<2 - k>::calc(input, 0.0f - offset);
  LagrangeResampleHelper<3 - k>::calc(input, 1.0f - offset);
  LagrangeResampleHelper<4 - k>::calc(input, 2.0f - offset);
  return input;
}

float SIMDInterpolators::LagrangeTraits::valueAtOffset(
    const float *inputs, const float offset) noexcept {
  return calcCoefficient<0>(inputs[((int)offset) - 4], offset) +
         calcCoefficient<1>(inputs[((int)offset) - 3], offset) +
         calcCoefficient<2>(inputs[((int)offset) - 2], offset) +
         calcCoefficient<3>(inputs[((int)offset) - 1], offset) +
         calcCoefficient<4>(inputs[((int)offset)], offset);
}

} // namespace juce
