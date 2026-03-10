#version 410 core
in vec3 vNormal;
in vec3 vFragPos;
in vec2 vUV;
in vec4 vFragPosLightSpace;

uniform sampler2D uShadowMap;
uniform vec3  uLightPos;
uniform vec3  uLightColor;
uniform vec3  uObjectColor;
uniform vec3  uCameraPos;
uniform float uAmbientStrength;
uniform float uSpecularStrength;

out vec4 fragColor;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 norm, vec3 lightDir) {

    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Outside light frustum — no shadow
    if (projCoords.z > 1.0) return 0.0;

    float closestDepth = texture(uShadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    // Bias to prevent shadow acne
    float bias = max(0.0005 * (1.0 - dot(norm, lightDir)), 0.0003);

    // PCF — sample surrounding texels for soft shadow edges
    float shadow     = 0.0;
    vec2  texelSize  = 1.0 / textureSize(uShadowMap, 0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = texture(uShadowMap,
                             projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;  // Average 9 samples

    return shadow;
}

void main() {
    vec3 norm     = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);

    // Ambient
    vec3 ambient = uAmbientStrength * uLightColor;

    // Diffuse
    float diff   = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;

    // Specular
    vec3  viewDir    = normalize(uCameraPos - vFragPos);
    vec3  reflectDir = reflect(-lightDir, norm);
    float spec       = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3  specular   = uSpecularStrength * spec * uLightColor;

    // Shadow — attenuates diffuse and specular but not ambient
    float shadow = ShadowCalculation(vFragPosLightSpace, norm, lightDir);

    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * uObjectColor;
    fragColor     = vec4(lighting, 1.0);
    
}