// Links the actual translator; no GL context or Android device is required.
#include "gl/glsl/glsl_for_es.h"
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <fstream>
#include <iostream>
#include <sstream>

hardware_s test_hardware{};
hardware_t hardware = &test_hardware;
gles_caps_t g_gles_caps{};
extern "C" void write_log(const char*, ...) {}
int __android_log_print(int, const char*, const char*, ...) { return 0; }

std::string process_uniform_declarations(const std::string&);

static bool validate(const std::string& source, EShLanguage stage = EShLangFragment) {
    glslang::TShader shader(stage);
    const char* text = source.c_str();
    shader.setStrings(&text, 1);
    if (!shader.parse(GetDefaultResources(), 320, false, EShMsgDefault)) {
        std::cerr << shader.getInfoLog() << '\n' << source << '\n';
        return false;
    }
    return true;
}

static bool translate(const std::string& source, unsigned version = 320, GLenum stage = GL_FRAGMENT_SHADER) {
    int error = 0;
    auto converted = GLSLtoGLSLES_2(source.c_str(), stage, version, error);
    converted = process_uniform_declarations(converted);
    return error == 0 && !converted.empty() && validate(converted,
        stage == GL_VERTEX_SHADER ? EShLangVertex : EShLangFragment);
}

int main(int argc, char** argv) {
    glslang::InitializeProcess();
    g_gles_caps.GL_EXT_texture_query_lod = true;
    int failures = 0;
    const std::string uniform_use =
        "layout(std140) uniform TerrainUniform { ivec2 TextureSize; } _uniform;\n"
        "void main() { if (_uniform.TextureSize.x == 1) { return; } }\n";
    if (process_uniform_declarations(uniform_use) != uniform_use) {
        std::cerr << "FAIL: generated uniform block instance was corrupted\n";
        ++failures;
    }
    const std::string array_output = R"(#version 330
layout(location = 0) out vec4 coeff[2];
void main() {
    for (int attachment = 0; attachment < 2; ++attachment)
        for (int component = 0; component < 4; ++component)
            coeff[attachment][component] = gl_FragCoord.z;
}
)";
    for (unsigned version : {300u, 310u, 320u}) {
        if (!translate(array_output, version)) {
            std::cerr << "FAIL: dynamically indexed fragment output, ESSL " << version << '\n';
            ++failures;
        }
    }
    const std::string early_return = R"(#version 330
layout(location = 1) out vec4 outputs[2];
void fillOutputs() {
    for (int i = 0; i < 2; ++i) outputs[i] = vec4(float(i), 0.25, 0.5, 1.0);
}
void main() {
    fillOutputs();
    if (gl_FragCoord.x < 2.0) return;
    if (gl_FragCoord.y < 2.0) discard;
    outputs[0].r = 0.75;
    gl_FragDepth = gl_FragCoord.z;
}
)";
    if (!translate(early_return)) {
        std::cerr << "FAIL: output locations, helper writes, early return and depth\n";
        ++failures;
    }
    const std::string vertex_array = R"(#version 330
#extension GL_ARB_separate_shader_objects : require
layout(location = 0) out vec4 outputs[2];
void main() {
    for (int i = 0; i < 2; ++i) outputs[i] = vec4(float(i));
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)";
    if (!translate(vertex_array, 320, GL_VERTEX_SHADER)) {
        std::cerr << "FAIL: vertex output arrays\n";
        ++failures;
    }
    if (process_uniform_declarations("uniform float exposure = 1.0;\n") !=
        "uniform float exposure;\n") {
        std::cerr << "FAIL: uniform initializer removal\n";
        ++failures;
    }
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i]);
        std::ostringstream source;
        source << file.rdbuf();
        if (!file || !translate(source.str())) {
            std::cerr << "FAIL: " << argv[i] << '\n';
            ++failures;
        }
    }
    std::cout << failures << " shader regression failures\n";
    return failures ? 1 : 0;
}
