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

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "animation.hh"
#include "material.hh"

// This includes opengl for us, along side debuging callbacks
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

#include "gl_util.hh"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "cxxopts.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/matrix.hpp"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <fstream>
#include <iostream>
#include <sstream>

#include "gltf-graph.hh"
#include "gltf-loader.hh"
#include "gui_util.hh"
#include "shader.hh"
#include "tiny_gltf.h"
#include "tiny_gltf_util.h"

// last
#include "tiny_obj_loader.h"

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
  std::vector<std::vector<float>> display_position;
  std::vector<std::vector<float>> display_normals;
  std::vector<std::vector<float>> soft_skinned_position;
  std::vector<std::vector<float>> soft_skinned_normals;
  std::vector<std::vector<float>> colors;
  std::vector<int> materials;

  // Rendering
  std::vector<GLuint> VAOs;
  std::vector<std::array<GLuint, VBO_count>> VBOs;
  std::vector<draw_call_submesh> draw_call_descriptors;

  // Each mesh comes with a set of shader objects to be used. They need to be
  // created after we known some info about the mesh Because gl_util's
  // load_shader function take templated shader code and substitues values. This
  // is required for the GPU skinning implementation.
  std::unique_ptr<std::map<std::string, shader>> shader_list;
  std::unique_ptr<std::map<std::string, shader>> soft_skin_shader_list;

  // is this mesh displayed on screen
  bool displayed = true;

  // Standard C++ stuff
  mesh(const mesh&) = delete;
  mesh& operator=(const mesh&) = delete;
  mesh& operator=(mesh&& o);
  mesh(mesh&& other);
  mesh();
  ~mesh();
};

struct editor_lighting {
  glm::vec3 color = glm::vec3(1.f, 1.f, 1.f);
  float multiplier = 1.f;
  glm::vec3 non_normalized_direction = glm::vec3(-1.f, -0.2f, -1.f);
  glm::vec3 get_directional_light_direction() const;

  bool control_open = true;
  void show_control();

  bool use_ibl = false;
};

class app {
 public:
  enum class display_mode : int {
    normal = 0,
    debug = 1
  } current_display_mode = display_mode::normal;

  void load_sensible_default_material(material& material);
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
  void load_as_metal_roughness(size_t i, material& currently_loading,
                               tinygltf::Material gltf_material);
  void load();

  void main_loop();
  bool main_loop_frame();

  static void open_url(std::string url);

 private:
  void draw_mesh(const glm::vec3& world_camera_position, const mesh& mesh,
                 glm::mat3 normal_matrix, glm::mat4 model_matrix);

  void draw_scene(const glm::vec3& world_camera_position);

  struct defered_draw {
    gltf_node* node;
    defered_draw(gltf_node* n) : node(n) {}
  };

  void draw_scene_recur(const glm::vec3& world_camera_position, gltf_node& node,
                        std::vector<defered_draw>& alpha_models);

  editor_lighting editor_light;

  ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
  ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::WORLD;
  float lower_docked_prop_size = 0.25;
  float lower_docked_max_px_size = 300;

  enum selection_mode : int { manipulate_mesh = 0, manipulate_joint = 1 };

  selection_mode current_mode = manipulate_mesh;

  bool show_shader_selector_window = true;
  bool show_asset_image_window = true;
  bool show_model_info_window = true;
  bool show_animation_window = true;
  bool show_mesh_display_window = true;
  bool show_morph_target_window = true;
  bool show_camera_parameter_window = true;
  bool show_transform_window = true;
  bool show_timeline = true;
  bool show_gizmo = true;
  bool show_bone_selector = true;
  bool show_material_window = true;
  bool show_bone_display_window = true;
  bool show_scene_outline_window = true;
  bool do_soft_skinning = false;

  std::vector<mesh> loaded_meshes;
  std::vector<material> loaded_material;
  material dummy_material;

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
  glm::mat4& root_node_model_matrix = gltf_scene_tree.local_xform;
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

  void parse_command_line(int argc, char** argv);

  // Load glTF asset. Initial input filename is given at `parse_command_line`
  void load_glTF_asset();

  // Assume `textures` are already allocated at least with the size
  // `nb_textures`
  void load_all_textures(size_t nb_textures);

  void genrate_joint_inverse_bind_matrix_map(
      const tinygltf::Skin& skin, const std::vector<int>::size_type nb_joints,
      std::map<int, int>& joint_inverse_bind_matrix_map);

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
      std::vector<std::array<GLuint, VBO_count>>& VBOs);

  void perform_software_morphing(
      gltf_node mesh_skeleton_graph, size_t submesh_id,
      const std::vector<std::vector<morph_target>>& morph_targets,
      const std::vector<std::vector<float>>& positions,
      const std::vector<std::vector<float>>& normals,
      std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal,
      std::vector<std::array<GLuint, VBO_count>>& VBOs,
      bool upload_to_gpu = true);

  void perform_software_skinning(
      size_t submesh_id, const std::vector<glm::mat4>& joint_matrices,
      const std::vector<std::vector<float>>& positions,
      const std::vector<std::vector<float>>& normals,
      const std::vector<std::vector<unsigned short>>& joints,
      const std::vector<std::vector<float>>& weights,
      std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal);

  void draw_bone_overlay(gltf_node& mesh_skeleton_graph, int active_joint_node,
                         const glm::mat4& view_matrix,
                         const glm::mat4& projection_matrix,
                         std::map<std::string, shader>& shaders,
                         const mesh& a_mesh);

  void compute_joint_matrices(gltf_node& mesh_skeleton_graph,
                              glm::mat4& model_matrix,
                              std::vector<glm::mat4>& joint_matrices,
                              std::vector<gltf_node*>& flat_joint_list,
                              std::vector<glm::mat4>& inverse_bind_matrices);

  void run_animation_timeline(gltf_insight::AnimSequence& sequence,
                              bool& looping, int& selectedEntry,
                              int& firstFrame, bool& expanded,
                              int& currentFrame, double& currentPlayTime,
                              double& last_frame_time, bool& playing_state,
                              std::vector<animation>& animations);

  void run_3D_gizmo(gltf_node* active_bone);

  void fill_sequencer();
};
}  // namespace gltf_insight
