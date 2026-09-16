#version 450

// Real vertex attributes this time, read from the vertex buffer instead of
// a hardcoded shader-local array — location numbers here must match the
// VkVertexInputAttributeDescription offsets set up in createGraphicsPipeline().
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

// Push constants: a small block of data written straight into the command
// buffer each frame (vkCmdPushConstants), instead of a full uniform-buffer +
// descriptor-set setup. A 4x4 matrix is 64 bytes, comfortably inside the
// typical 128-byte push-constant budget, so this sidesteps descriptor sets
// entirely for this milestone.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 fragNormal;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    // Passing the *object-space* normal through unTransformed is only
    // correct because our model matrix is a pure rotation (no non-uniform
    // scale) — a general renderer would transform normals by the inverse
    // transpose of the model matrix instead.
    fragNormal = inNormal;
}
