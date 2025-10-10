#!/bin/bash
find pedalboard/ -name '*.cpp' -o -name '*.h' | xargs clang-format -i
