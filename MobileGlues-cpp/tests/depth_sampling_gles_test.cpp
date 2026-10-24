// SPDX-License-Identifier: LGPL-2.1-only
// Links the production texture and draw wrappers to a real EGL/GLES driver.
// Use tests/run-depth-gles.sh; ANGLE Vulkan/SwiftShader enforces depth completeness.
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include "gl/texture.h"
#include "gl/depth_sampling.h"
#include "gl/drawing.h"
#include "gl/mg.h"
#include "config/settings.h"
#include "egl/context.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <string>

UnorderedMap<GLuint, bool> program_map_is_sampler_buffer_emulated;
thread_local MGContext* g_current_ctx = nullptr;
extern "C" GLuint mg_driver_bound_buffer(GLenum) { return 0; }
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
extern "C" void set_gl_state_proxy_width(GLint) {}
extern "C" void set_gl_state_proxy_height(GLint) {}
extern "C" void set_gl_state_proxy_intformat(GLenum) {}
extern "C" void set_gl_state_current_tex_unit(GLuint unit) { gl_state->current_tex_unit = unit; }


static decltype(g_gles_func.glDrawArrays) real_draw;
static GLuint app_sampler, last_private_sampler;
static bool expect_private;
static int failures, checks;
static void check(bool okay, const char* label) {
    ++checks;
    if (!okay) { std::printf("FAIL: %s\n", label); ++failures; }
}
static void observed_draw(GLenum mode, GLint first, GLsizei count) {
    GLint active = 0, bound = 0;
    GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    GLES.glActiveTexture(GL_TEXTURE0);
    GLES.glGetIntegerv(GL_SAMPLER_BINDING, &bound);
    if (expect_private) {
        check(bound && (GLuint)bound != app_sampler, "depth uses a private sampler");
        last_private_sampler = bound;
        GLint value = 0;
        GLES.glGetSamplerParameteriv(bound, GL_TEXTURE_MIN_FILTER, &value);
        check(value == GL_NEAREST_MIPMAP_NEAREST, "private min filter is depth-legal");
        GLES.glGetSamplerParameteriv(bound, GL_TEXTURE_MAG_FILTER, &value);
        check(value == GL_NEAREST, "private mag filter is depth-legal");
        GLES.glGetSamplerParameteriv(bound, GL_TEXTURE_WRAP_S, &value);
        check(value == GL_CLAMP_TO_EDGE, "private sampler preserves wrap S");
        GLES.glGetSamplerParameteriv(bound, GL_TEXTURE_WRAP_T, &value);
        check(value == GL_MIRRORED_REPEAT, "private sampler preserves wrap T");
        GLES.glGetSamplerParameteriv(bound, GL_TEXTURE_COMPARE_FUNC, &value);
        check(value == GL_GREATER, "private sampler preserves compare function");
        GLfloat lod = 0;
        GLES.glGetSamplerParameterfv(bound, GL_TEXTURE_MIN_LOD, &lod);
        check(lod == -2.0f, "private sampler preserves min LOD");
        GLES.glGetSamplerParameterfv(bound, GL_TEXTURE_MAX_LOD, &lod);
        check(lod == 0.25f, "private sampler preserves max LOD");
        if (app_sampler) {
            GLES.glGetSamplerParameteriv(app_sampler, GL_TEXTURE_MIN_FILTER, &value);
            check(value == GL_NEAREST_MIPMAP_LINEAR, "shared application sampler is unchanged");
        }
    } else {
        check((GLuint)bound == app_sampler, "colour draw keeps original sampler");
        GLES.glActiveTexture(GL_TEXTURE1);
        GLES.glGetIntegerv(GL_SAMPLER_BINDING, &bound);
        check((GLuint)bound == app_sampler, "unused depth unit keeps original sampler");
    }
    GLES.glActiveTexture(active);
    real_draw(mode, first, count);
}

