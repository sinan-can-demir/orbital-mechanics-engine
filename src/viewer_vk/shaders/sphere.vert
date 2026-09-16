#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

// xyz = this body's world-space position, w = its radius. Packed into a
// single vec4 (rather than a separate vec3 + float) specifically to dodge a
// classic Vulkan/GLSL gotcha: a lone vec3 in a push-constant/uniform block
// still reserves a 16-byte-aligned slot, but exactly how a *following*
// scalar packs against it is easy to get subtly wrong between the C++
// struct and the GLSL block. A vec4 has no such ambiguity on either side.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 worldOffsetAndRadius;
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragWorldPos;

void main() {
    vec3 worldOffset = pc.worldOffsetAndRadius.xyz;
    float radius = pc.worldOffsetAndRadius.w;

    // The CPU builds mvp from the same translate(worldOffset) * scale(radius)
    // model transform applied here, so gl_Position and fragWorldPos must
    // compute it identically to stay in sync.
    fragWorldPos = inPosition * radius + worldOffset;
    gl_Position = pc.mvp * vec4(inPosition, 1.0);

    // Translation + *uniform* scale only (no rotation, no non-uniform
    // scale) — neither operation rotates or skews a normal, only its
    // length, and we normalize in the fragment shader anyway. So the
    // object-space normal is already correct in world space; a
    // general-purpose renderer would instead need mat3(transpose(inverse(model))).
    fragNormal = inNormal;
}
