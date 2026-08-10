#include "Skybox.h"
#include "RenderThread.h"
#include "../core/Logger.h"
#include <glad/glad.h>
#include <vector>
#include <iostream>

#include <filesystem>
namespace Archura {
#include <stb_image.h>

Skybox::Skybox() {}

Skybox::~Skybox() {
    if ((m_VAO || m_VBO || m_TextureID) && !RenderThread::IsCurrent()) {
        ARCH_LOG_ERROR("Skybox destruction attempted outside the render thread; GPU deletion skipped");
        m_VAO = m_VBO = m_TextureID = 0;
        return;
    }
    if (m_VAO != 0) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO != 0) glDeleteBuffers(1, &m_VBO);
    if (m_TextureID != 0) glDeleteTextures(1, &m_TextureID);
}

bool Skybox::Init() {
    if (!RenderThread::IsCurrent()) return false;
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    const std::string vertexShaderSource = R"(#version 330 core
        layout (location = 0) in vec3 aPos;

        out vec3 TexCoords;

        uniform mat4 projection;
        uniform mat4 view;

        void main()
        {
            TexCoords = aPos;
            vec4 pos = projection * view * vec4(aPos, 1.0);
            gl_Position = pos.xyww;
        }
    )";

    const std::string fragmentShaderSource = R"(#version 330 core
        out vec4 FragColor;

        in vec3 TexCoords;

        uniform samplerCube skybox;
        uniform bool uUseTexture;

        void main()
        {    
            if (uUseTexture) {
                FragColor = texture(skybox, TexCoords);
            } else {
                // Debug Rainbow
                vec3 norm = normalize(TexCoords);
                FragColor = vec4(norm * 0.5 + 0.5, 1.0);
            }
        }
    )";

    m_Shader = std::make_unique<Shader>();
    if (!m_Shader->LoadFromSource(vertexShaderSource, fragmentShaderSource)) {
         std::cerr << "Skybox Shader Failed!\n";
         return false;
    }
    return m_VAO != 0 && m_VBO != 0;
}

bool Skybox::LoadCubemap(const std::vector<std::string>& faces) {
    if (!RenderThread::IsCurrent() || faces.size() != 6 || !m_Shader) {
        ARCH_LOG_ERROR("Skybox cubemap requires exactly six faces on the render thread");
        return false;
    }

    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousCubemap = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previousCubemap);

    unsigned int newTexture = 0;
    glGenTextures(1, &newTexture);
    if (!newTexture) return false;
    glBindTexture(GL_TEXTURE_CUBE_MAP, newTexture);

    int expectedWidth = 0;
    int expectedHeight = 0;
    stbi_set_flip_vertically_on_load_thread(0);
    
    std::cout << "Skybox: Loading 6 faces..." << std::endl;
    bool loaded = true;

    for (unsigned int i = 0; i < faces.size(); i++)
    {
        int width = 0, height = 0, nrChannels = 0;
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data && width > 0 && height > 0 && nrChannels >= 1 && nrChannels <= 4 &&
            (i == 0 || (width == expectedWidth && height == expectedHeight)))
        {
            if (i == 0) { expectedWidth = width; expectedHeight = height; }
            GLenum format = GL_RED;
            GLenum internalFormat = GL_R8;
            if (nrChannels == 2) { format = GL_RG; internalFormat = GL_RG8; }
            else if (nrChannels == 3) { format = GL_RGB; internalFormat = GL_RGB8; }
            else if (nrChannels == 4) { format = GL_RGBA; internalFormat = GL_RGBA8; }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                         0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        }
        else
        {
            std::cout << "[ERROR] Cubemap texture failed to load: " << faces[i] << std::endl;
            // Print absolute path to help user debug
            try {
                std::filesystem::path p = std::filesystem::absolute(faces[i]);
                std::cout << "        Tried looking at absolute path: " << p.string() << std::endl;
            } catch(...) {}
            std::cout << "        Reason: " << stbi_failure_reason() << std::endl;
            stbi_image_free(data);
            loaded = false;
            break;
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(previousCubemap));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    if (!loaded) {
        glDeleteTextures(1, &newTexture);
        return false;
    }

    const unsigned int oldTexture = m_TextureID;
    m_TextureID = newTexture;
    m_TextureLoaded = true;
    if (oldTexture) glDeleteTextures(1, &oldTexture);

    m_Shader->Bind();
    m_Shader->SetInt("skybox", 0);
    std::cout << "Skybox: LoadCubemap completed (ID=" << m_TextureID << ")" << std::endl;
    return true;
}

void Skybox::Draw(const Camera& camera, float aspectRatio) {
    if (!RenderThread::IsCurrent() || !m_Shader || !m_VAO) return;
    GLint previousDepthFunc = GL_LESS;
    GLint previousProgram = 0;
    GLint previousVAO = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousCubemap = 0;
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previousCubemap);
    glDepthFunc(GL_LEQUAL);
    m_Shader->Bind();
    
    // Remove translation from the view matrix
    glm::mat4 view = glm::mat4(glm::mat3(camera.GetViewMatrix())); 
    glm::mat4 projection = camera.GetProjectionMatrix(aspectRatio);

    m_Shader->SetMat4("view", view);
    m_Shader->SetMat4("projection", projection);
    m_Shader->SetInt("uUseTexture", m_TextureLoaded ? 1 : 0);

    // Cube is viewed from inside, so faces are "back" faces.
    // We must disable culling to see them.
    glDisable(GL_CULL_FACE);

    glBindVertexArray(m_VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_TextureID);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(previousCubemap));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindVertexArray(static_cast<GLuint>(previousVAO));
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    glUseProgram(static_cast<GLuint>(previousProgram));
}

} // namespace Archura
