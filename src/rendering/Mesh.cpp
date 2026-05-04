#include "Mesh.h"
#include "Shader.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <map>
#include <algorithm>
#include <ufbx.h>
#include <fast_obj.h>

namespace Archura {

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : m_Vertices(vertices)
    , m_Indices(indices)
    , m_VAO(0)
    , m_VBO(0)
    , m_EBO(0)
{
    SetupMesh();
}

Mesh::~Mesh() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
}

void Mesh::SetupMesh() {
    // VAO olustur
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    // VBO - Vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex), m_Vertices.data(), GL_STATIC_DRAW);

    // EBO - Index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(unsigned int), m_Indices.data(), GL_STATIC_DRAW);

    // Vertex attributes
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // TexCoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    // Color
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    // Animation Attributes (8, 9 to avoid conflict with Instancing 4-7)
    // Bone IDs
    glEnableVertexAttribArray(8);
    glVertexAttribIPointer(8, MAX_BONE_INFLUENCE, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

    // Weights
    glEnableVertexAttribArray(9);
    glVertexAttribPointer(9, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));

    glBindVertexArray(0);
}


void Mesh::SetupInstancedAttributes() {
    if (m_InstancedSetup) return;

    glBindVertexArray(m_VAO);
    glGenBuffers(1, &m_InstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_InstanceVBO);
    
    // Mat4 4 tane vec4'ten olusur. Attribute location 4, 5, 6, 7 kullanacagiz.
    std::size_t vec4Size = sizeof(glm::vec4);
    
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(4 * vec4Size), (void*)(i * vec4Size));
        glVertexAttribDivisor(4 + i, 1); // Her instance icin 1 kez ilerle
    }

    glBindVertexArray(0);
    m_InstancedSetup = true;
}

void Mesh::DrawInstanced(Shader* shader, const std::vector<glm::mat4>& models) {
    if (models.empty()) return;

    if (shader) {
        shader->Bind();
        // Instanced cizimde uModel uniform'u yerine attribute kullanilir. 
        // Shader'in buna uygun olmasi lazim.
        // Eger shader desteklemiyorsa, normal draw fallback yapilabilir ama performans duser.
        // Simdilik Shader'in "uUseInstancing" uniformu oldugunu varsayalim veya
        // Shader kodunu da guncelleyelim.
        shader->SetInt("uUseInstancing", 1); 
    }

    if (!m_InstancedSetup) {
        SetupInstancedAttributes();
    }

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_InstanceVBO);

    // Buffer verisini guncelle
    // Eger kapasite yeterliyse glBufferSubData, degilse glBufferData
    size_t dataSize = models.size() * sizeof(glm::mat4);
    if (dataSize > m_InstanceCapacity) {
        glBufferData(GL_ARRAY_BUFFER, dataSize, models.data(), GL_DYNAMIC_DRAW);
        m_InstanceCapacity = dataSize;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, models.data());
    }

    glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(m_Indices.size()), GL_UNSIGNED_INT, 0, static_cast<unsigned int>(models.size()));
    
    glBindVertexArray(0);
    
    if (shader) {
        shader->SetInt("uUseInstancing", 0); // Reset
    }
}

