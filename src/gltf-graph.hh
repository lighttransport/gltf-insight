#pragma once

#include <glm/matrix.hpp>
#include <memory>
#include <vector>

struct gltf_node {
  ~gltf_node();

  enum class node_type { mesh, bone };
  const node_type type;
  glm::mat4 local_xform, world_xform;
  std::vector<std::shared_ptr<gltf_node>> children;
  int gltf_model_node_index;
  gltf_node* parent;

  gltf_node(node_type t, gltf_node* p = nullptr);

  void add_child_bone(glm::mat4 local_xform);

  gltf_node* get_ptr();

  gltf_node* get_node_with_index(int index);
};

static void update_subgraph_transform(
    gltf_node& node, glm::mat4 parent_matrix = glm::mat4(1.f)) {
  node.world_xform = parent_matrix * node.local_xform;
  for (auto& child : node.children)
    update_subgraph_transform(*child, node.world_xform);
}
