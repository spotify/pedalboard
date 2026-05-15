#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
uv export --extra test --no-dev --no-emit-project --no-hashes --frozen -o test-requirements.txt
