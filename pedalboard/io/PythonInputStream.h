/*
 * pedalboard
 * Copyright 2021 Spotify AB
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

#include <mutex>
#include <optional>

namespace py = pybind11;

#include "../JuceHeader.h"

namespace Pedalboard {

bool isReadableFileLike(py::object fileLike) {
  return py::hasattr(fileLike, "read") && py::hasattr(fileLike, "seek") &&
         py::hasattr(fileLike, "tell") && py::hasattr(fileLike, "seekable");
}

/**
 * A juce::InputStream subclass that fetches its
 * data from a provided Python file-like object.
 */
class PythonInputStream : public juce::InputStream {
public:
  PythonInputStream(py::object fileLike) : fileLike(fileLike) {
    if (!isReadableFileLike(fileLike)) {
      throw py::type_error("Expected a file-like object (with read, seek, "
                           "seekable, and tell methods).");
    }
  }

  bool isSeekable() {
    try {
      return fileLike.attr("seekable")().cast<bool>();
    } catch (py::error_already_set e) {
      pythonErrorRaised = true;
      e.restore();
      return false;
    }
  }

  juce::int64 getTotalLength() {
    py::gil_scoped_acquire acquire;

    // TODO: Try reading a couple of Python properties that may contain the
    // total length: urllib3.response.HTTPResponse provides `length_remaining`,
    // for instance

    try {
      if (!fileLike.attr("seekable")().cast<bool>()) {
        return -1;
      }
    } catch (py::error_already_set e) {
      pythonErrorRaised = true;
      e.restore();
      return 0;
    }

    if (totalLength == -1) {
      try {
        juce::int64 pos = fileLike.attr("tell")().cast<juce::int64>();
        fileLike.attr("seek")(0, 2);
        totalLength = fileLike.attr("tell")().cast<juce::int64>();
        fileLike.attr("seek")(pos, 0);
      } catch (py::error_already_set e) {
        pythonErrorRaised = true;
        e.restore();
        return -1;
      }
    }

    return totalLength;
  }

  int read(void *buffer, int bytesToRead) {
    // The buffer should never be null, and a negative size is probably a
    // sign that something is broken!
    jassert(buffer != nullptr && bytesToRead >= 0);

    py::bytes result;
    {
      py::gil_scoped_acquire acquire;
      try {
        result = fileLike.attr("read")(bytesToRead).cast<py::bytes>();
      } catch (py::error_already_set e) {
        pythonErrorRaised = true;
        e.restore();
        return 0;
      }
    }

    std::string resultAsString = result;
    std::memcpy((char *)buffer, (char *)resultAsString.data(),
                resultAsString.size());

    int bytesCopied = resultAsString.size();

    lastReadWasSmallerThanExpected = bytesToRead > bytesCopied;
    return bytesCopied;
  }

  bool isExhausted() {
    if (lastReadWasSmallerThanExpected) {
      return true;
    }

    try {
      py::gil_scoped_acquire acquire;
      return fileLike.attr("tell")().cast<juce::int64>() == getTotalLength();
    } catch (py::error_already_set e) {
      pythonErrorRaised = true;
      e.restore();
      return true;
    }
  }

  juce::int64 getPosition() {
    try {
      py::gil_scoped_acquire acquire;
      return fileLike.attr("tell")().cast<juce::int64>();
    } catch (py::error_already_set e) {
      pythonErrorRaised = true;
      e.restore();
      return -1;
    }
  }

  bool setPosition(juce::int64 pos) {
    try {
      py::gil_scoped_acquire acquire;

      if (fileLike.attr("seekable")().cast<bool>()) {
        fileLike.attr("seek")(pos);
      }

      return fileLike.attr("tell")().cast<juce::int64>() == pos;
    } catch (py::error_already_set e) {
      pythonErrorRaised = true;
      e.restore();
      return false;
    }
  }

  void raisePendingPythonExceptions() {
    if (pythonErrorRaised) {
      pythonErrorRaised = false;
      py::gil_scoped_acquire acquire;
      throw py::error_already_set();
    }
  }

  py::object getFileLikeObject() { return fileLike; }

private:
  py::object fileLike;
  juce::int64 totalLength = -1;
  bool lastReadWasSmallerThanExpected = false;

  // the juce::InputStream interface is not exception-safe, so we have to
  // change the behaviour of this subclass if an exception gets thrown,
  // then throw the exception ourselves when we have the opportunity.
  bool pythonErrorRaised = false;
};
}; // namespace Pedalboard