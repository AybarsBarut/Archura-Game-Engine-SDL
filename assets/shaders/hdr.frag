#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D uHDRTexture;   // Ham HDR render sonucu
uniform sampler2D uBloomTexture; // Gaussian blur'dan gelen bloom
uniform float     uExposure;     // Pozlama (varsayilan 1.0)
uniform float     uBloomStrength;// Bloom katki katsayisi (0..1)
uniform bool      uHDREnabled;   // HDR acik/kapali

// ACES filmic tone mapping (Hollywood standardı)
vec3 ACESFilmic(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard tone mapping (daha yumusak)
vec3 Reinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

void main() {
    vec3 hdrColor   = texture(uHDRTexture, TexCoords).rgb;
    vec3 bloomColor = texture(uBloomTexture, TexCoords).rgb;

    // Bloom mix
    hdrColor += bloomColor * uBloomStrength;

    vec3 result;
    if (uHDREnabled) {
        // Pozlama + ACES tone map
        vec3 mapped = ACESFilmic(hdrColor * uExposure);
        // The active backbuffer path uses legacy display-space output. Apply
        // the transfer function here instead of relying on global framebuffer
        // conversion, which would also brighten ImGui and unlinearized passes.
        result = pow(mapped, vec3(1.0 / 2.2));
    } else {
        result = clamp(hdrColor, 0.0, 1.0);
    }

    FragColor = vec4(result, 1.0);
}
