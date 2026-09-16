#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 0) out vec4 outColor;

void main() {
    // Real lighting is #73's job — for now, just visualize the interpolated
    // normal as a color (mapped from [-1, 1] to [0, 1]). This is a cheap,
    // standard way to confirm a mesh is genuinely 3D and curved (you'll see
    // color vary continuously across the sphere's surface) without writing
    // any actual shading math yet.
    outColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);
}
