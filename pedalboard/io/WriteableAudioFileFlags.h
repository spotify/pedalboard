/*
 * pedalboard
 * Copyright 2026 Spotify AB
 *
 * Licensed under the GNU Public License, Version 3.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    https://www.gnu.org/licenses/gpl-3.0.html
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <variant>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace Pedalboard {

enum class WriteableAudioFileFlag {
  Mp3EnableBitReservoir,
};

using CodecOptionValue = std::variant<bool, float, int, std::string>;
using CodecOptionsMap = std::map<WriteableAudioFileFlag, CodecOptionValue>;

static inline const std::map<WriteableAudioFileFlag, std::string> FLAG_NAMES = {
    {WriteableAudioFileFlag::Mp3EnableBitReservoir, "Mp3EnableBitReservoir"},
};

static inline const std::map<std::string, std::set<WriteableAudioFileFlag>>
    FORMAT_SUPPORTED_FLAGS = {
        {"MP3", {WriteableAudioFileFlag::Mp3EnableBitReservoir}},
};

inline std::string flagName(WriteableAudioFileFlag flag) {
  auto it = FLAG_NAMES.find(flag);
  if (it != FLAG_NAMES.end())
    return it->second;
  return "<unknown flag>";
}

inline std::string supportedFlagNames(const std::string &formatName) {
  auto it = FORMAT_SUPPORTED_FLAGS.find(formatName);
  if (it == FORMAT_SUPPORTED_FLAGS.end() || it->second.empty())
    return "";

  std::ostringstream ss;
  bool first = true;
  for (const auto &f : it->second) {
    if (!first)
      ss << ", ";
    ss << flagName(f);
    first = false;
  }
  return ss.str();
}

/**
 * Expected value type for each flag, used for early validation before the
 * value reaches the encoder (where errors may be swallowed).
 */
enum class CodecOptionType { Bool, Float, Int, String };

static inline const std::map<WriteableAudioFileFlag, CodecOptionType>
    FLAG_EXPECTED_TYPES = {
        {WriteableAudioFileFlag::Mp3EnableBitReservoir, CodecOptionType::Bool},
};

inline std::string codecOptionTypeName(CodecOptionType t) {
  switch (t) {
  case CodecOptionType::Bool:
    return "bool";
  case CodecOptionType::Float:
    return "float";
  case CodecOptionType::Int:
    return "int";
  case CodecOptionType::String:
    return "str";
  }
  return "<unknown>";
}

inline bool valueMatchesExpectedType(const CodecOptionValue &value,
                                     CodecOptionType expected) {
  switch (expected) {
  case CodecOptionType::Bool:
    return std::holds_alternative<bool>(value);
  case CodecOptionType::Float:
    return std::holds_alternative<float>(value);
  case CodecOptionType::Int:
    return std::holds_alternative<int>(value);
  case CodecOptionType::String:
    return std::holds_alternative<std::string>(value);
  }
  return false;
}

/**
 * Validate that all flags in the options map are supported by the given
 * audio format and that their values have the correct types. Throws
 * std::domain_error if any flag is unsupported or has the wrong type.
 */
inline void validateCodecOptions(const CodecOptionsMap &options,
                                 const std::string &formatName) {
  if (options.empty())
    return;

  auto it = FORMAT_SUPPORTED_FLAGS.find(formatName);
  std::set<WriteableAudioFileFlag> supported;
  if (it != FORMAT_SUPPORTED_FLAGS.end()) {
    supported = it->second;
  }

  for (const auto &[flag, value] : options) {
    if (supported.find(flag) == supported.end()) {
      std::ostringstream ss;
      ss << "The codec option " << flagName(flag) << " is not supported by the "
         << formatName << " encoder.";

      std::string supported_names = supportedFlagNames(formatName);
      if (supported_names.empty()) {
        ss << " The " << formatName
           << " encoder does not support any codec options.";
      } else {
        ss << " Supported options for " << formatName << ": " << supported_names
           << ".";
      }

      throw std::domain_error(ss.str());
    }

    auto typeIt = FLAG_EXPECTED_TYPES.find(flag);
    if (typeIt != FLAG_EXPECTED_TYPES.end() &&
        !valueMatchesExpectedType(value, typeIt->second)) {
      throw std::domain_error(flagName(flag) + " expects a " +
                              codecOptionTypeName(typeIt->second) + " value.");
    }
  }
}

inline void init_writeable_audio_file_flags(py::module &m) {
  py::enum_<WriteableAudioFileFlag>(m, "WriteableAudioFileFlag",
                                    R"(
An enumeration of codec-specific options that can be passed when opening an
audio file for writing. These flags are used as keys in the ``codec_options``
dictionary parameter accepted by :class:`WriteableAudioFile` and
:class:`AudioFile`.

Not all flags are supported by all codecs. Passing an unsupported flag for
the selected codec will raise a ``ValueError``.

.. note::
    These flags control low-level encoder behavior. Most users will not need
    to use them. The ``quality`` parameter is usually sufficient for
    controlling encoder output.
)")
      .value("Mp3EnableBitReservoir",
             WriteableAudioFileFlag::Mp3EnableBitReservoir,
             R"(
When writing MP3 files, controls whether the LAME encoder's bit reservoir
is enabled. The bit reservoir allows the encoder to use fewer bits on
simple frames and save them for complex frames, improving overall quality
at a given bitrate. Disabling it forces each frame to be independently
decodable at the cost of slightly lower quality.

Set to ``False`` for streaming or seeking applications where individual
frames need to be independently decodable.

Accepts a ``bool`` value. Defaults to ``True`` (bit reservoir enabled).
)");
}

} // namespace Pedalboard
