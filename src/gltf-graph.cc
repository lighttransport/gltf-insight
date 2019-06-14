#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "gltf-graph.hh"

#include "gl_util.hh"

bool draw_joint_point = true;
bool draw_bone_segment = true;
bool draw_mesh_anchor_point = true;
bool draw_bone_axes = true;

gltf_node::~gltf_node() = default;

gltf_node::gltf_node(node_type t, gltf_node* p)
    : type(t),
      local_xform(1.f),
      world_xform(1.f),
      gltf_model_node_index(-1),
      parent(p) {}

void gltf_node::add_child_bone(glm::mat4 local_xform) {
  gltf_node* child = new gltf_node(node_type::bone);
  child->local_xform = local_xform;
  child->parent = this;
  children.emplace_back(child);
}

gltf_node* gltf_node::get_ptr() { return this; }

gltf_node* find_index_in_children(gltf_node* node, int index) {
  if (node->gltf_model_node_index == index) return node;

  gltf_node* found = nullptr;
  for (auto child : node->children) {
    found = find_index_in_children(child->get_ptr(), index);
    if (found) return found;
  }

  return nullptr;
}

gltf_node* gltf_node::get_node_with_index(int index) {
  auto* node = find_index_in_children(this, index);

  if (node) {
    assert(node->gltf_model_node_index == index);
  }
  return node;
}

void update_mesh_skeleton_graph_transforms(gltf_node& node,
                                           glm::mat4 parent_matrix) {
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

  node.world_xform = parent_matrix * node.local_xform *
                     (node.type != gltf_node::node_type::mesh
                          ? glm::inverse(node.local_xform) * pose_matrix
                          : glm::mat4(1.f));

  // recursively call itself until you reach a node with no children
  for (auto& child : node.children)
    update_mesh_skeleton_graph_transforms(*child, node.world_xform);
}

// skeleton_index is the first node that will be added as a child of
// "graph_root" e.g: a gltf node has a mesh. That mesh has a skin, and that
// skin as a node index as "skeleton". You need to pass that "skeleton"
// integer to this function as skeleton_index. This returns a flat array of
// the bones to be used by the skinning code
void populate_gltf_skeleton_subgraph(const tinygltf::Model& model,
                                     gltf_node& graph_root,
                                     int skeleton_index) {
  const auto& skeleton_node = model.nodes[skeleton_index];

  // Holder for the data.
  glm::mat4 xform(1.f);
  glm::vec3 translation(0.f), scale(1.f, 1.f, 1.f);
  glm::quat rotation(1.f, 0.f, 0.f, 0.f);

  // A node can store both a transform matrix and separate translation, rotation
  // and scale. We need to load them if they are present. tiny_gltf signal this
  // by either having empty vectors, or vectors of the expected size.
  const auto& node_matrix = skeleton_node.matrix;
  if (node_matrix.size() == 16)  // 4x4 matrix
  {
    double tmp[16];
    float tmpf[16];
    memcpy(tmp, skeleton_node.matrix.data(), 16 * sizeof(double));

    // Convert a double array to a float array
    for (int i = 0; i < 16; ++i) {
      tmpf[i] = float(tmp[i]);
    }

    // Both glm matrices and this float array have the same data layout. We can
    // pass the pointer to glm::make_mat4
    xform = glm::make_mat4(tmpf);
  }

  // Do the same for translation rotation and scale.
  const auto& node_translation = skeleton_node.translation;
  if (node_translation.size() == 3)  // 3D vector
  {
    for (glm::vec3::length_type i = 0; i < 3; ++i)
      translation[i] = float(node_translation[i]);
  }

  const auto& node_scale = skeleton_node.scale;
  if (node_scale.size() == 3)  // 3D vector
  {
    for (glm::vec3::length_type i = 0; i < 3; ++i)
      scale[i] = float(node_scale[i]);
  }

  const auto& node_rotation = skeleton_node.rotation;
  if (node_rotation.size() == 4)  // Quaternion
  {
    rotation.w = float(node_rotation[3]);
    rotation.x = float(node_rotation[0]);
    rotation.y = float(node_rotation[1]);
    rotation.z = float(node_rotation[2]);
    glm::normalize(rotation);  // Be prudent
  }

  const glm::mat4 rotation_matrix = glm::toMat4(rotation);
  const glm::mat4 translation_matrix =
      glm::translate(glm::mat4(1.f), translation);
  const glm::mat4 scale_matrix = glm::scale(glm::mat4(1.f), scale);
  const glm::mat4 reconstructed_matrix =
      translation_matrix * rotation_matrix * scale_matrix;

  // In the case that the node has both the matrix and the individual vectors,
  // both transform has to be applied.
  xform = xform * reconstructed_matrix;

  graph_root.add_child_bone(xform);
  auto& new_bone = *graph_root.children.back().get();
  new_bone.gltf_model_node_index = skeleton_index;

  for (int child : skeleton_node.children) {
    populate_gltf_skeleton_subgraph(model, new_bone, child);
  }
}

