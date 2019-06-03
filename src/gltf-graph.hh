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

#include "glad/include/glad/glad.h"

struct gltf_node {
  ~gltf_node();

  /// A node can be a mesh, or a bone
  enum class node_type { mesh, bone };
  const node_type type;

  /// Store the local (at load time) and world (to display) matrices.
  glm::mat4 local_xform, world_xform;

  /// List of all children node of this one
  std::vector<std::shared_ptr<gltf_node>> children;

  /// Index of that node in the glTF asset
  int gltf_model_node_index;

  /// Pointer to the parent
  gltf_node *parent;

  /// Construct a node, give it it's node type, and a parent
  gltf_node(node_type t, gltf_node *p = nullptr);

  /// Construct and attach a bone
  void add_child_bone(glm::mat4 local_xform);

  /// Get `this` easily
  gltf_node *get_ptr();

  /// Find node with set glTF index in children
  gltf_node *get_node_with_index(int index);

  /// Variables animated on this node.
  struct animation_state {
    glm::vec3 translation;
    glm::vec3 scale;
    glm::quat rotation;

    // TODO sore here arrays of weights for each morph targets?
    std::vector<float> blend_weights;

    // Sets default for a "neutral pose"
    animation_state()
        : translation(glm::vec3(0.f, 0.f, 0.f)),
          scale(glm::vec3(1.f, 1.f, 1.f)),
          rotation(1.f, 0.f, 0.f, 0.f) {}
  } pose;
};

void update_mesh_skeleton_graph_transforms(
    gltf_node &node, glm::mat4 parent_matrix = glm::mat4(1.f));

// skeleton_index is the first node that will be added as a child of
// "graph_root" e.g: a gltf node has a mesh. That mesh has a skin, and that
// skin as a node index as "skeleton". You need to pass that "skeleton"
// integer to this function as skeleton_index. This returns a flat array of
// the bones to be used by the skinning code
void populate_gltf_skeleton_subgraph(const tinygltf::Model &model,
                                     gltf_node &graph_root, int skeleton_index);

// TODO use this snipet in a fragment shader to draw a cirle instead of a
// square vec2 coord = gl_PointCoord - vec2(0.5);  //from [0,1] to
// [-0.5,0.5] if(length(coord) > 0.5)                  //outside of circle
// radius?
//    discard;
void draw_bones(gltf_node &root, GLuint shader, glm::mat4 view_matrix,
                glm::mat4 projection_matrix);

void create_flat_bone_array(gltf_node &root,
                            std::vector<gltf_node *> &flat_array);

void sort_bone_array(std::vector<gltf_node *> &bone_array,
                     const tinygltf::Skin &skin_object);

void create_flat_bone_list(const tinygltf::Skin &skin,
                           const std::vector<int>::size_type nb_joints,
                           gltf_node mesh_skeleton_graph,
                           std::vector<gltf_node *> &flatened_bone_list);

// This is useful because mesh.skeleton isn't required to point to the skeleton
// root. We still want to find the skeleton root, so we are going to search for
// it by hand.
int find_skeleton_root(const tinygltf::Model &model,
                       const std::vector<int> &joints, int start_node = 0);

void bone_display_window();
