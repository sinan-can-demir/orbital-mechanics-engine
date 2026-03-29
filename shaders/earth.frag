#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 lightDir;   // normalized, passed from CPU
uniform vec3 baseColor;

void main() {
    vec3 N = normalize(vNormal);   // define N from the varying
    vec3 L = normalize(lightDir);  // define L from the uniform

    float diff = max(dot(N, L), 0.0);
    float ambient = 0.5;

    FragColor = vec4(baseColor * (ambient + diff), 1.0);
}