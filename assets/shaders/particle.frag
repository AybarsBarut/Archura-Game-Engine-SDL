#version 330 core

in vec4 vColor;
in vec2 vTexCoords;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform bool      uUseTexture;

void main() {
    vec4 base = uUseTexture ? texture(uTexture, vTexCoords) : vec4(1.0);
    FragColor  = base * vColor;

    // Tamamen seffaf parcaciklari kes (performans icin)
    if (FragColor.a < 0.01) discard;
}
