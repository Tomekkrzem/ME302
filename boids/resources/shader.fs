#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;

uniform bool uUseSolidColor;
uniform vec4 uSolidColor;

out vec4 FragColor;

void main()
{   
    if (uUseSolidColor) {
        FragColor = uSolidColor; // or whatever your output variable is
        return;
    }
    // Simple directional light + ambient (no textures)
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.2, 0.2, 0.2));
    float ndotl = max(dot(N, L), 0.0);

    vec3 ambient = vec3(0.3);
    vec3 diffuse = vec3(0.75) * ndotl;

    FragColor = vec4(ambient + diffuse, 1.0);
}