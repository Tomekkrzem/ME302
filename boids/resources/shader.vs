#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uViewProj;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vWorldPos;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    mat3 normalMat = mat3(transpose(inverse(uModel)));
    vNormal = normalize(normalMat * aNormal);

    gl_Position = uViewProj * worldPos;
}