void Mesh::Draw(Shader* shader) {
    if (shader) {
        shader->Bind();
    }

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(m_Indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// ==================== Prosedurel Model Olusturucular ====================

Mesh* Mesh::CreateCube(float size) {
    float s = size * 0.5f;
    
    std::vector<Vertex> vertices = {
        // On yuz
        {{-s, -s,  s}, { 0,  0,  1}, {0, 0}, {1, 1, 1}},
        {{ s, -s,  s}, { 0,  0,  1}, {1, 0}, {1, 1, 1}},
        {{ s,  s,  s}, { 0,  0,  1}, {1, 1}, {1, 1, 1}},
        {{-s,  s,  s}, { 0,  0,  1}, {0, 1}, {1, 1, 1}},
        
        // Arka yuz
        {{ s, -s, -s}, { 0,  0, -1}, {0, 0}, {1, 1, 1}},
        {{-s, -s, -s}, { 0,  0, -1}, {1, 0}, {1, 1, 1}},
        {{-s,  s, -s}, { 0,  0, -1}, {1, 1}, {1, 1, 1}},
        {{ s,  s, -s}, { 0,  0, -1}, {0, 1}, {1, 1, 1}},
        
        // Ust yuz
        {{-s,  s,  s}, { 0,  1,  0}, {0, 0}, {1, 1, 1}},
        {{ s,  s,  s}, { 0,  1,  0}, {1, 0}, {1, 1, 1}},
        {{ s,  s, -s}, { 0,  1,  0}, {1, 1}, {1, 1, 1}},
        {{-s,  s, -s}, { 0,  1,  0}, {0, 1}, {1, 1, 1}},
        
        // Alt yuz
        {{-s, -s, -s}, { 0, -1,  0}, {0, 0}, {1, 1, 1}},
        {{ s, -s, -s}, { 0, -1,  0}, {1, 0}, {1, 1, 1}},
        {{ s, -s,  s}, { 0, -1,  0}, {1, 1}, {1, 1, 1}},
        {{-s, -s,  s}, { 0, -1,  0}, {0, 1}, {1, 1, 1}},
        
        // Sag yuz
        {{ s, -s,  s}, { 1,  0,  0}, {0, 0}, {1, 1, 1}},
        {{ s, -s, -s}, { 1,  0,  0}, {1, 0}, {1, 1, 1}},
        {{ s,  s, -s}, { 1,  0,  0}, {1, 1}, {1, 1, 1}},
        {{ s,  s,  s}, { 1,  0,  0}, {0, 1}, {1, 1, 1}},
        
        // Sol yuz
        {{-s, -s, -s}, {-1,  0,  0}, {0, 0}, {1, 1, 1}},
        {{-s, -s,  s}, {-1,  0,  0}, {1, 0}, {1, 1, 1}},
        {{-s,  s,  s}, {-1,  0,  0}, {1, 1}, {1, 1, 1}},
        {{-s,  s, -s}, {-1,  0,  0}, {0, 1}, {1, 1, 1}},
    };
    
    std::vector<unsigned int> indices;
    for (unsigned int i = 0; i < 6; ++i) {
        unsigned int base = i * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    
    return new Mesh(vertices, indices);
}

Mesh* Mesh::CreatePlane(float width, float height, float uvScale) {
    float w = width * 0.5f;
    float h = height * 0.5f;
    
    std::vector<Vertex> vertices = {
        {{-w, 0,  h}, {0, 1, 0}, {0, 0}, {0.8f, 0.8f, 0.8f}},
        {{ w, 0,  h}, {0, 1, 0}, {1 * uvScale, 0}, {0.8f, 0.8f, 0.8f}},
        {{ w, 0, -h}, {0, 1, 0}, {1 * uvScale, 1 * uvScale}, {0.8f, 0.8f, 0.8f}},
        {{-w, 0, -h}, {0, 1, 0}, {0, 1 * uvScale}, {0.8f, 0.8f, 0.8f}},
    };
    
    std::vector<unsigned int> indices = {
        0, 1, 2,
        0, 2, 3
    };
    
    return new Mesh(vertices, indices);
}

Mesh* Mesh::CreateSphere(float radius, int segments) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    const float PI = 3.14159265359f;
    const int rings = segments / 2;
    const int sectors = segments;
    
    // Koseler
    for (int r = 0; r <= rings; ++r) {
        float phi = PI * (float)r / (float)rings;
        
        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * PI * (float)s / (float)sectors;
            
            Vertex vertex;
            vertex.position.x = radius * sin(phi) * cos(theta);
            vertex.position.y = radius * cos(phi);
            vertex.position.z = radius * sin(phi) * sin(theta);
            
            vertex.normal = glm::normalize(vertex.position);
            vertex.texCoords = glm::vec2((float)s / sectors, (float)r / rings);
            vertex.color = glm::vec3(1.0f);
            
            vertices.push_back(vertex);
        }
    }
    
    // Indeksler
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            unsigned int current = r * (sectors + 1) + s;
            unsigned int next = current + sectors + 1;
            
            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);
            
            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
        }
    }
    
    return new Mesh(vertices, indices);
}

