#pragma once

#include <vector>

#include "glad/include/glad/glad.h"
#include "gltf-graph.hh"

struct draw_call_submesh {
  GLenum draw_mode;
  size_t count;
  GLuint VAO, main_texture;
};

struct morph_target {
  // std::string name;
  // TODO having acces to the name of a morph target would be nice. BUT glTF 2.0
  // doesn't define this.
  // See: https://github.com/KhronosGroup/glTF/issues/1036

  std::vector<float> position, normal /*, tangent*/;
};

void load_animations(const tinygltf::Model &model,
                     std::vector<animation> &animations);

void load_geometry(const tinygltf::Model &model, std::vector<GLuint> &textures,
                   const std::vector<tinygltf::Primitive> &primitives,
                   std::vector<draw_call_submesh> &draw_call_descriptor,
                   const std::vector<GLuint> &VAOs,
                   const std::vector<GLuint[6]> &VBOs,
                   std::vector<std::vector<unsigned>> &indices,
                   std::vector<std::vector<float>> &vertex_coord,
                   std::vector<std::vector<float>> &texture_coord,
                   std::vector<std::vector<float>> &normals,
                   std::vector<std::vector<float>> &weights,
                   std::vector<std::vector<unsigned short>> &joints);

void load_morph_targets(const tinygltf::Model &model,
                        const tinygltf::Primitive &primitive,
                        std::vector<morph_target> &morph_targets);
