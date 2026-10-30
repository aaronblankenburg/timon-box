#!/usr/bin/env bash
# Build doc/assembly.pdf from content.py and the images in fig/.
#   ./build.sh                    PDF only
#   ./build.sh --figures          re-render all figures from models/box.step first
#   ./build.sh --figures lid door re-render only these figures
set -euo pipefail
cd "$(dirname "$0")"
if [[ "${1:-}" == "--figures" ]]; then
  shift
  python3 figures.py "$@"
fi
python3 content.py
