// SPDX-License-Identifier: LGPL-2.1-only
// Links the real texture/sampler/draw-guard code against a small driver model.
#include "gl/texture.h"
#include "gl/mg.h"
#include "config/settings.h"
#include "egl/context.h"
#include <array>
#include <cstdio>
#include <unordered_map>

gles_func_t g_gles_func{};
gles_caps_t g_gles_caps{};
global_settings_t global_settings{};
hardware_s test_hardware{320, false};
hardware_t hardware = &test_hardware;
gl_state_s g_default_gl_state{};
thread_local gl_state_t gl_state = &g_default_gl_state;
int __android_log_print(int, const char*, const char*, ...) { return 0; }
extern "C" void write_log(const char*, ...) {}
extern "C" const char* glEnumToString(GLenum) { return "enum"; }
extern "C" GLenum pname_convert(GLenum value) { return value; }
extern "C" GLenum map_tex_target(GLenum value) { return value; }
extern "C" void mg_set_gl_error(GLenum) {}
extern "C" void set_gl_state_current_tex_unit(GLuint unit) { gl_state->current_tex_unit = unit; }

struct Sampling {
    GLint min = GL_NEAREST_MIPMAP_LINEAR, mag = GL_LINEAR, compare = GL_NONE;
};
static int active;
static std::array<GLuint, 128> textures{}, samplers{};
static std::unordered_map<GLuint, Sampling> texture_state, sampler_state;
static GLuint next_sampler = 10;
static GLenum driver_error;
static int failures, checks;
static void eq(const char* message, long got, long wanted) {
    ++checks;
    if (got != wanted) { std::printf("FAIL %s: got %ld, expected %ld\n", message, got, wanted); ++failures; }
}
static void set(Sampling& state, GLenum pname, GLint value) {
    if (pname == GL_TEXTURE_MIN_FILTER) state.min = value;
    if (pname == GL_TEXTURE_MAG_FILTER) state.mag = value;
    if (pname == GL_TEXTURE_COMPARE_MODE) state.compare = value;
}
static GLenum get_error() { GLenum value = driver_error; driver_error = 0; return value; }
static void bind_sampler(GLuint unit, GLuint sampler) {
    if (unit >= samplers.size()) { driver_error = GL_INVALID_VALUE; return; }
    if (sampler && !sampler_state.count(sampler)) { driver_error = GL_INVALID_OPERATION; return; }
    samplers[unit] = sampler;
}
static void sampler_param(GLuint sampler, GLenum pname, GLint param) {
    auto it = sampler_state.find(sampler);
    if (it == sampler_state.end()) { driver_error = GL_INVALID_OPERATION; return; }
    set(it->second, pname, param);
}
static void bind_texture(GLenum, GLuint texture) { textures[active] = texture; texture_state.try_emplace(texture); }
static void texture_param(GLenum target, GLenum pname, GLint param) {
    if (target != GL_TEXTURE_2D) { driver_error = GL_INVALID_ENUM; return; }
    set(texture_state[textures[active]], pname, param);
}
static void gen_samplers(GLsizei count, GLuint* names) {
    for (int i = 0; i < count; ++i) { names[i] = next_sampler++; sampler_state[names[i]] = Sampling{}; }
}
static void delete_samplers(GLsizei count, const GLuint* names) {
    if (count < 0) { driver_error = GL_INVALID_VALUE; return; }
    for (int i = 0; i < count; ++i) {
        sampler_state.erase(names[i]);
        for (auto& bound : samplers) if (bound == names[i]) bound = 0;
    }
}
static void make_texture(GLuint unit, GLuint name, GLenum format) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, name);
    glTexStorage2D(GL_TEXTURE_2D, 1, format, 4, 4);
}

int main() {
    GLES.glGetError = get_error;
    GLES.glActiveTexture = [](GLenum unit) { active = unit - GL_TEXTURE0; };
    GLES.glGetIntegerv = [](GLenum pname, GLint* value) { *value = pname == GL_ACTIVE_TEXTURE ? GL_TEXTURE0 + active : 0; };
    GLES.glBindTexture = bind_texture;
    GLES.glTexStorage2D = [](GLenum, GLsizei, GLenum, GLsizei, GLsizei) {};
    GLES.glTexParameteri = texture_param;
    GLES.glTexParameteriv = [](GLenum target, GLenum pname, const GLint* param) { texture_param(target, pname, *param); };
    GLES.glGetTexParameteriv = [](GLenum, GLenum, GLint* value) { *value = -1; };
    GLES.glBindSampler = bind_sampler;
    GLES.glIsSampler = [](GLuint sampler) -> GLboolean { return sampler_state.count(sampler) ? GL_TRUE : GL_FALSE; };
    GLES.glSamplerParameteri = sampler_param;
    GLES.glGenSamplers = gen_samplers;
    GLES.glDeleteSamplers = delete_samplers;
    mg_texture_bind_context(1, 1);

    make_texture(0, 1, GL_DEPTH_COMPONENT32F);
    eq("depth defaults get legal min", texture_state[1].min, GL_NEAREST_MIPMAP_NEAREST);
    eq("depth defaults get legal mag", texture_state[1].mag, GL_NEAREST);
    GLint requested = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &requested);
    eq("app reads requested default min", requested, GL_NEAREST_MIPMAP_LINEAR);
    GLuint sampler;
    glGenSamplers(1, &sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindSampler(0, sampler);
    make_texture(1, 2, GL_RGBA8);
    glBindSampler(1, sampler);
    {
        mg_depth_draw_guard guard;
        eq("bad depth sampler removed for draw", samplers[0], 0);
        eq("shared colour sampler preserved", samplers[1], sampler);
        eq("shared sampler filtering unchanged", sampler_state[sampler].min, GL_NEAREST_MIPMAP_LINEAR);
        { mg_depth_draw_guard nested; }
        eq("nested guard must not restore outer pairing early", samplers[0], 0);
    }
    eq("depth sampler restored after draw", samplers[0], sampler);

    make_texture(0, 1, GL_DEPTH_COMPONENT32F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    make_texture(3, 3, GL_DEPTH_COMPONENT32F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glBindSampler(3, sampler);
    {
        mg_depth_draw_guard guard;
        eq("first compare override applied", texture_state[1].compare, GL_NONE);
        eq("original active unit compare override applied", texture_state[3].compare, GL_NONE);
        eq("active unit restored during draw", active, 3);
    }
    eq("first compare mode restored", texture_state[1].compare, GL_COMPARE_REF_TO_TEXTURE);
    eq("second compare mode restored", texture_state[3].compare, GL_COMPARE_REF_TO_TEXTURE);
    eq("active unit restored after draw", active, 3);

    glBindSampler(0, 9999); // Driver rejects the unknown name; existing binding must survive.
    driver_error = 0;
    { mg_depth_draw_guard guard; }
    eq("rejected bind does not poison tracked binding", samplers[0], sampler);
    eq("no GL errors from restoring rejected binding", get_error(), GL_NO_ERROR);
    glSamplerParameteri(~0u, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    eq("unknown sampler stays a driver error without allocating shadow state", get_error(), GL_INVALID_OPERATION);
    eq("invalid texture target has no tracked object", mgGetTexObjectByTarget(0xDEAD) == nullptr, true);
    glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    {
        mg_depth_draw_guard guard;
        eq("comparison sampler remains bound for PCF", samplers[0], sampler);
        eq("comparison sampler linear filtering is preserved", sampler_state[sampler].mag, GL_LINEAR);
    }
    std::printf("depth sampling state: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
