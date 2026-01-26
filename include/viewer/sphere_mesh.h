#ifndef SPHERE_MESH_H
#define SPHERE_MESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class SphereMesh {
public:
  SphereMesh();
  ~SphereMesh();

  void build(float radius, int segments, int rings);

  void draw() const;

private:
  GLuint vao = 0;
  GLuint vbo = 0;
  GLuint ebo = 0;
  GLsizei indexCount = 0;
};

#endif // SPHERE_MESH_H