Mesh* Mesh::CreateCapsule(float radius, float height) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    int segments = 16;
    int rings = 8;
    float cylinderHeight = height - 2 * radius;
    if (cylinderHeight < 0) cylinderHeight = 0;
    float halfHeight = cylinderHeight * 0.5f;

    // Ust Yarikure
    for (int r = 0; r <= rings; ++r) {
        float phi = glm::half_pi<float>() * (float)r / (float)rings; // 0 to PI/2
        
        for (int s = 0; s <= segments; ++s) {
            float theta = glm::two_pi<float>() * (float)s / (float)segments;

            float x = radius * cos(phi) * cos(theta);
            float y = radius * sin(phi);
            float z = radius * cos(phi) * sin(theta);

            Vertex v;
            v.position = glm::vec3(x, y + halfHeight, z);
            v.normal = glm::normalize(glm::vec3(x, y, z)); // Normal is relative to sphere center
            v.texCoords = glm::vec2((float)s / segments, (float)r / (rings * 2 + 1)); // Simplified UV
            v.color = glm::vec3(1.0f);
            vertices.push_back(v);
        }
    }

    // Silindir Govdesi
    // Silindirin ust halkasi ust yarikurenin alt halkasiyla eslesir
    // Silindirin alt halkasi alt yarikurenin ust halkasiyla eslesir
    // Keskin normaller isteseydik silindir govdesi icin koseler ekleyebilirdik,
    // ancak puruzsuz bir kapsul icin koseleri paylasabilir veya uretime devam edebiliriz.
    // UV haritalama ve yapi basitligi icin ozel silindir koseleri uretelim.
    
    for (int s = 0; s <= segments; ++s) {
        float theta = glm::two_pi<float>() * (float)s / (float)segments;
        float x = radius * cos(theta);
        float z = radius * sin(theta);

        // Silindirin ustu
        Vertex vTop;
        vTop.position = glm::vec3(x, halfHeight, z);
        vTop.normal = glm::normalize(glm::vec3(x, 0, z));
        vTop.texCoords = glm::vec2((float)s / segments, 0.5f);
        vTop.color = glm::vec3(1.0f);
        vertices.push_back(vTop);

        // Silindirin alti
        Vertex vBottom;
        vBottom.position = glm::vec3(x, -halfHeight, z);
        vBottom.normal = glm::normalize(glm::vec3(x, 0, z));
        vBottom.texCoords = glm::vec2((float)s / segments, 0.5f);
        vBottom.color = glm::vec3(1.0f);
        vertices.push_back(vBottom);
    }

    // Alt Yarikure
    for (int r = 0; r <= rings; ++r) {
        float phi = glm::half_pi<float>() * (float)r / (float)rings; // 0 to PI/2 (inverted later)
        
        for (int s = 0; s <= segments; ++s) {
            float theta = glm::two_pi<float>() * (float)s / (float)segments;

            float x = radius * cos(phi) * cos(theta);
            float y = -radius * sin(phi); // Alt icin negatif y
            float z = radius * cos(phi) * sin(theta);

            Vertex v;
            v.position = glm::vec3(x, y - halfHeight, z);
            v.normal = glm::normalize(glm::vec3(x, y, z));
            v.texCoords = glm::vec2((float)s / segments, 0.5f + (float)r / (rings * 2 + 1));
            v.color = glm::vec3(1.0f);
            vertices.push_back(v);
        }
    }

    // Indeks Uretimi
    // Cok parcali uretim nedeniyle bu biraz karmasik.
    // Basitlik ve saglamlik icin standart Kure uretimini kullanalim ancak ortayi uzatalim.
    // "Uzatilmis Kure" yaklasimiyla yeniden uygulama:
    
    vertices.clear();
    indices.clear();
    
    int totalRings = rings * 2 + 2; // Ust Kapak + silindir (2 halka) + alt kapak
    // Aslinda, soyle yapalim: Ust Kapak (halkalar), Silindir (2 halka), Alt Kapak (halkalar)
    // Ancak tek bir puruzsuz model yapmak icin:
    // 0'dan halkalara: Ust Yarikure
    // halkalardan halkalar+1'e: Silindir
    // halkalar+1'den 2*halkalar+1'e: Alt Yarikure
    
    for (int r = 0; r <= rings; ++r) { // Top Hemisphere
        float phi = glm::half_pi<float>() * (1.0f - (float)r / rings); // PI/2 to 0
        for (int s = 0; s <= segments; ++s) {
            float theta = glm::two_pi<float>() * (float)s / segments;
            float x = radius * cos(phi) * cos(theta);
            float y = radius * sin(phi);
            float z = radius * cos(phi) * sin(theta);
            
            Vertex v;
            v.position = glm::vec3(x, y + halfHeight, z);
            v.normal = glm::normalize(glm::vec3(x, y, z));
            v.texCoords = glm::vec2((float)s / segments, (float)r / (totalRings)); 
            v.color = glm::vec3(1.0f);
            vertices.push_back(v);
        }
    }
    
    for (int r = 0; r <= rings; ++r) { // Bottom Hemisphere
        float phi = glm::half_pi<float>() * ((float)r / rings); // 0 to PI/2 (going down)
        for (int s = 0; s <= segments; ++s) {
            float theta = glm::two_pi<float>() * (float)s / segments;
            float x = radius * cos(phi) * cos(theta);
            float y = -radius * sin(phi);
            float z = radius * cos(phi) * sin(theta);
            
            Vertex v;
            v.position = glm::vec3(x, y - halfHeight, z);
            v.normal = glm::normalize(glm::vec3(x, y, z));
            v.texCoords = glm::vec2((float)s / segments, (float)(r + rings) / (totalRings));
            v.color = glm::vec3(1.0f);
            vertices.push_back(v);
        }
    }
    
    // Indices
    int verticesPerRing = segments + 1;
    int ringCount = static_cast<int>(vertices.size() / verticesPerRing);
    
    for (int r = 0; r < ringCount - 1; ++r) {
        for (int s = 0; s < segments; ++s) {
            int current = r * verticesPerRing + s;
            int next = current + verticesPerRing;
            
            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);
            
            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
        }
    }

    return new Mesh(vertices, indices);
}

