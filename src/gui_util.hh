#pragma once
// include glad first
#include "glad/include/glad/glad.h"

// All head that permit to describe the gui and the data structure used by the
// program

#include "GLFW/glfw3.h"
#include "IconsIonicons.h"
#include "ImCurveEdit.h"
#include "ImGuiFileDialog.h"
#include "ImGuizmo.h"
#include "ImSequencer.h"
#include "animation.hh"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl2.h"
#include "glm/glm.hpp"
#include "gltf-graph.hh"
#include "imgui.h"
#include "imgui_internal.h"
#include "tiny_gltf.h"
#include "tiny_gltf_util.h"

// after:
#include "animation-sequencer.inc.h"

struct application_parameters {
  glm::vec3& camera_position;
  bool button_states[3]{false};
  double last_mouse_x{0}, last_mouse_y{0};
  double rot_pitch{0}, rot_yaw{0};
  double rotation_scale = 0.2;
  application_parameters(glm::vec3& cam_pos) : camera_position(cam_pos) {}
};

void gui_new_frame();

void gl_new_frame(GLFWwindow* window, ImVec4 clear_color, int& display_w,
                  int& display_h);

void gui_end_frame(GLFWwindow* window);

// Build a combo widget from a vector of strings
bool ImGuiCombo(const char* label, int* current_item,
                const std::vector<std::string>& items);

// Window that display all the images inside the gltf asset
void asset_images_window(const std::vector<GLuint>& textures);

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
void model_info_window(const tinygltf::Model& model);

// Window that display data about the animations in the model
void animation_window(const std::vector<animation>& animations);

// Window that display joint and weights asignements of the gltf asset
void skinning_data_window(
    const std::vector<std::vector<float>>& weights,
    const std::vector<std::vector<unsigned short>>& joints);

// Window that display the morph target and their current weights
void morph_target_window(gltf_node& mesh_skeleton_graph, int nb_morph_targets);

// Call this to initialize a window, and an opengl context in it
void initialize_glfw_opengl_window(GLFWwindow*& window);

// Call this to initialize Dear ImGui
void initialize_imgui(GLFWwindow* window);

// Cleaup the gui and the window systems
void deinitialize_gui_and_window(GLFWwindow* window);

void transform_window(glm::mat4& view_matrix, glm::vec3& camera_position,
                      application_parameters& my_user_pointer,
                      float vecTranslation[3], float vecRotation[3],
                      float vecScale[3],
                      ImGuizmo::OPERATION& mCurrentGizmoOperation);

void sequencer_window(gltf_insight::AnimSequence mySequence,
                      bool& playing_state, bool& need_to_update_pose,
                      bool& looping, int& selectedEntry, int& firstFrame,
                      bool& expanded, int& currentFrame,
                      double& currentPlayTime);

void shader_selector_window(const std::vector<std::string>& shader_names,
                            int& selected_shader, std::string& shader_to_use);

void utilities_window(bool& show_imgui_demo);

void camera_parameters_window(float& fovy, float& z_far);
