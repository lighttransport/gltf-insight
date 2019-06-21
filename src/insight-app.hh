#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include "animation.hh"
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
  std::string name;

  // skinning and morph data
  int nb_joints = 0;
  std::vector<std::vector<unsigned short>> joints;
  std::vector<glm::mat4> joint_matrices;
  std::map<int, int> joint_inverse_bind_matrix_map;
  std::vector<gltf_node*> flat_joint_list;
  std::vector<glm::mat4> inverse_bind_matrices;
  int nb_morph_targets = 0;
  std::vector<std::vector<morph_target>> morph_targets;
  // if true, mesh has skinning data
  bool skinned = false;

  // geometry data
  std::vector<std::vector<unsigned>> indices;
  std::vector<std::vector<float>> positions;
  std::vector<std::vector<float>> uvs;
  std::vector<std::vector<float>> normals;
  std::vector<std::vector<float>> weights;
  std::vector<std::vector<float>> tangents;
  std::vector<std::vector<float>> display_position;
  std::vector<std::vector<float>> display_normals;
  std::vector<std::vector<float>> display_tangents;

  // Rendering
  std::vector<GLuint> VAOs;
  std::vector<std::array<GLuint, 7>> VBOs;
  std::vector<draw_call_submesh> draw_call_descriptors;
  // The "material" is just defined as the shader used to render the object...
  // string = a standardized display mode
  // shader = shader object to use
  std::unique_ptr<std::map<std::string, shader>> shader_list;

  // is this mesh displayed on screen
  bool displayed = true;

  // Standard C++ stuff
  mesh(const mesh&) = delete;
  mesh& operator=(const mesh&) = delete;
  mesh& operator=(mesh&& o) throw();
  mesh(mesh&& other) throw();
  mesh();
  ~mesh();
};

class app {
 public:
  app(int argc, char** argv);
  ~app();
  void run_file_menu();
  void run_edit_menu();
  void run_view_menu();
  void run_debug_menu();
  void run_help_menu(bool& about_open);
  void run_menubar(bool& about_open);
  void create_transparent_docking_area(ImVec2 pos, ImVec2 size,
                                       std::string name);

  void unload();
  void load();
  void main_loop();

 private:
  ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
  ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::WORLD;
  float lower_docked_prop_size = 0.25;
  float lower_docked_max_px_size = 300;

  enum selection_mode : int { manipulate_mesh = 0, manipulate_joint = 1 };

  selection_mode current_mode = manipulate_mesh;

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
  int active_joint = 0;

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
  bool playing_state = false;

  GLuint logo = 0;

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
      std::vector<std::array<GLuint, 7>>& VBOs);

  void perform_software_morphing(
      gltf_node mesh_skeleton_graph, size_t submesh_id,
      const std::vector<std::vector<morph_target>>& morph_targets,
      const std::vector<std::vector<float>>& vertex_coord,
      const std::vector<std::vector<float>>& normals,
      std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal,
      std::vector<std::array<GLuint, 7>>& VBOs);

  void draw_bone_overlay(gltf_node& mesh_skeleton_graph, int active_joint_node,
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
                    glm::mat4& model_matrix, gltf_node* active_bone,
                    glm::vec3& camera_position,
                    application_parameters& my_user_pointer);

  void fill_sequencer(gltf_insight::AnimSequence& sequence,
                      const std::vector<animation>& animations);
};
}  // namespace gltf_insight