Mesh* Mesh::CreateStairs(float width, float height, float depth, int steps) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float stepHeight = height / steps;
    float stepDepth = depth / steps;
    float halfWidth = width * 0.5f;

    // Her basamak bir kutu (cube) gibidir
    // Basitlik icin her basamagi ayri ayri olusturalim
    // Optimize edilebilir ama bu yeterli
    
    // int vertexOffset = 0; // Removed unused variable

    for (int i = 0; i < steps; ++i) {
        // Basamak boyutlari
        float yBottom = 0.0f; // Side faces still go to ground for "solid" look
        float yRiseBottom = i * stepHeight; // Front face (riser) starts here
        float yTop = (i + 1) * stepHeight;
        float zFront = (i) * stepDepth - (depth * 0.5f);
        float zBack = (i + 1) * stepDepth - (depth * 0.5f);
        
        // 8 Kose - Yan yuzeyler icin tam yukseklik (yBottom -> yTop)
        glm::vec3 p0(-halfWidth, yBottom, zBack);
        glm::vec3 p1( halfWidth, yBottom, zBack);
        glm::vec3 p2( halfWidth, yTop,    zBack);
        glm::vec3 p3(-halfWidth, yTop,    zBack);
        
        // On yuzey (Riser) icin kisaltilmis alt noktalar
        // Not: Yan yuzeyler p4-p7'yi kullaniyor mu?
        // Evet: Sol yuz p0, p4, p7, p3. 
        // Sorun: Eger p4'u degistirirsek yan yuzeyin sekli bozulur mu?
        // Yan yuzey (p0, p4, p7, p3) -> (BottomBack, BottomFront, TopFront, TopBack).
        // Eger p4.y'yi yukseltirsek, yan yuzeyin alti "yirtilir".
        // COZUM: On yuz icin AYRI vertexler veya p4/p5'i sadece on yuz kullanirken modifiye etmemiz lazim.
        // Mevcut kodda p4..p7 point'leri paylasiliyor.
        // Ancak push_back sirasinda vertex struct'i anlik olusturuluyor.
        // Yani p4_full ve p4_riser diye ayirabiliriz.
        
        glm::vec3 p4(-halfWidth, yBottom, zFront);
        glm::vec3 p5( halfWidth, yBottom, zFront);
        glm::vec3 p6( halfWidth, yTop,    zFront);
        glm::vec3 p7(-halfWidth, yTop,    zFront);

        // --- On (Front) Face (Riser) ---
        // Sadece gorunur kismi ciz: yRiseBottom -> yTop
        glm::vec3 p4_riser = p4; p4_riser.y = yRiseBottom;
        glm::vec3 p5_riser = p5; p5_riser.y = yRiseBottom;

        vertices.push_back({p4_riser, {0,0,1}, {0,0}, {1,1,1}});
        vertices.push_back({p5_riser, {0,0,1}, {1,0}, {1,1,1}});
        vertices.push_back({p6,       {0,0,1}, {1,1}, {1,1,1}});
        vertices.push_back({p7,       {0,0,1}, {0,1}, {1,1,1}});
        
        // --- Ust (Top) Face ---
        vertices.push_back({p7, {0,1,0}, {0,0}, {1,1,1}});
        vertices.push_back({p6, {0,1,0}, {1,0}, {1,1,1}});
        vertices.push_back({p2, {0,1,0}, {1,1}, {1,1,1}});
        vertices.push_back({p3, {0,1,0}, {0,1}, {1,1,1}});

        // --- Yanlar (Left/Right) ---
        // Yanlar tam yukseklikte olmali (Yerde duruyor gibi gorunmesi icin)
        
        // Sag - p5, p1, p2, p6
        vertices.push_back({p5, {1,0,0}, {0,0}, {1,1,1}});
        vertices.push_back({p1, {1,0,0}, {1,0}, {1,1,1}});
        vertices.push_back({p2, {1,0,0}, {1,1}, {1,1,1}});
        vertices.push_back({p6, {1,0,0}, {0,1}, {1,1,1}});

        // Sol - p0, p4, p7, p3
        vertices.push_back({p0, {-1,0,0}, {0,0}, {1,1,1}});
        vertices.push_back({p4, {-1,0,0}, {1,0}, {1,1,1}});
        vertices.push_back({p7, {-1,0,0}, {1,1}, {1,1,1}});
        vertices.push_back({p3, {-1,0,0}, {0,1}, {1,1,1}});

        // --- Arka (Back) ---
        // Sadece en son basamakta ciz (Digerleri iceride kaliyor)
        if (i == steps - 1) {
            vertices.push_back({p1, {0,0,-1}, {0,0}, {1,1,1}});
            vertices.push_back({p0, {0,0,-1}, {1,0}, {1,1,1}});
            vertices.push_back({p3, {0,0,-1}, {1,1}, {1,1,1}});
            vertices.push_back({p2, {0,0,-1}, {0,1}, {1,1,1}});
        }

        // --- Alt (Bottom) ---
        // Her basamak icin cizilebilir (Yuzeyler yanyana, cakisma yok)
        vertices.push_back({p0, {0,-1,0}, {0,0}, {1,1,1}});
        vertices.push_back({p1, {0,-1,0}, {1,0}, {1,1,1}});
        vertices.push_back({p5, {0,-1,0}, {1,1}, {1,1,1}});
        vertices.push_back({p4, {0,-1,0}, {0,1}, {1,1,1}});

        // Indeksler
        // Back Face haric (i != steps-1) indeks sayisi degisir mi?
        // Evet. Eger vertex eklemezsek, index offset kayar.
        // Ancak biz burada vertex push_back'i IF icine aldik.
        // Bu yuzden vertexOffset'i dinamik hesaplamaliyiz veya
        // push_back yaptikca index eklemeliyiz.
        // En guzeli push_back sirasinda index eklemek.
        // Mevcut dongu "vertexOffset" sabit artiriyor (+24). Bu yanlis olur.
        // Kodu degistirip anlik vertex count uzerinden index ekleyelim.
    }
    
    // Indeksleri sona, toplu degil, dongu icinde vertex ekledikce ekleyelim.
    // Ancak yukaridaki yapi vertices/indices ayri vektorler.
    // Yeniden duzenliyoruz:
    
    vertices.clear();
    indices.clear();

    for (int i = 0; i < steps; ++i) {
        float yBottom = 0.0f;
        float yRiseBottom = i * stepHeight;
        float yTop = (i + 1) * stepHeight;
        float zFront = (i) * stepDepth - (depth * 0.5f);
        float zBack = (i + 1) * stepDepth - (depth * 0.5f);
        
        glm::vec3 p0(-halfWidth, yBottom, zBack);
        glm::vec3 p1( halfWidth, yBottom, zBack);
        glm::vec3 p2( halfWidth, yTop,    zBack);
        glm::vec3 p3(-halfWidth, yTop,    zBack);
        
        glm::vec3 p4(-halfWidth, yBottom, zFront);
        glm::vec3 p5( halfWidth, yBottom, zFront);
        glm::vec3 p6( halfWidth, yTop,    zFront);
        glm::vec3 p7(-halfWidth, yTop,    zFront);

        // Helper Lambda
        auto AddQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& normal) {
            unsigned int startIdx = (unsigned int)vertices.size();
            vertices.push_back({v0, normal, {0,0}, {1,1,1}});
            vertices.push_back({v1, normal, {1,0}, {1,1,1}});
            vertices.push_back({v2, normal, {1,1}, {1,1,1}});
            vertices.push_back({v3, normal, {0,1}, {1,1,1}});
            
            indices.push_back(startIdx + 0);
            indices.push_back(startIdx + 1);
            indices.push_back(startIdx + 2);
            indices.push_back(startIdx + 0);
            indices.push_back(startIdx + 2);
            indices.push_back(startIdx + 3);
        };

        // Front (Riser)
        glm::vec3 p4_riser = p4; p4_riser.y = yRiseBottom;
        glm::vec3 p5_riser = p5; p5_riser.y = yRiseBottom;
        AddQuad(p5_riser, p4_riser, p7, p6, {0,0,-1});
        
        // Top
        AddQuad(p6, p7, p3, p2, {0,1,0});
        
        // Sides
        AddQuad(p1, p5, p6, p2, {1,0,0});  // Right
        AddQuad(p4, p0, p3, p7, {-1,0,0}); // Left
        
        // Bottom
        AddQuad(p0, p1, p5, p4, {0,-1,0});
        
        // Back (Only last)
        if (i == steps - 1) {
            AddQuad(p0, p1, p2, p3, {0,0,1});
        }
    }

    return new Mesh(vertices, indices);
}

