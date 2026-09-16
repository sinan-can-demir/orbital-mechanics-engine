#version 450

// Runs once per *pixel* covered by the triangle. fragColor here is not one
// of the 3 original vertex colors — it's whatever the rasterizer
// interpolated for this exact pixel's position, which is what produces the
// smooth red/green/blue blend across the triangle's face.
layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
