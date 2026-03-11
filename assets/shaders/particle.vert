#version 330 core

// Per-vertex data (bir quad / billboard icin sadece köşeler)
layout (location = 0) in vec3 aPos;  // Yerel konum (billboard quad vertices)
layout (location = 2) in vec2 aTexCoords;

// Per-instance data (her parcacik icin)
layout (location = 4) in vec3  aInstPos;        // Dunya konumu
layout (location = 5) in vec4  aInstColor;      // Renk + alpha
layout (location = 6) in float aInstSize;       // Olcek
layout (location = 7) in float aInstLifeFrac;   // Kalan omur orani [0..1]

uniform mat4 uView;
uniform mat4 uProjection;

out vec4 vColor;
out vec2 vTexCoords;

void main() {
    // Billboard: View matrisinin rotasyonunu iptal ederek ekrana bakan quad yap
    // Camera'nin sag ve yukari vektorlerini view matrisinden al
    vec3 camRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 camUp    = vec3(uView[0][1], uView[1][1], uView[2][1]);

    // Quad vertex'ini dunya uzayina tasima
    vec3 worldPos = aInstPos
                  + camRight * aPos.x * aInstSize
                  + camUp    * aPos.y * aInstSize;

    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
    
    // Rengi yasam oranina gore soldur (alpha fade-out)
    vColor = vec4(aInstColor.rgb, aInstColor.a * aInstLifeFrac);
    vTexCoords = aTexCoords;
}
