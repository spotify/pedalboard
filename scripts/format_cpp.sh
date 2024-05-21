#!/bin/bash
set +x
find pedalboard/ -name '*.cpp' -o -name '*.h' | xargs clang-format -i