Mesh* Mesh::CreateRamp(float width, float height, float depth) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float w = width * 0.5f;
    float h = height; // Rampanin yuksekligi
    float d = depth * 0.5f;

    // Wedge (Kama) Sekli
    // 6 Kose (Prizma)
    // Taban dikdortgen, ust kenar cizgi
    
    // Arka Yuz (Dikdortgen)
    glm::vec3 p0(-w, 0, -d);
    glm::vec3 p1( w, 0, -d);
    glm::vec3 p2( w, h, -d);
    glm::vec3 p3(-w, h, -d);

    // On Yuz (Cizgi - Yerde)
    glm::vec3 p4(-w, 0, d);
    glm::vec3 p5( w, 0, d);

    // Egimli Yuzey (Rampa) - p4, p5, p2, p3
    // Normali hesapla
    glm::vec3 rampVec = glm::vec3(0, h, -2*d);
    glm::vec3 rampNormal = glm::normalize(glm::cross(glm::vec3(1,0,0), rampVec)); // Cikan yone dogru

    vertices.push_back({p4, rampNormal, {0,0}, {1,1,1}});
    vertices.push_back({p5, rampNormal, {1,0}, {1,1,1}});
    vertices.push_back({p2, rampNormal, {1,1}, {1,1,1}});
    vertices.push_back({p3, rampNormal, {0,1}, {1,1,1}});

    // Arka Yuz
    vertices.push_back({p1, {0,0,-1}, {0,0}, {1,1,1}});
    vertices.push_back({p0, {0,0,-1}, {1,0}, {1,1,1}});
    vertices.push_back({p3, {0,0,-1}, {1,1}, {1,1,1}});
    vertices.push_back({p2, {0,0,-1}, {0,1}, {1,1,1}});

    // Alt Yuz
    vertices.push_back({p4, {0,-1,0}, {0,0}, {1,1,1}});
    vertices.push_back({p5, {0,-1,0}, {1,0}, {1,1,1}});
    vertices.push_back({p1, {0,-1,0}, {1,1}, {1,1,1}});
    vertices.push_back({p0, {0,-1,0}, {0,1}, {1,1,1}});

    // Yan Yuzler (Ucgen)
    // Sag - p5, p1, p2
    vertices.push_back({p5, {1,0,0}, {0,0}, {1,1,1}});
    vertices.push_back({p1, {1,0,0}, {1,0}, {1,1,1}});
    vertices.push_back({p2, {1,0,0}, {0,1}, {1,1,1}});

    // Sol - p0, p4, p3
    vertices.push_back({p0, {-1,0,0}, {0,0}, {1,1,1}});
    vertices.push_back({p4, {-1,0,0}, {1,0}, {1,1,1}});
    vertices.push_back({p3, {-1,0,0}, {0,1}, {1,1,1}});

    // Indeksler
    // Rampa (Quad)
    indices.push_back(0); indices.push_back(1); indices.push_back(2);
    indices.push_back(0); indices.push_back(2); indices.push_back(3);

    // Arka (Quad)
    indices.push_back(4); indices.push_back(5); indices.push_back(6);
    indices.push_back(4); indices.push_back(6); indices.push_back(7);

    // Alt (Quad)
    indices.push_back(8); indices.push_back(9); indices.push_back(10);
    indices.push_back(8); indices.push_back(10); indices.push_back(11);

    // Sag (Tri)
    indices.push_back(12); indices.push_back(13); indices.push_back(14);

    // Sol (Tri)
    indices.push_back(15); indices.push_back(16); indices.push_back(17);

    return new Mesh(vertices, indices);
}

