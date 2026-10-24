#!/bin/sh
# Optional real-driver test, using the production texture and draw entry points.
# MG_GLES_TEST_ANGLE_DIR=/path/to/gles_angle VK_ICD_FILENAMES=/path/to/ICD.json sh tests/run-depth-gles.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
${CXX:-g++} -O2 -std=gnu++20 -w -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I. -Iinclude -I3rdparty/xxhash tests/depth_sampling_gles_test.cpp \
    gl/texture.cpp gl/depth_filter.cpp gl/gl_native.cpp gl/drawing.cpp gl/pixel.cpp gl/transfer.cpp \
    -ldl -o "$work/depth_sampling_gles_test"
"$work/depth_sampling_gles_test"
