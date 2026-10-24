// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_DEPTH_SAMPLING_H
#define MOBILEGLUES_DEPTH_SAMPLING_H
#include <GL/gl.h>

// A raw depth sampler must use nearest filtering on ES. Only samplers used by
// the current program are considered. Private copies preserve the application's
// sampler/texture state and cannot change colour units sharing the same sampler.
class mg_depth_sampling_guard {
    struct Binding { GLuint unit, original, temporary; };
    Binding bindings[128];
    int count = 0;
public:
    mg_depth_sampling_guard();
    ~mg_depth_sampling_guard();
    mg_depth_sampling_guard(const mg_depth_sampling_guard&) = delete;
    mg_depth_sampling_guard& operator=(const mg_depth_sampling_guard&) = delete;
};

// Program names and uniform locations can be recycled or changed by relinking.
void mg_depth_forget_program(GLuint program);
#endif
