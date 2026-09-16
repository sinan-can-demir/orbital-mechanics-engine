#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;

// A real GPU buffer this time, not push constants: its *layout* (this block)
// and *binding slot* (set 0, binding 0) are declared upfront via the
// descriptor set layout, then connected to an actual VkBuffer per draw call
// by binding a descriptor set — replacing OpenGL's "call glUniform3fv
// whenever" with a declare-then-bind model.
layout(set = 0, binding = 0) uniform BodyUBO {
    vec3 color;
    vec3 lightPos;
    // xyz = camera position, w = 1.0 if this body itself is the light
    // source (the Sun), 0.0 otherwise.
    vec4 viewPosAndEmissive;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    // A body that *is* the light source can't be meaningfully lit by
    // Blinn-Phong against its own position — the diffuse term is ~0
    // everywhere (the light can't illuminate a surface it's embedded in)
    // and the result is a flat, dim disc regardless of how correct the
    // lighting math is. Real Suns are self-luminous, not diffusely lit, so
    // skip the lighting model entirely and just output a bright flat color.
    if (ubo.viewPosAndEmissive.w > 0.5) {
        outColor = vec4(pow(ubo.color, vec3(1.0 / 2.2)), 1.0);
        return;
    }

    // Blinn-Phong + rim light + gamma correction, ported unchanged from the
    // OpenGL viewer's fragment shader (src/viewer/orbit_viewer.cpp) so both
    // renderers are visually comparable for the same scene.
    vec3 viewPos = ubo.viewPosAndEmissive.xyz;
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(ubo.lightPos - fragWorldPos);
    vec3 V = normalize(viewPos - fragWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    float ambient = 0.18;

    vec3 base = ubo.color * (ambient + diff) + vec3(0.4) * spec;

    float rim = pow(1.0 - max(dot(N, V), 0.0), 2.0);
    vec3 rimColor = vec3(0.3, 0.4, 0.9) * rim * 0.5;

    vec3 color = base + rimColor;
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
