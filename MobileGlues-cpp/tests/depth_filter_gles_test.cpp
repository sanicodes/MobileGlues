// SPDX-License-Identifier: LGPL-2.1-only
// Optional Linux surfaceless EGL/GLES check. No Minecraft assets required.
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include "gl/depth_filter.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <string>

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
    LOAD(gl, glDrawArrays); LOAD(gl, glReadPixels); LOAD(gl, glGetError); LOAD(gl, glCheckFramebufferStatus);
    std::printf("GLES renderer: %s\n", glGetString(GL_RENDERER));
    GLuint textures[2], framebuffers[2], sampler;
    glGenTextures(2, textures); glGenFramebuffers(2, framebuffers); glGenSamplers(1, &sampler);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, 4, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   mg_depth_filter_make_legal(GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[0], 0);
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none); glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 1;
    glClearDepthf(0.75f); glClear(GL_DEPTH_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1], 0);
    GLenum colour = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &colour); glReadBuffer(colour);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 1;
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
    glUseProgram(program); glViewport(0, 0, 4, 4);
    unsigned char pixels[2][4];
    for (int pass = 0; pass < 2; ++pass) {
        glBindSampler(0, pass == 0 ? sampler : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels[pass]);
    }
    std::printf("Raw depth 0.75: incomplete pairing sampled %u; corrected pairing sampled %u (expected 191/192).\n",
                pixels[0][0], pixels[1][0]);
    GLenum error = glGetError();
    std::printf("GL error: %u\n", error);
    return ((angle_dir && pixels[0][0] != 0) || std::abs(int(pixels[1][0]) - 191) > 1
            || error != GL_NO_ERROR) ? 1 : 0;
}
