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

#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/matrix.hpp>
#include <memory>
#include <vector>

#include "animation.hh"
#include "glad/include/glad/glad.h"

struct gltf_node {
  ~gltf_node();

  /// A node can be a mesh, or a bone, or can just be empty.
  ///
  /// A empty node can serve to just hold a transform
  enum class node_type { mesh, bone, empty };
  node_type type;

  /// Store the local (at load time) and world (to display) matrices.
  glm::mat4 local_xform, world_xform;

  /// List of all children node of this one
  std::vector<std::shared_ptr<gltf_node>> children;

  /// Index of that node in the glTF asset
  int gltf_node_index = -1;

  int gltf_mesh_id = -1;

  /// Pointer to the parent
  gltf_node* parent;

  /// Construct a node, give it it's node type, and a parent
  gltf_node(node_type t, gltf_node* p = nullptr);

  /// Construct and attach a bone
  void add_child();

  /// Get `this` easily
  gltf_node* get_ptr();

  /// Find node with set glTF index in children
  gltf_node* get_node_with_index(int index);

  /// Variables animated on this node.
  struct animation_state {
    glm::vec3 translation;
    glm::vec3 scale;
    glm::quat rotation;

    // TODO sore here arrays of weights for each morph targets?
    std::vector<float> blend_weights;
    std::vector<std::string> target_names;

    // Sets default for a "neutral pose"
    animation_state()
        : translation(glm::vec3(0.f, 0.f, 0.f)),
          scale(glm::vec3(1.f, 1.f, 1.f)),
          rotation(1.f, 0.f, 0.f, 0.f) {}
  } pose;
};

void update_mesh_skeleton_graph_transforms(
    gltf_node& node, glm::mat4 parent_matrix = glm::mat4(1.f));

void populate_gltf_graph(const tinygltf::Model& model, gltf_node& graph_root,
                         int gltf_index);

void set_mesh_attachement(const tinygltf::Model& model, gltf_node& graph_root);

struct gltf_mesh_instance {
  int node;
  int mesh;
};
std::vector<gltf_mesh_instance> get_list_of_mesh_instances(
    const gltf_node& root);

inline void empty_gltf_graph(gltf_node& graph_root) {
  for (auto& child : graph_root.children) empty_gltf_graph(*child);

  graph_root.children.clear();  // shared pointers should go to 0 references,
                                // only node that will survive is the root one
}

// TODO use this snipet in a fragment shader to draw a cirle instead of a
// square vec2 coord = gl_PointCoord - vec2(0.5);  //from [0,1] to
// [-0.5,0.5] if(length(coord) > 0.5)                  //outside of circle
// radius?
//    discard;
void draw_bones(gltf_node& root, int active_joint_node_index, GLuint shader,
                glm::mat4 view_matrix, glm::mat4 projection_matrix);

void create_flat_bone_array(gltf_node& root,
                            std::vector<gltf_node*>& flat_array,
                            const std::vector<int>& skin_joints);

void sort_bone_array(std::vector<gltf_node*>& bone_array,
                     const tinygltf::Skin& skin_object);

void create_flat_bone_list(const tinygltf::Skin& skin,
                           const std::vector<int>::size_type nb_joints,
                           gltf_node mesh_skeleton_graph,
                           std::vector<gltf_node*>& flatened_bone_list);

// This is useful because mesh.skeleton isn't required to point to the skeleton
// root. We still want to find the skeleton root, so we are going to search for
// it by hand.
int find_skeleton_root(const tinygltf::Model& model,
                       const std::vector<int>& joints, int start_node = 0);

void bone_display_window(bool* open = nullptr);
glm::mat4 load_node_local_xform(const tinygltf::Node& node);
