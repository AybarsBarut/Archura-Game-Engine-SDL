#include "Shader.h"
#include "RenderThread.h"
#include "../core/Logger.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <utility>
#include <vector>

namespace Archura {

namespace {
bool RequireRenderThread(const char* operation) {
    if (RenderThread::IsCurrent()) return true;
    ARCH_LOG_ERROR(std::string(operation) + " must run on the OpenGL render thread");
    return false;
}
}

Shader::~Shader() {
    Release();
}

Shader::Shader(Shader&& other) noexcept {
    *this = std::move(other);
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this == &other) return *this;
    Release();
    m_ProgramID = std::exchange(other.m_ProgramID, 0);
    m_UniformLocationCache = std::move(other.m_UniformLocationCache);
    m_VertexPath = std::move(other.m_VertexPath);
    m_FragmentPath = std::move(other.m_FragmentPath);
    m_VertLastWrite = other.m_VertLastWrite;
    m_FragLastWrite = other.m_FragLastWrite;
    m_LoadedFromFile = std::exchange(other.m_LoadedFromFile, false);
    return *this;
}

void Shader::Release() noexcept {
    if (m_ProgramID && !RenderThread::IsCurrent()) {
        ARCH_LOG_ERROR("Shader destruction attempted outside the render thread; GPU deletion skipped");
        m_ProgramID = 0;
        m_UniformLocationCache.clear();
        return;
    }
    if (m_ProgramID) glDeleteProgram(std::exchange(m_ProgramID, 0));
    m_UniformLocationCache.clear();
}

bool Shader::LoadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    if (!RequireRenderThread("Shader::LoadFromSource")) return false;
    // Vertex shader compile
    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    if (vertexShader == 0) return false;

    // Fragment shader compile
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    // Link into a temporary program. The old program remains live when compile/link
    // fails, which makes editor hot reload transactional.
    const unsigned int newProgram = LinkProgram(vertexShader, fragmentShader);

    // Shader'lari temizle (artik programa baglandi)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (newProgram == 0) return false;

    const unsigned int oldProgram = m_ProgramID;
    m_ProgramID = newProgram;
    m_UniformLocationCache.clear();
    if (oldProgram) glDeleteProgram(oldProgram);
    return true;
}

bool Shader::LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
    // Vertex shader dosyasini oku
    std::ifstream vertexFile(vertexPath);
    if (!vertexFile.is_open()) {
        ARCH_LOG_ERROR("Failed to open vertex shader: " + vertexPath);
        return false;
    }
    std::stringstream vertexStream;
    vertexStream << vertexFile.rdbuf();
    std::string vertexSrc = vertexStream.str();

    // Fragment shader dosyasini oku
    std::ifstream fragmentFile(fragmentPath);
    if (!fragmentFile.is_open()) {
        ARCH_LOG_ERROR("Failed to open fragment shader: " + fragmentPath);
        return false;
    }
    std::stringstream fragmentStream;
    fragmentStream << fragmentFile.rdbuf();
    std::string fragmentSrc = fragmentStream.str();

    bool ok = LoadFromSource(vertexSrc, fragmentSrc);
    if (ok) {
        // Hot-reload icin dosya bilgilerini kaydet
        m_VertexPath    = vertexPath;
        m_FragmentPath  = fragmentPath;
        m_LoadedFromFile = true;
        namespace fs = std::filesystem;
        try {
            m_VertLastWrite = fs::last_write_time(fs::path(vertexPath));
            m_FragLastWrite = fs::last_write_time(fs::path(fragmentPath));
        } catch (const fs::filesystem_error& e) {
            ARCH_LOG_WARN("Shader loaded, but file timestamps are unavailable: " +
                          std::string(e.what()));
            m_LoadedFromFile = false;
        }
    }
    return ok;
}

bool Shader::CheckAndReload() {
    if (!m_LoadedFromFile) return false;
    namespace fs = std::filesystem;

    bool changed = false;
    try {
        auto vertNow = fs::last_write_time(fs::path(m_VertexPath));
        auto fragNow = fs::last_write_time(fs::path(m_FragmentPath));
        if (vertNow != m_VertLastWrite || fragNow != m_FragLastWrite) {
            changed = true;
        }
    } catch (...) {
        return false; // Dosya gecici olarak erisilemez durumda olabilir
    }

    if (!changed) return false;

    ARCH_LOG_INFO("[HotReload] Shader degisti, yeniden derleniyor: " + m_VertexPath);

    // LoadFromFile swaps the program only after both stages link successfully.
    bool ok = LoadFromFile(m_VertexPath, m_FragmentPath);
    if (ok) {
        ARCH_LOG_INFO("[HotReload] Shader basariyla yeniden derlendi.");
    } else {
        ARCH_LOG_ERROR("[HotReload] Shader yeniden derleme HATASI: " + m_VertexPath);
    }
    return ok;
}

void Shader::Bind() const {
    if (!RequireRenderThread("Shader::Bind")) return;
    glUseProgram(m_ProgramID);
}

void Shader::Unbind() const {
    if (!RequireRenderThread("Shader::Unbind")) return;
    glUseProgram(0);
}

void Shader::SetInt(const std::string& name, int value) {
    if (!RequireRenderThread("Shader::SetInt")) return;
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) {
    if (!RequireRenderThread("Shader::SetFloat")) return;
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) {
    if (!RequireRenderThread("Shader::SetVec2")) return;
    glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) {
    if (!RequireRenderThread("Shader::SetVec3")) return;
    glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) {
    if (!RequireRenderThread("Shader::SetVec4")) return;
    glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetMat3(const std::string& name, const glm::mat3& value) {
    if (!RequireRenderThread("Shader::SetMat3")) return;
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) {
    if (!RequireRenderThread("Shader::SetMat4")) return;
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

unsigned int Shader::CompileShader(unsigned int type, const std::string& source) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // Derleme hatasini kontrol et
    int success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(static_cast<size_t>(std::max(logLength, 1)), '\0');
        glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), nullptr,
                           infoLog.data());
        std::string msg = std::string("Shader compilation error (") +
                          (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment") + 
                          "):\n" + infoLog.data();
        ARCH_LOG_ERROR(msg);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

unsigned int Shader::LinkProgram(unsigned int vertexShader, unsigned int fragmentShader) {
    const unsigned int program = glCreateProgram();
    if (program == 0) {
        ARCH_LOG_ERROR("OpenGL failed to create a shader program object");
        return 0;
    }
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Baglama hatasini kontrol et
    int success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        int logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(static_cast<size_t>(std::max(logLength, 1)), '\0');
        glGetProgramInfoLog(program, static_cast<GLsizei>(infoLog.size()), nullptr,
                            infoLog.data());
        ARCH_LOG_ERROR("Shader linking error:\n" + std::string(infoLog.data()));
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

int Shader::GetUniformLocation(const std::string& name) {
    // Onbellekte var mi kontrol et
    const auto cached = m_UniformLocationCache.find(name);
    if (cached != m_UniformLocationCache.end()) return cached->second;

    // OpenGL'den konumu al ve onbellege al
    int location = glGetUniformLocation(m_ProgramID, name.c_str());
    if (location == -1) {
        ARCH_LOG_WARN("uniform '" + name + "' not found!");
    }

    m_UniformLocationCache[name] = location;
    return location;
}

} // namespace Archura
