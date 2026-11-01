#!/usr/bin/env bash
# Build the build guide from content.py and the images in fig/: doc/assembly.pdf (German) and doc/assembly-en.pdf.
#   ./build.sh                    both languages
#   ./build.sh --lang en          only one language (de or en)
#   ./build.sh --figures          re-render all figures from models/box.step first
#   ./build.sh --figures lid door re-render only these figures
# The English build lists every text that is still German (see i18n.py).
set -euo pipefail
cd "$(dirname "$0")"
langs=(de en)
if [[ "${1:-}" == "--lang" ]]; then langs=("$2"); shift 2; fi
if [[ "${1:-}" == "--figures" ]]; then
  shift
  python3 figures.py "$@"
fi
for l in "${langs[@]}"; do
  ASSEMBLY_LANG="$l" python3 content.py
done
