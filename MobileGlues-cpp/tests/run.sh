#!/bin/sh
# Host-side pixel, framebuffer and depth-sampling checks. The state test links
# the actual texture/sampler wrappers against a small driver model.
#
#   sh MobileGlues-cpp/tests/run.sh
set -e
cd "$(dirname "$0")/.."
INC="-I. -I./includes -I./include -I./3rdparty/xxhash"
CXX="${CXX:-g++} -std=gnu++20 -w $INC"
$CXX -o /tmp/mg_pixel_test  tests/pixel_size_test.cpp         gl/pixel.cpp
$CXX -o /tmp/mg_fb_test     tests/framebuffer_shuffle_test.cpp gl/framebuffer.cpp
$CXX -o /tmp/mg_depth_test  tests/depth_filter_test.cpp       gl/depth_filter.cpp
$CXX -ffunction-sections -fdata-sections -Wl,--gc-sections -o /tmp/mg_depth_state_test \
    tests/depth_sampling_state_test.cpp gl/texture.cpp gl/depth_filter.cpp gl/gl_native.cpp
/tmp/mg_pixel_test
echo
/tmp/mg_fb_test
echo
/tmp/mg_depth_test
/tmp/mg_depth_state_test
