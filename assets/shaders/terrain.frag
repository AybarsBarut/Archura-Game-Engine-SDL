#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

out vec4 FragColor;

// Dokular (katmanlı arazi)
uniform sampler2D uTexGrass;    // Düşük yükseklik
uniform sampler2D uTexRock;     // Orta yükseklik / dik eğim
uniform sampler2D uTexSnow;     // Yüksek yükseklik
uniform float     uTexScale;    // Doku tekrar sayısı

// Işık
uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform vec3  uViewPos;

// Gölge
uniform sampler2D uShadowMap;

// Arazi yükseklik sınırları (CPU'dan gönderilir)
uniform float uMinHeight;
uniform float uMaxHeight;

// ── Gölge hesabı (PCF 3x3) ────────────────────────────────────────────────
float ShadowCalculation(vec4 fragPLS, vec3 norm, vec3 lightDir) {
    vec3 projCoords = fragPLS.xyz / fragPLS.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float cosTheta = max(dot(norm, lightDir), 0.0);
    float bias = mix(0.012, 0.002, cosTheta);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    return shadow / 9.0;
}

void main() {
    // ── Yükseklik bazlı doku karıştırma ──────────────────────────────────
    float range    = uMaxHeight - uMinHeight;
    float normH    = clamp((FragPos.y - uMinHeight) / max(range, 0.01), 0.0, 1.0);
    float slope    = 1.0 - abs(dot(normalize(Normal), vec3(0.0, 1.0, 0.0)));

    // Eğim yüksekse kaya, düşük ve yüksekte kar/çimen
    float grassW = clamp(1.0 - slope * 3.0 - normH * 2.0, 0.0, 1.0);
    float snowW  = clamp((normH - 0.6) * 3.0 - slope * 2.0, 0.0, 1.0);
    float rockW  = 1.0 - grassW - snowW;

    vec2 tc = TexCoords * uTexScale;
    vec3 grassCol = texture(uTexGrass, tc).rgb;
    vec3 rockCol  = texture(uTexRock,  tc).rgb;
    vec3 snowCol  = texture(uTexSnow,  tc).rgb;
    vec3 baseCol  = grassCol * grassW + rockCol * rockW + snowCol * snowW;

    // ── Işıklandırma (Blinn-Phong basit) ─────────────────────────────────
    vec3 norm     = normalize(Normal);
    vec3 lightD   = normalize(-uLightDir);
    float diff    = max(dot(norm, lightD), 0.0);

    float shadow  = ShadowCalculation(FragPosLightSpace, norm, lightD);
    
    vec3 ambient  = 0.25 * baseCol;
    vec3 diffuse  = diff * uLightColor * uLightIntensity * baseCol * (1.0 - shadow * 0.7);

    FragColor = vec4(ambient + diffuse, 1.0);
}
