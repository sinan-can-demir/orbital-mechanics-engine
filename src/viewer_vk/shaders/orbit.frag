#version 450

layout(push_constant) uniform PushConstants {
    mat4 vp;
    vec4 color;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    // Flat, unlit color — orbit trails are reference lines, not lit
    // geometry, matching the OpenGL viewer's separate (unlit) orbit shader.
    outColor = vec4(pc.color.rgb, 1.0);
}