Mesh* Mesh::LoadFromOBJ(const std::string& path) {
    fastObjMesh* obj = fast_obj_read(path.c_str());
    if (!obj) {
        std::cerr << "Failed to open OBJ file: " << path << "\n";
        return nullptr;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    unsigned int index_offset = 0;
    
    // fast_obj returns faces (polygons), we need to triangulate them.
    for (unsigned int i = 0; i < obj->face_count; ++i) {
        unsigned int num_vertices = obj->face_vertices[i];
        
        // Triangulate on the fly using ear clipping (triangle fan)
        for (unsigned int v = 1; v + 1 < num_vertices; ++v) {
            fastObjIndex idx[3];
            idx[0] = obj->indices[index_offset + 0];
            idx[1] = obj->indices[index_offset + v];
            idx[2] = obj->indices[index_offset + v + 1];

            // Produce 3 vertices for this triangle
            for (int k = 0; k < 3; ++k) {
                Vertex vertex;
                // Reset to default
                vertex.position = glm::vec3(0.0f);
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                vertex.texCoords = glm::vec2(0.0f);
                vertex.color = glm::vec3(1.0f); // Default white color

                if (idx[k].p) {
                    vertex.position = glm::vec3(
                        obj->positions[3 * idx[k].p + 0],
                        obj->positions[3 * idx[k].p + 1],
                        obj->positions[3 * idx[k].p + 2]
                    );
                }
                
                if (idx[k].t) {
                    vertex.texCoords = glm::vec2(
                        obj->texcoords[2 * idx[k].t + 0],
                        obj->texcoords[2 * idx[k].t + 1]
                    );
                }
                
                if (idx[k].n) {
                    vertex.normal = glm::vec3(
                        obj->normals[3 * idx[k].n + 0],
                        obj->normals[3 * idx[k].n + 1],
                        obj->normals[3 * idx[k].n + 2]
                    );
                }
                
                vertices.push_back(vertex);
                indices.push_back((unsigned int)vertices.size() - 1);
            }
        }
        index_offset += num_vertices;
    }
    
    fast_obj_destroy(obj);
    
    if (vertices.empty()) return nullptr;
    
    return new Mesh(vertices, indices);
}

Mesh* Mesh::LoadFromFBX(const std::string& path) {
    ufbx_load_opts opts = { 0 }; 
    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);

    if (!scene) {
        std::cerr << "Failed to load FBX: " << path << " Error: " << error.description.data << "\n";
        return nullptr;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    size_t indexOffset = 0;

    // Sahnedeki tum meshleri tek bir Mesh objesinde birlestirelim
    for (size_t i = 0; i < scene->nodes.count; i++) {
        ufbx_node* node = scene->nodes.data[i];
        if (node->mesh) {
            ufbx_mesh* mesh = node->mesh;
            
            // --- Precompute Skinning Weights ---
            // Map: Original Vertex Index -> List of (BoneID, Weight)
            std::map<int, std::vector<std::pair<int, float>>> vertexWeights;

            if (mesh->skin_deformers.count > 0) {
                ufbx_skin_deformer* skin = mesh->skin_deformers.data[0];
                for (size_t ci = 0; ci < skin->clusters.count; ci++) {
                    ufbx_skin_cluster* cluster = skin->clusters.data[ci];
                    int boneID = (int)ci; // Ideally map bone name to ID using BoneInfo, but for now linear ID

                    // Note: This logic assumes simple linear bone mapping. 
                    // To do it properly, we need to build the skeleton hierarchy first.
                    // But simplified: Just assign IDs.
                    
                    for (size_t vi = 0; vi < cluster->vertices.count; vi++) {
                        int vertIndex = cluster->vertices.data[vi];
                        float weight = (float)cluster->weights.data[vi];
                        vertexWeights[vertIndex].push_back({boneID, weight});
                    }
                }
            }

            // Transformu hesaba katalim mi? Simdilik local space alalim, 
            // ama ideal olarak node->geometry_to_world kullanmaliyiz.
            // Karmasiklik olmamasi icin raw mesh verisini alalim.
            
            // Indeksleri isle
            // ufbx yuzleri (faces) saklar. Genelde ucgenleme gerekir ama ufbx bunu kolaylastirir.
            // Basitce tum yuzleri dolasip ucgenlere bolelim.
            
            // Onceden vertexleri duzlestirmek yerine, her bir face corner icin vertex olusturacagiz.
            // Bu duplicate vertex yaratabilir ama en guvenli yoldur.
            
            // Daha iyi yol: ufbx_triangulate_face
            std::vector<uint32_t> tri_indices;
            tri_indices.resize(mesh->max_face_triangles * 3);

            for (size_t fi = 0; fi < mesh->faces.count; fi++) {
                ufbx_face face = mesh->faces.data[fi];
                size_t num_tris = ufbx_triangulate_face(tri_indices.data(), tri_indices.size(), mesh, face);

                for (size_t ti = 0; ti < num_tris; ti++) {
                    for (int k = 0; k < 3; k++) {
                        uint32_t ix = tri_indices[ti * 3 + k];
                        
                        Vertex v;
                        
                        // Position
                        ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, ix);
                        v.position = glm::vec3(pos.x, pos.y, pos.z);

                        // Normal
                        if (mesh->vertex_normal.exists) {
                            ufbx_vec3 norm = ufbx_get_vertex_vec3(&mesh->vertex_normal, ix);
                            v.normal = glm::vec3(norm.x, norm.y, norm.z);
                        }

                        // UV
                        if (mesh->vertex_uv.exists) {
                            ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, ix);
                            v.texCoords = glm::vec2(uv.x, uv.y);
                        }

                        if (mesh->vertex_color.exists) {
                            ufbx_vec4 col = ufbx_get_vertex_vec4(&mesh->vertex_color, ix);
                            v.color = glm::vec3(col.x, col.y, col.z);
                        }
                        
                        // Weights
                        // ix is the control point index (index into vertex_position), which matches cluster indices.
                        if (vertexWeights.count((int)ix)) {
                            auto& weights = vertexWeights[(int)ix];
                            // Sort by weight descending
                            std::sort(weights.begin(), weights.end(), [](const auto& a, const auto& b){ return a.second > b.second; });
                            
                            for (int w = 0; w < std::min((int)weights.size(), MAX_BONE_INFLUENCE); w++) {
                                v.m_BoneIDs[w] = weights[w].first;
                                v.m_Weights[w] = weights[w].second;
                            }
                        }

                        vertices.push_back(v);
                        indices.push_back((unsigned int)(indexOffset + vertices.size() - 1));
                    }
                }
            }
            // indexOffset += ... (Tek dongude hallettik, indisler surekli artiyor)
        }
    }

    ufbx_free_scene(scene);

    if (vertices.empty()) {
        std::cerr << "FBX loaded but no mesh data found: " << path << "\n";
        return nullptr;
    }

    // std::cout << "Loaded FBX: " << path << " (" << vertices.size() << " vertices)" << std::endl;
    return new Mesh(vertices, indices);
}



