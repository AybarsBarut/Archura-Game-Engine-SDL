#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 Color;

out vec4 FragColor;

// Texture
uniform sampler2D uTexture;
uniform bool uUseTexture;

// Lighting
struct Light {
    vec3  position;
    vec3  direction;
    vec3  color;
    float intensity;
    float range;
    int   type; // 0 = Directional, 1 = Point, 2 = Ambient/Circle
};

#define MAX_LIGHTS 4
uniform Light uLights[MAX_LIGHTS];
uniform int   uLightCount;

uniform vec3  uViewPos;

// Material
uniform vec3  uAmbient;
uniform vec3  uDiffuse;
uniform vec3  uSpecular;
uniform float uShininess;

// Shadow map
uniform sampler2DShadow uShadowMap;
uniform mat4      uLightSpaceMatrix;

// ── Shadow PCF 3x3 with normal-based bias ────────────────────────────────────
float ShadowCalculation(vec4 fragPosLS, vec3 norm, vec3 lightDir) {
    if (fragPosLS.w <= 0.0) return 0.0;
    vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Outside ortho frustum → fully lit
    if (projCoords.z <= 0.0 || projCoords.z >= 1.0 ||
        any(lessThan(projCoords.xy, vec2(0.0))) ||
        any(greaterThan(projCoords.xy, vec2(1.0)))) return 0.0;

    float currentDepth = projCoords.z;

    // Normal-based bias: grazing angles get more bias to prevent acne
    float cosTheta = max(dot(norm, lightDir), 0.0);
    float bias = max(0.0015 * (1.0 - cosTheta), 0.00025);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float visibility = texture(uShadowMap,
                vec3(projCoords.xy + vec2(x, y) * texelSize,
                     currentDepth - bias));
            shadow += 1.0 - visibility;
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 baseColor = uUseTexture ? texture(uTexture, TexCoords).rgb : Color;

    vec3 norm    = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);

    // Ambient floor – C++ controls uAmbient but we guarantee a minimum of 0.15
    // so walls never go completely black regardless of light position.
    vec3 totalLight = max(uAmbient, vec3(0.15));

    // Guard shininess: pow(x, 0) = 1 if x>=0, but a zero / undefined uniform
    // from C++ can still cause NaN/full-bright specular.
    float shininess = max(uShininess, 1.0);

    for (int i = 0; i < uLightCount; i++) {
        Light L = uLights[i];

        // Ambient / everywhere light ──────────────────────────────────────────
        if (L.type == 2) {
            totalLight += L.color * L.intensity;
            continue;
        }

        vec3  lightDir    = vec3(0.0);
        float attenuation = 1.0;
        float shadow      = 0.0;

        // Directional ─────────────────────────────────────────────────────────
        if (L.type == 0) {
            // L.direction is the Forward vector of the light entity (away from source)
            // → flip to get "towards light"
            lightDir = normalize(-L.direction);
            vec4 fragPosLS = uLightSpaceMatrix * vec4(FragPos, 1.0);
            shadow = ShadowCalculation(fragPosLS, norm, lightDir);
        }
        // Point ───────────────────────────────────────────────────────────────
        else if (L.type == 1) {
            vec3  toLight = L.position - FragPos;
            float dist    = length(toLight);
            lightDir = toLight / max(dist, 0.0001);

            if (dist >= L.range) continue; // fully outside range

            // Smooth quadratic falloff: 1 at center, 0 at range edge
            float t = dist / L.range;
            attenuation = clamp(1.0 - t * t, 0.0, 1.0);
        }

        // Diffuse (Lambert) ───────────────────────────────────────────────────
        float diff    = max(dot(norm, lightDir), 0.0);
        vec3  diffuse = uDiffuse * diff * L.color * L.intensity;

        // Specular (Blinn-Phong) ──────────────────────────────────────────────
        vec3  halfDir = normalize(lightDir + viewDir);
        float spec    = pow(max(dot(norm, halfDir), 0.0), shininess);
        vec3  specular = uSpecular * spec * L.color * L.intensity;

        totalLight += (diffuse + specular) * attenuation * (1.0 - shadow);
    }

    vec3 result = clamp(totalLight, 0.0, 1.0) * baseColor;
    FragColor = vec4(result, 1.0);
}
