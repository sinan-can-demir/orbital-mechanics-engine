#version 450

layout(location = 0) in vec3 inPosition;

// Visible to both VERTEX and FRAGMENT stages in this pipeline's layout
// (unlike the sphere's push constants, which only the vertex stage needs) —
// the fragment shader reads .color below.
layout(push_constant) uniform PushConstants {
    mat4 vp;
    vec4 color;
} pc;

void main() {
    gl_Position = pc.vp * vec4(inPosition, 1.0);
}
