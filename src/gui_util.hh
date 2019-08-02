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

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

// include glad first
#ifndef __EMSCRIPTEN__  // we directly have GLES3
#include <glad/glad.h>
#else
#include <GLES3/gl3.h>
#endif
// All head that permit to describe the gui and the data structure used by the
// program

#include "GLFW/glfw3.h"
#include "IconsIonicons.h"
#include "ImCurveEdit.h"
#include "ImGuiFileDialog.h"
#include "ImGuizmo.h"
#include "ImSequencer.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl3.h"
#include "glm/glm.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "animation.hh"
#include "gltf-graph.hh"
#include "material.hh"
#include "tiny_gltf.h"
#include "tiny_gltf_util.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

// after:
#include <glm/common.hpp>

#include "animation-sequencer.inc.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

struct application_parameters {
  glm::vec3& camera_position;
  bool button_states[3]{false};
  double last_mouse_x{0}, last_mouse_y{0};
  double rot_pitch{-45}, rot_yaw{45};
  double rotation_scale = 0.2;
  application_parameters(glm::vec3& cam_pos) : camera_position(cam_pos) {}
};

void gui_new_frame();

void gl_new_frame(GLFWwindow* window, ImVec4 clear_color, int& display_w,
                  int& display_h);

void gl_gui_end_frame(GLFWwindow* window);

// Build a combo widget from a vector of strings
bool ImGuiCombo(const char* label, int* current_item,
                const std::vector<std::string>& items);

// Window that display all the images inside the gltf asset
void asset_images_window(const std::vector<GLuint>& textures,
                         bool* open = nullptr);

// Display error output from glfw to standard output
void glfw_error_callback(int error, const char* description);

// Keyboard event
void key_callback(GLFWwindow* window, int key, int scancode, int action,
                  int mods);

// mouse callback
void mouse_button_callback(GLFWwindow* window, int button, int action,
                           int mods);

// cursor position callback
void cursor_pos_callback(GLFWwindow* window, double mouse_x, double mouse_y);

// Describe the topology of nodes in the gltf as a tree of items in a window
void describe_node_topology_in_imgui_tree(const tinygltf::Model& model,
                                          int node_index);
// Window that display general informations
void model_info_window(const tinygltf::Model& model, bool* open = nullptr);

// Window that display data about the animations in the model
namespace gltf_insight {
struct mesh;
}
void mesh_display_window(std::vector<gltf_insight::mesh>& mesh,
                         bool* open = nullptr);

// Window that display the morph target and their current weights
void morph_target_window(gltf_node& mesh_skeleton_graph, int nb_morph_targets,
                         bool* open = nullptr);

// Call this to initialize a window, and an opengl context in it
void initialize_glfw_opengl_window(GLFWwindow*& window);

// Call this to initialize Dear ImGui
void initialize_imgui(GLFWwindow* window);

// Cleaup the gui and the window systems
void deinitialize_gui_and_window(GLFWwindow* window);

void transform_window(float* vecTranslation, float* vecRotation,
                      float* vecScale,
                      ImGuizmo::OPERATION& current_gizmo_operation,
                      ImGuizmo::MODE& current_gizmo_mode, int* mode,
                      bool* show_gizmo, bool* open = nullptr);

void timeline_window(gltf_insight::AnimSequence loaded_sequence,
                     bool& playing_state, bool& need_to_update_pose,
                     bool& looping, int& selectedEntry, int& firstFrame,
                     bool& expanded, int& currentFrame, double& currentPlayTime,
                     bool* open = nullptr, float docked_size_prop = 0.25f,
                     float docked_size_max_pixel = 300.f);

void shader_selector_window(const std::vector<std::string>& shader_names,
                            int& selected_shader, std::string& shader_to_use,
                            int& display_mode, bool* open = nullptr);

void utilities_window(bool& show_imgui_demo);

void animation_window(std::vector<animation>& animations, bool* open = nullptr);

void camera_parameters_window(float& fovy, float& z_far, bool* open = nullptr);

GLuint load_gltf_insight_icon();
void about_window(GLuint logo, bool* open = nullptr);

// NOTE(LTE): material may have a chance to be modified, so no `const`
// qualifiers.
void material_info_window(gltf_insight::material& dummy,
                          std::vector<gltf_insight::material>& loaded_materials,
                          bool* open = nullptr);

void scene_outline_window(gltf_node& sene, bool* open = nullptr);

void jsonrpc_window(const bool connected, const int port, bool *isopen = nullptr);