// TODO use this snipet in a fragment shader to draw a cirle instead of a
// square vec2 coord = gl_PointCoord - vec2(0.5);  //from [0,1] to
// [-0.5,0.5] if(length(coord) > 0.5)                  //outside of circle
// radius?
//    discard;
void draw_bones(gltf_node& root, GLuint shader, glm::mat4 view_matrix,
                glm::mat4 projection_matrix) {
  glUseProgram(shader);
  glm::mat4 mvp = projection_matrix * view_matrix * root.world_xform;
  glUniformMatrix4fv(glGetUniformLocation(shader, "mvp"), 1, GL_FALSE,
                     glm::value_ptr(mvp));

  const float line_width = 2;
  const float axis_scale = 0.125;

  if (draw_bone_axes) draw_space_base(shader, line_width, axis_scale);

  for (auto& child : root.children) {
    if (root.type != gltf_node::node_type::mesh && draw_bone_segment) {
      glUseProgram(shader);
      glLineWidth(8);
      glUniform4f(glGetUniformLocation(shader, "debug_color"), 0, 0.5, 0.5, 1);
      glBegin(GL_LINES);
      glVertex4f(0, 0, 0, 1);
      const auto child_position = child->local_xform[3];
      glVertex4f(child_position.x, child_position.y, child_position.z, 1);
      glEnd();
    }
    draw_bones(*child, shader, view_matrix, projection_matrix);
  }

  glUseProgram(shader);
  glUniformMatrix4fv(glGetUniformLocation(shader, "mvp"), 1, GL_FALSE,
                     glm::value_ptr(mvp));
  if (draw_joint_point && root.type == gltf_node::node_type::bone) {
    glUniform4f(glGetUniformLocation(shader, "debug_color"), 1, 0, 0, 1);

    draw_space_origin_point(10);
  } else if (draw_mesh_anchor_point &&
             root.type == gltf_node::node_type::mesh) {
    glUniform4f(glGetUniformLocation(shader, "debug_color"), 1, 1, 0, 1);
    draw_space_origin_point(10);
  }
  glUseProgram(0);
}

void create_flat_bone_array(gltf_node& root,
                            std::vector<gltf_node*>& flat_array) {
  if (root.type == gltf_node::node_type::bone)
    flat_array.push_back(root.get_ptr());

  for (auto& child : root.children) create_flat_bone_array(*child, flat_array);
}

void sort_bone_array(std::vector<gltf_node*>& bone_array,
                     const tinygltf::Skin& skin_object) {
  // assert(bone_array.size() == skin_object.joints.size());
  for (size_t counter = 0;
       counter < std::min(bone_array.size(), skin_object.joints.size());
       ++counter) {
    int index_to_find = skin_object.joints[counter];
    for (size_t bone_index = 0; bone_index < bone_array.size(); ++bone_index) {
      if (bone_array[bone_index]->gltf_model_node_index == index_to_find) {
        std::swap(bone_array[counter], bone_array[bone_index]);
        break;
      }
    }
  }
}

void create_flat_bone_list(const tinygltf::Skin& skin,
                           const std::vector<int>::size_type nb_joints,
                           gltf_node mesh_skeleton_graph,
                           std::vector<gltf_node*>& flatened_bone_list) {
  create_flat_bone_array(mesh_skeleton_graph, flatened_bone_list);
  sort_bone_array(flatened_bone_list, skin);
}

// This is useful because mesh.skeleton isn't required to point to the skeleton
// root. We still want to find the skeleton root, so we are going to search for
// it by hand.
int find_skeleton_root(const tinygltf::Model& model,
                       const std::vector<int>& joints, int start_node) {
  // Get the node to get the children
  const auto& node = model.nodes[start_node];

  for (int child : node.children) {
    // If we are part of the skeleton, return our parent
    for (int joint : joints) {
      if (joint == child) return joint;
    }
    // try to find in children, if found return the child's child's parent (so,
    // here, the retunred value by find_skeleton_root)
    const int result = find_skeleton_root(model, joints, child);
    if (result != -1) return result;
  }

  // Here it means we found nothing, return "nothing"
  return -1;
}

#include <imgui.h>
void bone_display_window() {
  if (ImGui::Begin("Skeleton drawing options")) {
    ImGui::Checkbox("Draw joint points", &draw_joint_point);
    ImGui::Checkbox("Draw Bone as segments", &draw_bone_segment);
    ImGui::Checkbox("Draw Bone's base axes", &draw_bone_axes);
    ImGui::Checkbox("Draw Skeleton's Mesh base", &draw_mesh_anchor_point);
  }
  ImGui::End();
}
