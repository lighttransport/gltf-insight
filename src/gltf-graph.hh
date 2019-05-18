#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/matrix.hpp>
#include <memory>
#include <vector>

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
  gltf_node* parent;

  /// Construct a node, give it it's node type, and a parent
  gltf_node(node_type t, gltf_node* p = nullptr);

  /// Construct and attach a bone
  void add_child_bone(glm::mat4 local_xform);

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
    // std::vector<float> blend_weights;

    // Sets default for a "neutral pose"
    animation_state()
        : translation(glm::vec3(0.f, 0.f, 0.f)),
          scale(glm::vec3(1.f, 1.f, 1.f)),
          rotation(1.f, 0.f, 0.f, 0.f) {}
  } pose;
};

static void update_mesh_skeleton_graph_transforms(
    gltf_node& node, glm::mat4 parent_matrix = glm::mat4(1.f)) {
  // Calculate a matrix that apply the "animated pose" transform to the node
  glm::mat4 pose_translation =
      glm::translate(glm::mat4(1.f), node.pose.translation);
  glm::mat4 pose_rotation = glm::toMat4(node.pose.rotation);
  glm::mat4 pose_scale = glm::scale(glm::mat4(1.f), node.pose.scale);
  glm::mat4 pose_matrix = pose_translation * pose_rotation * pose_scale;

  // The calculated "pose_matrix" is actually the absolute position on the local
  // space. I prefer to keep the "binding pose" of the skeleton, and reference
  // key frames as a "delta" from these ones. If that pose_matrix is "identity"
  // it means that the node hasn't been moved.

  // Set the transform as a "delta" from the local transform
  if (glm::mat4(1.f) == pose_matrix) {
    pose_matrix = node.local_xform;
  } else {
    pose_matrix = glm::inverse(node.local_xform) * pose_matrix;
  }

  /* This will accumulate the parent/child matrices to get everything in the
   * referential of `node`
   *
   * The node's "local" transform is the natural (binding) pose of the node
   * relative to it's parent. The calculated "pose_matrix" is how much the
   * node needs to be moved in space, relative to it's parent from this
   * binding pose to be in the correct transform the animation wants it to
   * be. The parent_matrix is the "world_transform" of the parent node,
   * recusively passed down along the graph.
   *
   * The content of the node's "pose" structure used here will be updated by
   * the animation playing system in accordance to it's current clock,
   * interpolating between key frames (see class defined in animation.hh)
   */

  node.world_xform =
      parent_matrix * node.local_xform *
      (node.type != gltf_node::node_type::mesh ? pose_matrix : glm::mat4(1.f));

  // recursively call itself until you reach a node with no children
  for (auto& child : node.children)
    update_mesh_skeleton_graph_transforms(*child, node.world_xform);
}
