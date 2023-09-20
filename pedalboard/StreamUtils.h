#pragma once
#include <iostream>

/**
 * A tiny RAII helper class that suppresses all output to the provided ostream.
 * Useful if calling into a third-party library that logs to std::cerr whose
 * logs you want to suppress/ignore.
 */
class SuppressOutput {
public:
  SuppressOutput(std::ostream &ostream)
      : ostream(ostream), previousState(ostream.rdstate()) {
    ostream.setstate(std::ios_base::failbit);
  }

  ~SuppressOutput() { ostream.clear(previousState); }

private:
  std::ostream &ostream;
  std::ios_base::iostate previousState;
};