void Mesh::UpdateVertices() {
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    // Note: glBufferSubData is faster than glBufferData for updates, assuming size hasn't changed.
    // If we were rescheduling the size, we'd need glBufferData. For simple deformation, size is const.
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_Vertices.size() * sizeof(Vertex), m_Vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Mesh::RecalculateNormals() {
    // 1. Reset normals
    for (auto& v : m_Vertices) {
        v.normal = glm::vec3(0.0f);
    }

    // 2. Accumulate face normals
    // Assumes GL_TRIANGLES
    for (size_t i = 0; i < m_Indices.size(); i += 3) {
        if (i + 2 >= m_Indices.size()) break;

        unsigned int i0 = m_Indices[i];
        unsigned int i1 = m_Indices[i + 1];
        unsigned int i2 = m_Indices[i + 2];

        // Bounds check
        if (i0 >= m_Vertices.size() || i1 >= m_Vertices.size() || i2 >= m_Vertices.size()) continue;

        glm::vec3 v0 = m_Vertices[i0].position;
        glm::vec3 v1 = m_Vertices[i1].position;
        glm::vec3 v2 = m_Vertices[i2].position;

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 normal = glm::cross(edge1, edge2);

        // We don't normalize 'normal' yet -> this weights the vertex normal by the triangle area
        // (large triangles contribute more to the average normal)
        
        m_Vertices[i0].normal += normal;
        m_Vertices[i1].normal += normal;
        m_Vertices[i2].normal += normal;
    }

    // 3. Normalize updated normals
    for (auto& v : m_Vertices) {
        if (glm::length(v.normal) > std::numeric_limits<float>::epsilon()) {
            v.normal = glm::normalize(v.normal);
        }
    }
}

} // namespace Archura
