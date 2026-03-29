#include "viewer/sphere_mesh.h"
#include <cmath>

SphereMesh::SphereMesh() {}

SphereMesh::~SphereMesh()
{
    if (ebo)
        glDeleteBuffers(1, &ebo);
    if (vbo)
        glDeleteBuffers(1, &vbo);
    if (vao)
        glDeleteVertexArrays(1, &vao);
}

void SphereMesh::build(float radius, int seg, int rings)
{
    std::vector<float> vertices; // pos + normal
    std::vector<unsigned int> indices;

    for (int y = 0; y <= rings; y++)
    {
        float v = (float)y / rings;
        float phi = v * M_PI;

        for (int x = 0; x <= seg; x++)
        {
            float u = (float)x / seg;
            float theta = u * 2.0f * M_PI;

            float px = radius * sin(phi) * cos(theta);
            float py = radius * cos(phi);
            float pz = radius * sin(phi) * sin(theta);

            float nx = px / radius;
            float ny = py / radius;
            float nz = pz / radius;

            vertices.push_back(px);
            vertices.push_back(py);
            vertices.push_back(pz);

            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    for (int y = 0; y < rings; y++)
    {
        for (int x = 0; x < seg; x++)
        {

            int i0 = y * (seg + 1) + x;
            int i1 = i0 + 1;
            int i2 = i0 + (seg + 1);
            int i3 = i2 + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    indexCount = indices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(),
                 GL_STATIC_DRAW);

    // positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // normals
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void SphereMesh::draw() const
{
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