#define LOAD(lib, name) auto name = reinterpret_cast<decltype(&::name)>(dlsym(lib, #name)); \
    if (!name) { std::fprintf(stderr, "Missing %s\n", #name); return 1; }

int main() {
    const char* angle_dir = std::getenv("MG_GLES_TEST_ANGLE_DIR");
    std::string egl_path = angle_dir ? std::string(angle_dir) + "/libEGL.so" : "libEGL.so.1";
    std::string gl_path = angle_dir ? std::string(angle_dir) + "/libGLESv2.so" : "libGLESv2.so.2";
    void* egl = dlopen(egl_path.c_str(), RTLD_NOW);
    void* gl = dlopen(gl_path.c_str(), RTLD_NOW);
    if (!egl || !gl) return 1;
    LOAD(egl, eglGetPlatformDisplay); LOAD(egl, eglInitialize); LOAD(egl, eglChooseConfig);
    LOAD(egl, eglBindAPI); LOAD(egl, eglCreatePbufferSurface); LOAD(egl, eglCreateContext);
    LOAD(egl, eglMakeCurrent);
    EGLAttrib angle_attributes[] = {0x3203 /* EGL_PLATFORM_ANGLE_TYPE_ANGLE */,
            0x3450 /* EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE */,
            0x3209 /* EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE */,
            0x3487 /* EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE */, EGL_NONE};
    EGLDisplay display = angle_dir
            ? eglGetPlatformDisplay(0x3202 /* EGL_PLATFORM_ANGLE_ANGLE */, EGL_DEFAULT_DISPLAY, angle_attributes)
            : eglGetPlatformDisplay(0x31DD /* EGL_PLATFORM_SURFACELESS_MESA */, EGL_DEFAULT_DISPLAY, nullptr);
    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) return 1;
    EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
    EGLConfig config; EGLint count;
    if (!eglChooseConfig(display, attributes, &config, 1, &count) || !count) return 1;
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint surface_attributes[] = {EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE};
    EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attributes);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if (!eglMakeCurrent(display, surface, surface, context)) return 1;

    LOAD(gl, glGetString); LOAD(gl, glGenTextures); LOAD(gl, glBindTexture); LOAD(gl, glTexStorage2D);
    LOAD(gl, glTexParameteri); LOAD(gl, glGenFramebuffers); LOAD(gl, glBindFramebuffer);
    LOAD(gl, glFramebufferTexture2D); LOAD(gl, glDrawBuffers); LOAD(gl, glReadBuffer);
    LOAD(gl, glClearDepthf); LOAD(gl, glClear); LOAD(gl, glGenSamplers); LOAD(gl, glSamplerParameteri);
    LOAD(gl, glBindSampler); LOAD(gl, glCreateShader); LOAD(gl, glShaderSource); LOAD(gl, glCompileShader);
    LOAD(gl, glGetShaderiv); LOAD(gl, glGetShaderInfoLog); LOAD(gl, glCreateProgram); LOAD(gl, glAttachShader);
    LOAD(gl, glLinkProgram); LOAD(gl, glGetProgramiv); LOAD(gl, glUseProgram); LOAD(gl, glViewport);
    LOAD(gl, glDrawArrays); LOAD(gl, glReadPixels); LOAD(gl, glGetError); LOAD(gl, glCheckFramebufferStatus); LOAD(gl, glUniform1iv);
    LOAD(gl, glDetachShader); LOAD(gl, glGetAttachedShaders); LOAD(gl, glDeleteShader);
    std::printf("GLES renderer: %s; %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    GLint major_gl = 0, minor_gl = 0;
    auto driver_get_int = reinterpret_cast<decltype(&::glGetIntegerv)>(dlsym(gl, "glGetIntegerv"));
    driver_get_int(GL_MAJOR_VERSION, &major_gl); driver_get_int(GL_MINOR_VERSION, &minor_gl);
    test_hardware.es_version = major_gl * 100 + minor_gl * 10;
    mg_texture_bind_context(1, 1);
#define DRIVER(name) GLES.name = reinterpret_cast<decltype(GLES.name)>(dlsym(gl, #name))
    DRIVER(glGetError); DRIVER(glGetIntegerv); DRIVER(glActiveTexture);
    DRIVER(glBindTexture); DRIVER(glTexStorage2D); DRIVER(glTexParameteri);
    DRIVER(glGenSamplers); DRIVER(glDeleteSamplers); DRIVER(glIsSampler);
    DRIVER(glSamplerParameteri); DRIVER(glSamplerParameterf); DRIVER(glBindSampler);
    DRIVER(glGetStringi); DRIVER(glGetActiveUniform); DRIVER(glGetUniformiv); DRIVER(glGetIntegeri_v);
    DRIVER(glGetSamplerParameteriv); DRIVER(glGetSamplerParameterfv);
    DRIVER(glGetTexLevelParameteriv); DRIVER(glGetTexParameterfv); DRIVER(glSamplerParameterfv);
    DRIVER(glTexParameterf); DRIVER(glTexImage2D); DRIVER(glGetTexParameteriv); DRIVER(glPixelStorei);
    DRIVER(glDrawArrays); DRIVER(glGetUniformLocation); DRIVER(glGetProgramiv);
    real_draw = GLES.glDrawArrays;
    GLES.glDrawArrays = observed_draw;
    GLuint textures[2], framebuffers[2], sampler;
    glGenTextures(2, textures); glGenFramebuffers(2, framebuffers); glGenSamplers(1, &sampler); app_sampler = sampler;
    ::glBindTexture(GL_TEXTURE_2D, textures[0]);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    ::glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 4, 4, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   GL_NEAREST_MIPMAP_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[0], 0);
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none); glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 1;
    glClearDepthf(0.75f); glClear(GL_DEPTH_BUFFER_BIT);
    ::glBindTexture(GL_TEXTURE_2D, textures[1]);
    ::glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1], 0);
    GLenum colour = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &colour); glReadBuffer(colour);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 1;
    ::glBindTexture(GL_TEXTURE_2D, textures[0]);
    ::glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    ::glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ::glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    ::glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
    GLES.glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, -2.0f);
    GLES.glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, 0.25f);
    const char* vertex = "#version 300 es\nvoid main(){const vec2 p[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],0,1);}";
    const char* fragment = "#version 300 es\nprecision highp float;uniform highp sampler2D depthTex;out vec4 colour;void main(){float d=texture(depthTex,vec2(0.5)).r;colour=vec4(d,d,d,1);}";
    const char* sources[] = {vertex, fragment};
    GLuint program = glCreateProgram();
    for (int i = 0; i < 2; ++i) {
        GLuint shader = glCreateShader(i ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
        glShaderSource(shader, 1, &sources[i], nullptr); glCompileShader(shader);
        GLint success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) { char log[4096]; glGetShaderInfoLog(shader, sizeof(log), nullptr, log); std::puts(log); return 1; }
        glAttachShader(program, shader);
    }
    glLinkProgram(program); GLint linked; glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) return 1;
    glUseProgram(program); gl_state->current_program = program; glViewport(0, 0, 4, 4);
    unsigned char pixels[2][4];
    for (int pass = 0; pass < 2; ++pass) {
        ::glBindSampler(0, sampler);
        if (pass == 0) { glDrawArrays(GL_TRIANGLES, 0, 3); }
        else { expect_private = true; ::glDrawArrays(GL_TRIANGLES, 0, 3); }
        glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels[pass]);
        GLint restored_sampler = 0;
        GLES.glGetIntegerv(GL_SAMPLER_BINDING, &restored_sampler);
        if ((GLuint)restored_sampler != sampler) { std::puts("Sampler binding was not restored"); return 1; }
        GLenum draw_error = glGetError();
        if (draw_error) { std::printf("Depth draw %d GL error: %u\n", pass, draw_error); return 1; }
    }
    std::printf("Raw depth 0.75: incomplete pairing sampled %u; corrected pairing sampled %u (expected 191/192).\n",
                pixels[0][0], pixels[1][0]);
    check(last_private_sampler && !GLES.glIsSampler(last_private_sampler), "private sampler is deleted after draw");
    GLuint colour_texture;
    glGenTextures(1, &colour_texture);
    ::glActiveTexture(GL_TEXTURE1);
    ::glBindTexture(GL_TEXTURE_2D, textures[0]);
    ::glBindSampler(1, sampler);
    ::glActiveTexture(GL_TEXTURE0);
    ::glBindTexture(GL_TEXTURE_2D, colour_texture);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
    unsigned char source[4*4*4];
    for (auto& channel : source) channel = 204;
    ::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, source);
    ::glBindSampler(0, sampler);
    unsigned char colour_pixel[4];
    expect_private = false;
    ::glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, colour_pixel);
    std::printf("Colour texture sharing depth sampler: %u (expected 204).\n", colour_pixel[0]);
    if (std::abs(int(colour_pixel[0]) - 204) > 1) return 1;
    // No sampler object: copy the effective texture parameters for this draw.
    ::glBindTexture(GL_TEXTURE_2D, textures[0]);
    ::glBindSampler(0, 0);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
    GLES.glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -2.0f);
    GLES.glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0.25f);
    // The test uses only ordinary texture/sampler wrappers for calls under test.
    app_sampler = 0; expect_private = true;
    ::glActiveTexture(GL_TEXTURE3);
    ::glDrawArrays(GL_TRIANGLES, 0, 3);
    GLint active_after = 0;
    GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &active_after);
    check(active_after == GL_TEXTURE3, "draw restores active texture unit");
    ::glActiveTexture(GL_TEXTURE0);
    GLint binding_after = -1, min_after = 0;
    GLES.glGetIntegerv(GL_SAMPLER_BINDING, &binding_after);
    GLES.glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_after);
    check(binding_after == 0, "texture-only draw restores sampler zero");
    check(min_after == GL_NEAREST_MIPMAP_LINEAR, "texture parameters are unchanged");
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, colour_pixel);
    check(std::abs(int(colour_pixel[0]) - 191) <= 1, "texture-only raw depth read is correct");
    // A real sampler array, with two depth images and changing unit assignments.
    auto build_program = [&](const char* fragment_source, GLuint existing = 0) {
        GLuint result = existing ? existing : glCreateProgram();
        if (existing) {
            GLuint attached[8]; GLsizei attached_count = 0;
            glGetAttachedShaders(result, 8, &attached_count, attached);
            for (int i = 0; i < attached_count; ++i) {
                glDetachShader(result, attached[i]); glDeleteShader(attached[i]);
            }
        }
        const char* shader_sources[] = {vertex, fragment_source};
        for (int stage = 0; stage < 2; ++stage) {
            GLuint shader = glCreateShader(stage ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
            glShaderSource(shader, 1, &shader_sources[stage], nullptr); glCompileShader(shader);
            GLint status = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
            check(status, "additional test shader compiles");
            glAttachShader(result, shader);
        }
        // The production glLinkProgram hook invokes this invalidation. Shader
        // compilation itself is deliberately the driver's job in this test.
        mg_depth_forget_program(result);
        glLinkProgram(result);
        GLint status = 0; glGetProgramiv(result, GL_LINK_STATUS, &status);
        check(status, "additional test program links");
        glUseProgram(result); gl_state->current_program = result;
        return result;
    };
    const char* array_fragment = "#version 300 es\nprecision highp float;uniform highp sampler2D depths[2];out vec4 colour;void main(){float d=0.25*texture(depths[0],vec2(0.5)).r+0.75*texture(depths[1],vec2(0.5)).r;colour=vec4(d,d,d,1);}";
    GLuint array_program = build_program(array_fragment);
    ::glActiveTexture(GL_TEXTURE2); ::glBindTexture(GL_TEXTURE_2D, textures[0]); ::glBindSampler(2, sampler);
    GLuint depth_quarter; glGenTextures(1, &depth_quarter);
    ::glActiveTexture(GL_TEXTURE3); ::glBindTexture(GL_TEXTURE_2D, depth_quarter);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    ::glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 4, 4, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    ::glBindSampler(3, sampler);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_quarter, 0);
    glClearDepthf(0.25f); glClear(GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[1]);
    ::glActiveTexture(GL_TEXTURE0);
    app_sampler = sampler;
    GLES.glDrawArrays = [](GLenum mode, GLint first, GLsizei count) {
        GLint active = 0; GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        check(active == GL_TEXTURE0, "array draw restores active texture before drawing");
        for (int unit : {2, 3}) {
            GLES.glActiveTexture(GL_TEXTURE0 + unit);
            GLint bound = 0; GLES.glGetIntegerv(GL_SAMPLER_BINDING, &bound);
            check(bound && (GLuint)bound != app_sampler, "array depth unit uses a private sampler");
        }
        GLES.glActiveTexture(active);
        real_draw(mode, first, count);
    };
    GLint array_location = GLES.glGetUniformLocation(array_program, "depths[0]");
    GLint units[] = {2, 3}; glUniform1iv(array_location, 2, units);
    ::glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, colour_pixel);
    std::printf("Depth array first ordering: %u\n", colour_pixel[0]);
    check(std::abs(int(colour_pixel[0]) - 96) <= 1, "two depth layers sample correctly");
    GLint reversed[] = {3, 2}; glUniform1iv(array_location, 2, reversed);
    GLint observed_units[2] = {-1, -1};
    GLES.glGetUniformiv(array_program, array_location, &observed_units[0]);
    GLES.glGetUniformiv(array_program, GLES.glGetUniformLocation(array_program, "depths[1]"), &observed_units[1]);
    std::printf("Reversed sampler units: %d, %d\n", observed_units[0], observed_units[1]);
    ::glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, colour_pixel);
    std::printf("Depth array reversed ordering: %u\n", colour_pixel[0]);
    const unsigned char guarded_reversed = colour_pixel[0];
    check(observed_units[0] == 3 && observed_units[1] == 2, "updated sampler array units are visible");
    ::glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, colour_pixel);
    std::printf("Driver-only reversed ordering: %u\n", colour_pixel[0]);
    check(guarded_reversed == colour_pixel[0], "cached guard agrees with the legal direct-driver reference after remapping");
    const bool old_angle = angle_dir && std::strstr((const char*)glGetString(GL_VERSION), "5fa2e61da947");
    if (old_angle && std::abs(int(colour_pixel[0]) - 159) > 1) {
        std::puts("KNOWN DRIVER LIMITATION: this old ANGLE build keeps the previous sampler-array mapping, including without MobileGlues; Mesa validates the updated mapping.");
    } else {
        check(std::abs(int(colour_pixel[0]) - 159) <= 1, "updated array mapping gives the expected GPU result");
    }
    ::glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    for (int unit : {2, 3}) {
        ::glActiveTexture(GL_TEXTURE0 + unit);
        GLES.glGetIntegerv(GL_SAMPLER_BINDING, &binding_after);
        check((GLuint)binding_after == sampler, "array sampler binding is restored");
    }
    ::glActiveTexture(GL_TEXTURE0);

    // A shadow comparison sampler is already complete with linear filtering;
    // it must not be converted into a raw point-sampling copy.
    const char* shadow_fragment = "#version 300 es\nprecision highp float;uniform highp sampler2DShadow depthTex;out vec4 colour;void main(){float d=texture(depthTex,vec3(0.5,0.5,0.5));colour=vec4(d,d,d,1);}";
    build_program(shadow_fragment, array_program);
    ::glBindTexture(GL_TEXTURE_2D, textures[0]); ::glBindSampler(0, sampler);
    ::glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    ::glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
    GLES.glDrawArrays = [](GLenum mode, GLint first, GLsizei count) {
        GLint bound = 0; GLES.glGetIntegerv(GL_SAMPLER_BINDING, &bound);
        check((GLuint)bound == app_sampler, "shadow sampler keeps original binding");
        real_draw(mode, first, count);
    };
    ::glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, colour_pixel);
    check(colour_pixel[0] == 255, "linear shadow comparison still works after program relink");
    GLenum error = glGetError();
    std::printf("GL error: %u; %d checks, %d failures\n", error, checks, failures);
    return ((angle_dir && pixels[0][0] != 0) || std::abs(int(pixels[1][0]) - 191) > 1
            || error != GL_NO_ERROR || failures) ? 1 : 0;
}
