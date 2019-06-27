/*
MIT License

Copyright (c) 2019 Light Transport Entertainment Inc. And many contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <vector>

#include "gl_util.hh"
#include "gltf-graph.hh"

struct morph_target {
  // std::string name;
  // TODO having acces to the name of a morph target would be nice. BUT glTF 2.0
  // doesn't define this.
  // See: https://github.com/KhronosGroup/glTF/issues/1036

  std::vector<float> position, normal;
};

void load_animations(const tinygltf::Model& model,
                     std::vector<animation>& animations);

void load_geometry(const tinygltf::Model& model, std::vector<GLuint>& textures,
                   const std::vector<tinygltf::Primitive>& primitives,
                   std::vector<draw_call_submesh>& draw_call_descriptor,
                   std::vector<GLuint>& VAOs,
                   std::vector<std::array<GLuint, 7>>& VBOs,
                   std::vector<std::vector<unsigned>>& indices,
                   std::vector<std::vector<float>>& vertex_coord,
                   std::vector<std::vector<float>>& texture_coord,
                   std::vector<std::vector<float>>& normals,
                   std::vector<std::vector<float>>& weights,
                   std::vector<std::vector<unsigned short>>& joints);

void load_morph_targets(const tinygltf::Model& model,
                        const tinygltf::Primitive& primitive,
                        std::vector<morph_target>& morph_targets,
                        bool& has_normals, bool& has_tangents);

void load_morph_target_names(const tinygltf::Mesh& mesh,
                             std::vector<std::string>& names);

void load_inverse_bind_matrix_array(
    tinygltf::Model model, const tinygltf::Skin& skin, size_t nb_joints,
    std::vector<glm::mat4>& inverse_bind_matrices);
