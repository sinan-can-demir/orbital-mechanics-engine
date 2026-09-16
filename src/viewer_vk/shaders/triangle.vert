#version 450

// No vertex buffer yet (that's #72, once VMA is wired up for GPU memory
// allocation) — for this milestone the triangle's geometry is baked directly
// into the shader as a small lookup table, indexed by gl_VertexIndex, which
// Vulkan fills in automatically for each of the 3 vertices in our draw call.
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

// One color per vertex. The fragment shader only receives *interpolated*
// values across the triangle's face — the actual blending between red,
// green, and blue is done by fixed-function hardware during rasterization,
// not by any code we write.
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

// "location = 0" is an arbitrary slot number — it just has to match the
// fragment shader's "layout(location = 0) in" for this value to connect.
layout(location = 0) out vec3 fragColor;

void main() {
    // gl_Position is a built-in output every vertex shader must write: clip
    // space coordinates (x, y, z, w). We're not using perspective or a camera
    // yet, so w = 1.0 and z = 0.0 (2D on a flat plane facing the viewer).
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
