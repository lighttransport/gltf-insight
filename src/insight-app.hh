#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// This includes opengl for us, along side debuging callbacks
#include "gl_util.hh"

// imgui
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

#include "animation.hh"
#include "cxxopts.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/matrix.hpp"
#include "gltf-graph.hh"
#include "gltf-loader.hh"
#include "gui_util.hh"
#include "shader.hh"
#include "tiny_gltf.h"
#include "tiny_gltf_util.h"

/// Main application class
namespace gltf_insight {

struct mesh {
  gltf_mesh_instance instance;
  std::vector<glm::mat4> joint_matrices;
  std::map<int, int> joint_inverse_bind_matrix_map;
  std::vector<gltf_node*> flat_joint_list;
  std::vector<glm::mat4> inverse_bind_matrices;
  std::vector<std::vector<morph_target>> morph_targets;
  std::vector<draw_call_submesh> draw_call_descriptors;
  std::vector<std::vector<unsigned>> indices;
  std::vector<std::vector<float>> vertex_coord, texture_coord, normals, weights,
      display_position, display_normal;
  std::vector<std::vector<unsigned short>> joints;
  int nb_morph_targets = 0;
  int nb_joints = 0;

  // OpenGL objects
  std::vector<GLuint> VAOs;
  std::vector<std::array<GLuint, 6>> VBOs;
  std::unique_ptr<std::map<std::string, shader>> shader_list;

  bool skinned = false;

  bool displayed = true;

  std::string name;

  ~mesh();

  mesh(const mesh&) = delete;
  mesh& operator=(const mesh&) = delete;

  mesh& operator=(mesh&& o) throw();
  mesh(mesh&& other) throw();
  mesh();
};

class app {
 public:
  app(int argc, char** argv);
  ~app();

  void unload();
  void load();
  void main_loop();

 private:
  ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
  ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::WORLD;

  bool show_asset_image_window = true;
  bool show_model_info_window = true;
  bool show_animation_window = true;
  bool show_mesh_display_window = true;
  bool show_morph_target_window = true;
  bool show_camera_parameter_window = true;
  bool show_transform_window = true;
  bool show_timeline = true;

  bool show_gizmo = true;

  std::vector<mesh> loaded_meshes;

  // Application state
  bool asset_loaded = false;
  bool found_textured_shader = false;

  gltf_node gltf_scene_tree{gltf_node::node_type::empty};

  ImVec4 viewport_background_color = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  tinygltf::Model model;
  tinygltf::TinyGLTF gltf_ctx;

  // display parameters
  std::vector<std::string> shader_names;
  int selected_shader = 0;
  std::string shader_to_use;
  glm::mat4 view_matrix{1.f}, projection_matrix{1.f};
  glm::mat4& model_matrix = gltf_scene_tree.local_xform;
  int display_w, display_h;
  glm::vec3 camera_position{0, 0, 7.f};
  float fovy = 45.f;
  float z_near = 1.f;
  float z_far = 100.f;

  // user interface state
  bool open_file_dialog = false;
  bool save_file_dialog = false;
  bool debug_output = false;
  bool show_imgui_demo = false;
  std::string input_filename;
  GLFWwindow* window{nullptr};
  application_parameters gui_parameters{camera_position};

  // Animation sequencer state
  gltf_insight::AnimSequence sequence;
  bool looping = true;
  int selectedEntry = -1;
  int firstFrame = 0;
  bool expanded = true;
  int currentFrame;
  double currentPlayTime = 0;
  double last_frame_time = 0;
  bool playing_state = true;

  // Loaded data
  std::vector<GLuint> textures;
  std::vector<animation> animations;
  std::vector<std::string> animation_names;

  // hidden methods

  static std::string GetFilePathExtension(const std::string& FileName);

  void parse_command_line(int argc, char** argv, bool debug_output,
                          std::string& input_filename) const;

  void load_glTF_asset(tinygltf::TinyGLTF& gltf_ctx,
                       const std::string& input_filename,
                       tinygltf::Model& model);

  void load_all_textures(tinygltf::Model model, size_t nb_textures,
                         std::vector<GLuint>& textures);

  void genrate_joint_inverse_bind_matrix_map(
      const tinygltf::Skin& skin, const std::vector<int>::size_type nb_joints,
      std::map<int, int> joint_inverse_bind_matrix_map);

  void cpu_compute_morphed_display_mesh(
      gltf_node mesh_skeleton_graph, size_t submesh_id,
      const std::vector<std::vector<morph_target>>& morph_targets,
      const std::vector<std::vector<float>>& vertex_coord,
      const std::vector<std::vector<float>>& normals,
      std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal, size_t vertex);

  void gpu_update_morphed_submesh(
      size_t submesh_id, std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal,
      std::vector<std::array<GLuint, 6>>& VBOs);

  void perform_software_morphing(
      gltf_node mesh_skeleton_graph, size_t submesh_id,
      const std::vector<std::vector<morph_target>>& morph_targets,
      const std::vector<std::vector<float>>& vertex_coord,
      const std::vector<std::vector<float>>& normals,
      std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal,
      std::vector<std::array<GLuint, 6>>& VBOs);

  void draw_bone_overlay(gltf_node& mesh_skeleton_graph,
                         const std::vector<gltf_node*> flat_bone_list,
                         const glm::mat4& view_matrix,
                         const glm::mat4& projection_matrix,
                         std::map<std::string, shader>& shaders);

  void precompute_hardware_skinning_data(
      gltf_node& mesh_skeleton_graph, glm::mat4& model_matrix,
      std::vector<glm::mat4>& joint_matrices,
      std::vector<gltf_node*>& flat_joint_list,
      std::vector<glm::mat4>& inverse_bind_matrices);

  void run_animation_timeline(gltf_insight::AnimSequence& sequence,
                              bool& looping, int& selectedEntry,
                              int& firstFrame, bool& expanded,
                              int& currentFrame, double& currentPlayTime,
                              double& last_frame_time, bool& playing_state,
                              std::vector<animation>& animations);

  void run_3D_gizmo(glm::mat4& view_matrix, const glm::mat4& projection_matrix,
                    glm::mat4& model_matrix, glm::vec3& camera_position,
                    application_parameters& my_user_pointer);

  void fill_sequencer(gltf_insight::AnimSequence& sequence,
                      const std::vector<animation>& animations);
};
}  // namespace gltf_insight
