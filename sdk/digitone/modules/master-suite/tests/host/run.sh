#!/bin/sh
# Builds and runs the DSP tests on the host (gcc, libm). Not part of the module build.
set -e
cd "$(dirname "$0")"
out="$(mktemp -d)"
gcc -O2 -Wall -Wextra -fsanitize=undefined -fno-sanitize-recover=undefined \
    ../../src/ms_dsp.c ../../src/ms_ui.c test_dsp.c -lm -o "$out/ms_test_dsp"
"$out/ms_test_dsp"
rm -rf "$out"
