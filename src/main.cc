#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

// For OpenGL, we always include loader library first
#include "glad/include/glad/glad.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "ImCurveEdit.h"
#include "ImGuizmo.h"
#include "ImSequencer.h"
#include "cxxopts.hpp"
#include "imgui.h"
#include "imgui_internal.h"

// imgui
#include "GLFW/glfw3.h"
#include "animation-sequencer.inc.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl2.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "animation.hh"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/matrix.hpp"
#include "gltf-graph.hh"
#include "shader.hpp"
#include "tiny_gltf.h"
#include "tiny_gltf_util.h"

static bool ImGuiCombo(const char *label, int *current_item,
                       const std::vector<std::string> &items) {
  return ImGui::Combo(
      label, current_item,
      [](void *data, int idx_i, const char **out_text) {
        size_t idx = static_cast<size_t>(idx_i);
        const std::vector<std::string> *str_vec =
            reinterpret_cast<std::vector<std::string> *>(data);
        if (idx_i < 0 || str_vec->size() <= idx) {
          return false;
        }
        *out_text = str_vec->at(idx).c_str();
        return true;
      },
      reinterpret_cast<void *>(const_cast<std::vector<std::string> *>(&items)),
      static_cast<int>(items.size()), static_cast<int>(items.size()));
}

static void error_callback(int error, const char *description) {
  std::cerr << "GLFW Error : " << error << ", " << description << std::endl;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  (void)scancode;

  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureKeyboard) {
    return;
  }

  if (key == GLFW_KEY_Q && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

#if 0
static void mouseButtonCallback(int button, int state, float x, float y) {
  (void)x;
  (void)y;
  ImGui_ImplBtGui_SetMouseButtonState(button, (state == 1));

  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
    retturn
#endif

static int find_node_with_mesh_in_children(const tinygltf::Model &model,
                                           int root) {
  const auto &root_node = model.nodes[root];
  if (root_node.mesh >= 0) return root;

  for (auto child : root_node.children) {
    const auto result = find_node_with_mesh_in_children(model, child);
    if (result > 0 && model.nodes[result].mesh >= 0) return result;
  }

  return -1;
}

static int find_main_mesh_node(const tinygltf::Model &model) {
  const auto &node_list =
      model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0].nodes;

  for (auto node : node_list) {
    const auto mesh_node = find_node_with_mesh_in_children(model, node);
    if (mesh_node >= 0) return mesh_node;
  }

  return -1;
}

static void describe_node_topology_in_imgui_tree(const tinygltf::Model &model,
                                                 int node_index) {
  std::string node_desc = "node [" + std::to_string(node_index) + "]";
  if (ImGui::TreeNode(node_desc.c_str())) {
    const auto &node = model.nodes[node_index];
    if (!node.name.empty()) ImGui::Text("name [%s]", node.name.c_str());

    for (auto child : node.children) {
      describe_node_topology_in_imgui_tree(model, child);
    }

    ImGui::TreePop();
  }
}

static void model_info_window(const tinygltf::Model &model) {
  // TODO also cache info found here
  if (ImGui::Begin("Model information")) {
    const auto main_node_index = find_main_mesh_node(model);
    if (main_node_index < 0) {
      ImGui::Text("Could not find a node with a mesh in your glTF file!");
      return;
    }

    ImGui::Text("Main node with a mesh [%d]");
    const auto &main_node = model.nodes[main_node_index];

    if (main_node.skin >= 0) {
      // mesh.weights
      ImGui::Text("Main mesh uses skin [%d]", main_node.skin);
      const auto &skin = model.skins[main_node.skin];
      ImGui::Text("Skin [%d] skeleton root node [%d]", main_node.skin,
                  skin.skeleton);
      ImGui::Text("Skin joint count [%d]", skin.joints.size());
      if (ImGui::CollapsingHeader("Skeleton topology"))
        describe_node_topology_in_imgui_tree(model, skin.skeleton);
    }

    // TODO check if the found node with a mesh has morph targets, and
    // describe them in this window here
  }
  ImGui::End();
}

static void animation_window(const std::vector<animation> &animations) {
  ImGui::Begin("Animations");

  if (animations.size() == 0) {
    ImGui::Text("No animations in glTF");
    ImGui::End();
    return;
  }

  std::vector<std::string> animation_names;

  for (const auto &input_animation : animations)
    animation_names.push_back(input_animation.name);

  static int idx = 0;
  ImGuiCombo("animations", &idx, animation_names);

  // const tinygltf::Animation &animation = model.animations[size_t(idx)];
  const auto &animation = animations[idx];

  //
  // channels
  //
  if (ImGui::CollapsingHeader("channels")) {
    ImGui::Text("Animation has [%d] channels", animation.channels.size());
    static int channel_idx = 0;
    ImGui::InputInt("channel", &channel_idx, 1, 1);
    channel_idx = std::max<int>(
        std::min<int>(channel_idx, animation.channels.size() - 1), 0);
    const auto &channel = animation.channels[channel_idx];

    // ImGui::Text("sampler [%d]", channel.sampler);
    ImGui::Text("target node [%d] path [%s]", channel.target_node, [channel] {
      switch (channel.mode) {
        case animation::channel::path::translation:
          return "translation";
        case animation::channel::path::rotation:
          return "rotation";
        case animation::channel::path::scale:
          return "scale";
        case animation::channel::path::weight:
          return "weight";
        case animation::channel::path::not_assigned:
        default:
          return "ERROR";
      }
    }());

    switch (channel.mode) {
      case animation::channel::path::weight:
        ImGui::Columns(2);
        ImGui::Separator();
        ImGui::Text("Frame");
        ImGui::NextColumn();
        ImGui::Text("Weight Value");
        ImGui::NextColumn();
        ImGui::Separator();
        for (auto keyframe : channel.keyframes) {
          {
            ImGui::Text("%d", keyframe.first);
            ImGui::NextColumn();
            ImGui::Text("%f", keyframe.second.motion.weight);
            ImGui::NextColumn();
            ImGui::Separator();
          }
        }
        ImGui::Columns();
        break;
      case animation::channel::path::translation:
        ImGui::Columns(4);
        ImGui::Separator();
        ImGui::Text("Frame");
        ImGui::NextColumn();
        ImGui::Text("X");
        ImGui::NextColumn();
        ImGui::Text("Y");
        ImGui::NextColumn();
        ImGui::Text("Z");
        ImGui::NextColumn();
        ImGui::Separator();
        for (auto keyframe : channel.keyframes) {
          ImGui::Text("%d", keyframe.first);
          ImGui::NextColumn();
          auto v = keyframe.second.motion.translation;
          ImGui::Text("%f", v.x);
          ImGui::NextColumn();
          ImGui::Text("%f", v.y);
          ImGui::NextColumn();
          ImGui::Text("%f", v.z);
          ImGui::NextColumn();
          ImGui::Separator();
        }
        ImGui::Columns();
        break;
      case animation::channel::path::scale:
        ImGui::Columns(4);
        ImGui::Separator();
        ImGui::Text("Frame");
        ImGui::NextColumn();
        ImGui::Text("X");
        ImGui::NextColumn();
        ImGui::Text("Y");
        ImGui::NextColumn();
        ImGui::Text("Z");
        ImGui::NextColumn();
        ImGui::Separator();
        for (auto keyframe : channel.keyframes) {
          ImGui::Text("%d", keyframe.first);
          ImGui::NextColumn();
          auto v = keyframe.second.motion.scale;
          ImGui::Text("%f", v.x);
          ImGui::NextColumn();
          ImGui::Text("%f", v.y);
          ImGui::NextColumn();
          ImGui::Text("%f", v.z);
          ImGui::NextColumn();
          ImGui::Separator();
        }
        ImGui::Columns();
        break;
      case animation::channel::path::rotation:
        ImGui::Columns(5);
        ImGui::Separator();
        ImGui::Text("Frame");
        ImGui::NextColumn();
        ImGui::Text("X");
        ImGui::NextColumn();
        ImGui::Text("Y");
        ImGui::NextColumn();
        ImGui::Text("Z");
        ImGui::NextColumn();
        ImGui::Text("W");
        ImGui::NextColumn();
        ImGui::Separator();
        for (auto keyframe : channel.keyframes) {
          ImGui::Text("%d", keyframe.first);
          ImGui::NextColumn();
          auto quat = keyframe.second.motion.rotation;
          ImGui::Text("%f", quat.x);
          ImGui::NextColumn();
          ImGui::Text("%f", quat.y);
          ImGui::NextColumn();
          ImGui::Text("%f", quat.z);
          ImGui::NextColumn();
          ImGui::Text("%f", quat.w);
          ImGui::NextColumn();
          ImGui::Separator();
        }
        ImGui::Columns();
        break;
      case animation::channel::path::not_assigned:
      default:
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.1f, 1.0f),
                           "ERROR Unknown target path name");
    }
  }

  //
  // samplers
  //
  if (ImGui::CollapsingHeader("samplers")) {
    std::vector<std::string> sampler_names;

    ImGui::Text("Animation has [%d] samplers", animation.samplers.size());
    static int sampler_idx = 0;
    ImGui::InputInt("sampler", &sampler_idx, 1, 1);
    sampler_idx = std::max<int>(
        std::min<int>(sampler_idx, animation.samplers.size() - 1), 0);
    const auto &sampler = animation.samplers[sampler_idx];

    ImGui::Text("Interpolation method [%s]", [sampler] {
      switch (sampler.mode) {
        case animation::sampler::interpolation::linear:
          return "LINEAR";
        case animation::sampler::interpolation::step:
          return "STEP";
        case animation::sampler::interpolation::cubic_spline:
          return "CUBICSPLINE";
        case animation::sampler::interpolation::not_assigned:
        default:
          return "ERROR";
      }
    }());

    ImGui::Text("Keyframe range : %f, %f [secs]", sampler.min_v, sampler.max_v);
    ImGui::Text("# of key frames : %d", sampler.keyframes.size());

    ImGui::Columns(2);
    ImGui::Text("Frame Number");
    ImGui::NextColumn();
    ImGui::Text("Timestamp");
    ImGui::NextColumn();
    ImGui::Separator();
    for (auto keyframe : sampler.keyframes) {
      ImGui::Text("%d", keyframe.first);
      ImGui::NextColumn();
      ImGui::Text("%f", keyframe.second);
      ImGui::NextColumn();
      ImGui::Separator();
    }
    ImGui::Columns();
  }

  ImGui::End();
}

void skinning_data_window(size_t submesh_id, const std::vector<float> &weights,
                          const std::vector<unsigned short> &joints) {
  ImGui::Begin("Skinning data", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Submesh[%d]", submesh_id);
  ImGui::BeginChild("##scrollable_data_region", ImVec2(600, 800), false,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar),
      submesh_id;
  const auto vertex_count = weights.size() / 4;
  assert(vertex_count == joints.size() / 4);

  ImGui::Columns(5);
  ImGui::Text("Vertex Index");
  ImGui::NextColumn();
  ImGui::Text("0");
  ImGui::NextColumn();
  ImGui::Text("1");
  ImGui::NextColumn();
  ImGui::Text("2");
  ImGui::NextColumn();
  ImGui::Text("3");
  ImGui::NextColumn();
  ImGui::Separator();
  for (size_t i = 0; i < vertex_count; ++i) {
    ImGui::Text("%d", i);
    ImGui::NextColumn();
    ImGui::Text("%f * %d", weights[i * 4], joints[i * 4]);
    ImGui::NextColumn();
    ImGui::Text("%f * %d", weights[i * 4 + 1], joints[i * 4 + 1]);
    ImGui::NextColumn();
    ImGui::Text("%f * %d", weights[i * 4 + 2], joints[i * 4 + 2]);
    ImGui::NextColumn();
    ImGui::Text("%f * %d", weights[i * 4 + 3], joints[i * 4 + 3]);
    ImGui::NextColumn();
    ImGui::Separator();
  }
  ImGui::Columns();
  ImGui::EndChild();
  ImGui::End();
}

static bool BuildAnimationSequencer(const tinygltf::Model &model,
                                    gltf_insight::AnimSequence *seq) {
  return true;
}

static std::string GetFilePathExtension(const std::string &FileName) {
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

struct draw_call_submesh {
  GLenum draw_mode;
  size_t count;
  GLuint VAO, main_texture;
};

void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar *message, void *userParam) {
  // ignore non-significant error/warning codes
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

  std::cout << "---------------" << '\n';
  std::cout << "Debug message (" << id << "): " << message << '\n';

  switch (source) {
    case GL_DEBUG_SOURCE_API:
      std::cout << "Source: API";
      break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      std::cout << "Source: Window System";
      break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      std::cout << "Source: Shader Compiler";
      break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      std::cout << "Source: Third Party";
      break;
    case GL_DEBUG_SOURCE_APPLICATION:
      std::cout << "Source: Application";
      break;
    case GL_DEBUG_SOURCE_OTHER:
      std::cout << "Source: Other";
      break;
    default:
      std::cout << "Source: Unknown";
  }
  std::cout << '\n';

  switch (type) {
    case GL_DEBUG_TYPE_ERROR:
      std::cout << "Type: Error";
      break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      std::cout << "Type: Deprecated Behaviour";
      break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      std::cout << "Type: Undefined Behaviour";
      break;
    case GL_DEBUG_TYPE_PORTABILITY:
      std::cout << "Type: Portability";
      break;
    case GL_DEBUG_TYPE_PERFORMANCE:
      std::cout << "Type: Performance";
      break;
    case GL_DEBUG_TYPE_MARKER:
      std::cout << "Type: Marker";
      break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
      std::cout << "Type: Push Group";
      break;
    case GL_DEBUG_TYPE_POP_GROUP:
      std::cout << "Type: Pop Group";
      break;
    case GL_DEBUG_TYPE_OTHER:
      std::cout << "Type: Other";
      break;
    default:
      std::cout << "Type: Unknown";
  }
  std::cout << '\n';

  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      std::cout << "Severity: high";
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      std::cout << "Severity: medium";
      break;
    case GL_DEBUG_SEVERITY_LOW:
      std::cout << "Severity: low";
      break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      std::cout << "Severity: notification";
      break;
    default:
      std::cout << "Severity: Unknown";
  }
  std::cout << "\n\n";
}

void asset_images_window(const std::vector<GLuint> &textures) {
  if (ImGui::Begin("glTF Images", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Number of textures [%d]", textures.size());
    ImGui::BeginChild("##ScrollableRegion0", ImVec2(256, 286), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (int i = 0; i < textures.size(); ++i) {
      auto &texture = textures[i];
      std::string name = "texture [" + std::to_string(i) + "]";
      if (ImGui::CollapsingHeader(name.c_str())) {
        ImGui::Image(ImTextureID(size_t(texture)), ImVec2(256.f, 256.f));
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();
}

// skeleton_index is the first node that will be added as a child of
// "graph_root" e.g: a gltf node has a mesh. That mesh has a skin, and that
// skin as a node index as "skeleton". You need to pass that "skeleton"
// integer to this function as skeleton_index. This returns a flat array of
// the bones to be used by the skinning code
void populate_gltf_skeleton_subgraph(const tinygltf::Model &model,
                                     gltf_node &graph_root,
                                     int skeleton_index) {
  const auto &skeleton_node = model.nodes[skeleton_index];

  glm::mat4 xform(1.f);
  glm::vec3 translation(0.f), scale(1.f, 1.f, 1.f);
  glm::quat rotation(1.f, 0.f, 0.f, 0.f);

  const auto &node_matrix = skeleton_node.matrix;
  if (node_matrix.size() == 16)  // 4x4 matrix
  {
    double tmp[16];
    float tmpf[16];
    memcpy(tmp, skeleton_node.matrix.data(), 16 * sizeof(double));
    for (int i = 0; i < 16; ++i) {
      tmpf[i] = float(tmp[i]);
    }

    xform = glm::make_mat4(tmpf);
  }

  const auto &node_translation = skeleton_node.translation;
  if (node_translation.size() == 3)  // 3D vector
  {
    for (size_t i = 0; i < 3; ++i) translation[i] = float(node_translation[i]);
  }

  const auto &node_scale = skeleton_node.scale;
  if (node_scale.size() == 3)  // 3D vector
  {
    for (size_t i = 0; i < 3; ++i) scale[i] = float(node_scale[i]);
  }

  const auto &node_rotation = skeleton_node.rotation;
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

  xform = xform * reconstructed_matrix;

  graph_root.add_child_bone(xform);
  auto &new_bone = *graph_root.children.back().get();
  new_bone.gltf_model_node_index = skeleton_index;

  for (int child : skeleton_node.children) {
    populate_gltf_skeleton_subgraph(model, new_bone, child);
  }
}

void draw_space_origin_point(float point_size) {
  // set the size
  glPointSize(point_size);

  // Since we're not even drawing a polygon, it's probably simpler to do
  // it with old-style opengl
  glBegin(GL_POINTS);
  glVertex4f(0, 0, 0, 1);
  glEnd();
}

void draw_space_base(GLuint shader, const float line_width,
                     const float axis_scale) {
  glLineWidth(line_width);
  glUniform4f(glGetUniformLocation(shader, "debug_color"), 1, 0, 0, 1);
  glBegin(GL_LINES);
  glVertex4f(0, 0, 0, 1);
  glVertex4f(axis_scale, 0, 0, 1);
  glEnd();

  glUniform4f(glGetUniformLocation(shader, "debug_color"), 0, 1, 0, 1);
  glBegin(GL_LINES);
  glVertex4f(0, 0, 0, 1);
  glVertex4f(0, axis_scale, 0, 1);
  glEnd();

  glUniform4f(glGetUniformLocation(shader, "debug_color"), 0, 0, 1, 1);
  glBegin(GL_LINES);
  glVertex4f(0, 0, 0, 1);
  glVertex4f(0, 0, axis_scale, 1);
  glEnd();
}

bool draw_joint_point = true;
bool draw_bone_segment = true;
bool draw_mesh_anchor_point = true;
bool draw_bone_axes = true;

// TODO use this snipet in a fragment shader to draw a cirle instead of a
// square vec2 coord = gl_PointCoord - vec2(0.5);  //from [0,1] to
// [-0.5,0.5] if(length(coord) > 0.5)                  //outside of circle
// radius?
//    discard;
void draw_bones(gltf_node &root, GLuint shader, glm::mat4 view_matrix,
                glm::mat4 projection_matrix) {
  glUseProgram(shader);
  glm::mat4 mvp = projection_matrix * view_matrix * root.world_xform;
  glUniformMatrix4fv(glGetUniformLocation(shader, "mvp"), 1, GL_FALSE,
                     glm::value_ptr(mvp));

  const float line_width = 2;
  const float axis_scale = 0.125;

  if (draw_bone_axes) draw_space_base(shader, line_width, axis_scale);

  for (auto &child : root.children) {
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

void create_flat_bone_array(gltf_node &root,
                            std::vector<gltf_node *> &flat_array) {
  if (root.type == gltf_node::node_type::bone)
    flat_array.push_back(root.get_ptr());

  for (auto &child : root.children) create_flat_bone_array(*child, flat_array);
}

void sort_bone_array(std::vector<gltf_node *> &bone_array,
                     const tinygltf::Skin &skin_object) {
  assert(bone_array.size() == skin_object.joints.size());
  for (size_t counter = 0; counter < bone_array.size(); ++counter) {
    int index_to_find = skin_object.joints[counter];
    for (size_t bone_index = 0; bone_index < bone_array.size(); ++bone_index) {
      if (bone_array[bone_index]->gltf_model_node_index == index_to_find) {
        std::swap(bone_array[counter], bone_array[bone_index]);
        break;
      }
    }
  }
}

void create_flat_bone_list(const tinygltf::Skin &skin,
                           const std::vector<int>::size_type nb_joints,
                           gltf_node mesh_skeleton_graph,
                           std::vector<gltf_node *> &flatened_bone_list) {
  create_flat_bone_array(mesh_skeleton_graph, flatened_bone_list);
  sort_bone_array(flatened_bone_list, skin);
}

void load_animations(const tinygltf::Model &model,
                     std::vector<animation> &animations) {
  const auto nb_animations = animations.size();
  for (int i = 0; i < nb_animations; ++i) {
    const auto &gltf_animation = model.animations[i];

    animations[i].name = !gltf_animation.name.empty()
                             ? gltf_animation.name
                             : "animation_" + std::to_string(i);

    animations[i].samplers.resize(gltf_animation.samplers.size());

    for (int sampler_index = 0; sampler_index < animations[i].samplers.size();
         ++sampler_index) {
      animations[i].samplers[sampler_index].mode = [&] {
        if (gltf_animation.samplers[sampler_index].interpolation == "LINEAR")
          return animation::sampler::interpolation::linear;
        if (gltf_animation.samplers[sampler_index].interpolation == "STEP")
          return animation::sampler::interpolation::step;
        if (gltf_animation.samplers[sampler_index].interpolation ==
            "CUBICSPLINE")
          return animation::sampler::interpolation::cubic_spline;
        return animation::sampler::interpolation::not_assigned;
      }();

      float min_v, max_v;
      tinygltf::util::GetAnimationSamplerInputMinMax(
          gltf_animation.samplers[sampler_index], model, &min_v, &max_v);
      animations[i].samplers[sampler_index].min_v = min_v;
      animations[i].samplers[sampler_index].max_v = max_v;

      const auto nb_frames = tinygltf::util::GetAnimationSamplerInputCount(
          gltf_animation.samplers[sampler_index], model);
      animations[i].samplers[sampler_index].keyframes.resize(nb_frames);

      float value = 0;
      for (int keyframe = 0; keyframe < nb_frames; ++keyframe) {
        tinygltf::util::DecodeScalarAnimationValue(
            size_t(keyframe),
            model.accessors[gltf_animation.samplers[sampler_index].input],
            model, &value);
        animations[i].samplers[sampler_index].keyframes[keyframe] =
            std::make_pair(keyframe, value);
      }
    }

    animations[i].channels.resize(gltf_animation.channels.size());
    for (int channel_index = 0; channel_index < animations[i].channels.size();
         ++channel_index) {
      animations[i].channels[channel_index].target_node =
          gltf_animation.channels[channel_index].target_node;

      animations[i].channels[channel_index].sampler_index =
          gltf_animation.channels[channel_index].sampler;

      const auto &sampler =
          gltf_animation
              .samplers[gltf_animation.channels[channel_index].sampler];

      const auto nb_frames =
          tinygltf::util::GetAnimationSamplerOutputCount(sampler, model);
      animations[i].channels[channel_index].keyframes.resize(nb_frames);
      const auto &accessor = model.accessors[sampler.output];

      if (gltf_animation.channels[channel_index].target_path == "weights") {
        animations[i].channels[channel_index].mode =
            animation::channel::path::weight;

        float value;

        for (int frame = 0; frame < nb_frames; ++frame) {
          tinygltf::util::DecodeScalarAnimationValue(size_t(frame), accessor,
                                                     model, &value);
          animations[i].channels[channel_index].keyframes[frame].first = frame;
          animations[i]
              .channels[channel_index]
              .keyframes[frame]
              .second.motion.weight = value;
        }
      }

      if (gltf_animation.channels[channel_index].target_path == "translation") {
        animations[i].channels[channel_index].mode =
            animation::channel::path::translation;

        float xyz[3];

        for (int frame = 0; frame < nb_frames; ++frame) {
          tinygltf::util::DecodeTranslationAnimationValue(size_t(frame),
                                                          accessor, model, xyz);
          animations[i].channels[channel_index].keyframes[frame].first = frame;
          animations[i]
              .channels[channel_index]
              .keyframes[frame]
              .second.motion.translation = glm::make_vec3(xyz);
        }
      }

      if (gltf_animation.channels[channel_index].target_path == "rotation") {
        animations[i].channels[channel_index].mode =
            animation::channel::path::rotation;

        float xyzw[4];

        for (int frame = 0; frame < nb_frames; ++frame) {
          tinygltf::util::DecodeRotationAnimationValue(size_t(frame), accessor,
                                                       model, xyzw);
          glm::quat q;
          q.w = xyzw[3];
          q.x = xyzw[0];
          q.y = xyzw[1];
          q.z = xyzw[2];
          q = glm::normalize(q);

          animations[i].channels[channel_index].keyframes[frame].first = frame;
          animations[i]
              .channels[channel_index]
              .keyframes[frame]
              .second.motion.rotation = q;
        }
      }

      if (gltf_animation.channels[channel_index].target_path == "scale") {
        animations[i].channels[channel_index].mode =
            animation::channel::path::scale;
        float xyz[3];

        for (int frame = 0; frame < nb_frames; ++frame) {
          tinygltf::util::DecodeScaleAnimationValue(size_t(frame), accessor,
                                                    model, xyz);
          animations[i].channels[channel_index].keyframes[frame].first = frame;
          animations[i]
              .channels[channel_index]
              .keyframes[frame]
              .second.motion.scale = glm::make_vec3(xyz);
        }
      }
    }

    animations[i].compute_time_boundaries();
  }
}

void load_geometry(const tinygltf::Model &model, std::vector<GLuint> &textures,
                   const std::vector<tinygltf::Primitive> &primitives,
                   std::vector<draw_call_submesh> &draw_call_descriptor,
                   const std::vector<GLuint> &VAOs,
                   const std::vector<GLuint[6]> &VBOs,
                   std::vector<std::vector<unsigned>> &indices,
                   std::vector<std::vector<float>> &vertex_coord,
                   std::vector<std::vector<float>> &texture_coord,
                   std::vector<std::vector<float>> &normals,
                   std::vector<std::vector<float>> &weights,
                   std::vector<std::vector<unsigned short>> &joints) {
  const auto nb_submeshes = primitives.size();
  for (size_t submesh = 0; submesh < nb_submeshes; ++submesh) {
    const auto &primitive = primitives[submesh];

    // We have one VAO per "submesh" (= gltf primitive)
    draw_call_descriptor[submesh].VAO = VAOs[submesh];
    // Primitive uses their own draw mode (eg: lines (for hairs?),
    // triangle fan/strip/list?)
    draw_call_descriptor[submesh].draw_mode = primitive.mode;

    // INDEX BUFFER
    {
      const auto &indices_accessor = model.accessors[primitive.indices];
      const auto &indices_buffer_view =
          model.bufferViews[indices_accessor.bufferView];
      const auto &indices_buffer = model.buffers[indices_buffer_view.buffer];
      const auto indices_start_pointer = indices_buffer.data.data() +
                                         indices_buffer_view.byteOffset +
                                         indices_accessor.byteOffset;
      const auto indices_stride =
          indices_accessor.ByteStride(indices_buffer_view);
      indices[submesh].resize(indices_accessor.count);
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(indices_accessor.componentType);
      assert(indices_accessor.type == TINYGLTF_TYPE_SCALAR);
      assert(sizeof(unsigned int) >= byte_size_of_component);

      for (size_t i = 0; i < indices_accessor.count; ++i) {
        unsigned int temp = 0;
        memcpy(&temp, indices_start_pointer + i * indices_stride,
               byte_size_of_component);
        indices[submesh][i] = unsigned(temp);
      }
      // number of elements to pass to glDrawElements(...)
      draw_call_descriptor[submesh].count = indices_accessor.count;
    }
    // VERTEX POSITIONS}
    {
      const auto position = primitive.attributes.at("POSITION");
      const auto &position_accessor = model.accessors[position];
      const auto &position_buffer_view =
          model.bufferViews[position_accessor.bufferView];
      const auto &position_buffer = model.buffers[position_buffer_view.buffer];
      const auto position_stride =
          position_accessor.ByteStride(position_buffer_view);
      const auto position_start_pointer = position_buffer.data.data() +
                                          position_buffer_view.byteOffset +
                                          position_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(position_accessor.componentType);
      assert(position_accessor.type == TINYGLTF_TYPE_VEC3);
      assert(sizeof(double) >= byte_size_of_component);

      vertex_coord[submesh].resize(position_accessor.count * 3);
      for (size_t i = 0; i < position_accessor.count; ++i) {
        if (byte_size_of_component == sizeof(double)) {
          double temp[3];
          memcpy(&temp, position_start_pointer + i * position_stride,
                 byte_size_of_component * 3);
          for (size_t j = 0; j < 3; ++j) {
            vertex_coord[submesh][i * 3 + j] = float(temp[j]);
          }
        } else if (byte_size_of_component == sizeof(float)) {
          memcpy(&vertex_coord[submesh][i * 3],
                 position_start_pointer + i * position_stride,
                 byte_size_of_component * 3);
        }
      }
    }
    // VERTEX NORMAL
    {
      const auto normal = primitive.attributes.at("NORMAL");
      const auto &normal_accessor = model.accessors[normal];
      const auto &normal_buffer_view =
          model.bufferViews[normal_accessor.bufferView];
      const auto &normal_buffer = model.buffers[normal_buffer_view.buffer];
      const auto normal_stride = normal_accessor.ByteStride(normal_buffer_view);
      const auto normal_start_pointer = normal_buffer.data.data() +
                                        normal_buffer_view.byteOffset +
                                        normal_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(normal_accessor.componentType);
      assert(normal_accessor.type == TINYGLTF_TYPE_VEC3);
      assert(sizeof(double) >= byte_size_of_component);

      normals[submesh].resize(normal_accessor.count * 3);
      for (size_t i = 0; i < normal_accessor.count; ++i) {
        if (byte_size_of_component == sizeof(double)) {
          double temp[3];
          memcpy(&temp, normal_start_pointer + i * normal_stride,
                 byte_size_of_component * 3);
          for (size_t j = 0; j < 3; ++j) {
            normals[submesh][i * 3 + j] = float(temp[j]);  // downcast to
            // float
          }
        } else if (byte_size_of_component == sizeof(float)) {
          memcpy(&normals[submesh][i * 3],
                 normal_start_pointer + i * normal_stride,
                 byte_size_of_component * 3);
        }
      }
    }

    // VERTEX UV
    if (textures.size() > 0) {
      const auto texture = primitive.attributes.at("TEXCOORD_0");
      const auto &texture_accessor = model.accessors[texture];
      const auto &texture_buffer_view =
          model.bufferViews[texture_accessor.bufferView];
      const auto &texture_buffer = model.buffers[texture_buffer_view.buffer];
      const auto texture_stride =
          texture_accessor.ByteStride(texture_buffer_view);
      const auto texture_start_pointer = texture_buffer.data.data() +
                                         texture_buffer_view.byteOffset +
                                         texture_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(texture_accessor.componentType);
      assert(texture_accessor.type == TINYGLTF_TYPE_VEC2);
      assert(sizeof(double) >= byte_size_of_component);

      texture_coord[submesh].resize(texture_accessor.count * 2);
      for (size_t i = 0; i < texture_accessor.count; ++i) {
        if (byte_size_of_component == sizeof(double)) {
          double temp[2];
          memcpy(&temp, texture_start_pointer + i * texture_stride,
                 byte_size_of_component * 2);
          for (size_t j = 0; j < 2; ++j) {
            texture_coord[submesh][i * 2 + j] = float(temp[j]);  // downcast
            // to float
          }
        } else if (byte_size_of_component == sizeof(float)) {
          memcpy(&texture_coord[submesh][i * 2],
                 texture_start_pointer + i * texture_stride,
                 byte_size_of_component * 2);
        }
      }
    }

    // VERTEX JOINTS ASSIGNMENT
    {
      const auto joint = primitive.attributes.at("JOINTS_0");
      const auto &joints_accessor = model.accessors[joint];
      const auto &joints_buffer_view =
          model.bufferViews[joints_accessor.bufferView];
      const auto &joints_buffer = model.buffers[joints_buffer_view.buffer];
      const auto joints_stride = joints_accessor.ByteStride(joints_buffer_view);
      const auto joints_start_pointer = joints_buffer.data.data() +
                                        joints_buffer_view.byteOffset +
                                        joints_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(joints_accessor.componentType);
      assert(joints_accessor.type == TINYGLTF_TYPE_VEC4);
      assert(sizeof(unsigned short) >= byte_size_of_component);

      joints[submesh].resize(4 * joints_accessor.count);

      for (size_t i = 0; i < joints_accessor.count; ++i) {
        memcpy(&joints[submesh][i * 4],
               joints_start_pointer + i * joints_stride,
               byte_size_of_component * 4);
      }
    }

    // VERTEX BONE WEIGHTS
    {
      const auto weight = primitive.attributes.at("WEIGHTS_0");
      const auto &weights_accessor = model.accessors[weight];
      const auto &weights_buffer_view =
          model.bufferViews[weights_accessor.bufferView];
      const auto &weights_buffer = model.buffers[weights_buffer_view.buffer];
      const auto weights_stride =
          weights_accessor.ByteStride(weights_buffer_view);
      const auto weights_start_pointer = weights_buffer.data.data() +
                                         weights_buffer_view.byteOffset +
                                         weights_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(weights_accessor.componentType);
      assert(weights_accessor.type == TINYGLTF_TYPE_VEC4);
      assert(sizeof(float) >= byte_size_of_component);

      weights[submesh].resize(4 * weights_accessor.count);

      for (size_t i = 0; i < weights_accessor.count; ++i) {
        if (weights_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
          memcpy(&weights[submesh][i * 4],
                 weights_start_pointer + i * weights_stride,
                 byte_size_of_component * 4);
        } else {
          // Must convert normalized unsigned value to floating point
          unsigned short temp = 0;
          for (int j = 0; j < 4; j++) {
            memcpy(&temp,
                   weights_start_pointer + i * weights_stride +
                       j * byte_size_of_component,
                   byte_size_of_component);
            weights[submesh][i * 4 + j] =
                float(temp) /
                (byte_size_of_component == 2 ? float(0xFFFF) : float(0xFF));
          }
        }
      }
    }

    {  // GPU upload and shader layout association
      glBindVertexArray(VAOs[submesh]);

      // Layout "0" = vertex coordinates
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][0]);
      glBufferData(GL_ARRAY_BUFFER,
                   vertex_coord[submesh].size() * sizeof(float),
                   vertex_coord[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(0);

      // Layout "1" = vertex normal
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][1]);
      glBufferData(GL_ARRAY_BUFFER, normals[submesh].size() * sizeof(float),
                   normals[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(1);

      if (textures.size() > 0) {
        // Layout "2" = vertex UV
        glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][2]);
        glBufferData(GL_ARRAY_BUFFER,
                     texture_coord[submesh].size() * sizeof(float),
                     texture_coord[submesh].data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                              nullptr);
        glEnableVertexAttribArray(2);
      }

      // Layout "3" joints assignment vector
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][3]);
      glBufferData(GL_ARRAY_BUFFER,
                   joints[submesh].size() * sizeof(unsigned short),
                   joints[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(3, 4, GL_UNSIGNED_SHORT, GL_FALSE,
                            4 * sizeof(unsigned short), nullptr);
      glEnableVertexAttribArray(3);

      // Layout "4" joints weights
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][4]);
      glBufferData(GL_ARRAY_BUFFER, weights[submesh].size() * sizeof(float),
                   weights[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(4);

      // EBO
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBOs[submesh][5]);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   indices[submesh].size() * sizeof(unsigned),
                   indices[submesh].data(), GL_STATIC_DRAW);
      glBindVertexArray(0);
    }

    const auto &material = model.materials[primitive.material];
    if (textures.size() > 0)
      draw_call_descriptor[submesh].main_texture =
          textures[material.values.at("baseColorTexture").TextureIndex()];
  }
}

int main(int argc, char **argv) {
  cxxopts::Options options("gltf-insignt", "glTF data insight tool");

  bool debug_output = false;

  options.add_options()("d,debug", "Enable debugging",
                        cxxopts::value<bool>(debug_output))(
      "i,input", "Input glTF filename", cxxopts::value<std::string>())(
      "h,help", "Show help");

  options.parse_positional({"input", "output"});

  if (argc < 2) {
    std::cout << options.help({"", "group"}) << std::endl;
    return EXIT_FAILURE;
  }

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help({"", "group"}) << std::endl;
    return EXIT_FAILURE;
  }

  if (!result.count("input")) {
    std::cerr << "Input file not specified." << std::endl;
    std::cout << options.help({"", "group"}) << std::endl;
    return EXIT_FAILURE;
  }

  std::string input_filename = result["input"].as<std::string>();

  tinygltf::Model model;
  tinygltf::TinyGLTF gltf_ctx;
  {
    std::string err;
    std::string warn;
    const std::string ext = GetFilePathExtension(input_filename);

    bool ret = false;
    if (ext.compare("glb") == 0) {
      std::cout << "Reading binary glTF" << std::endl;
      // assume binary glTF.
      ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn,
                                        input_filename.c_str());
    } else {
      std::cout << "Reading ASCII glTF" << std::endl;
      // assume ascii glTF.
      ret = gltf_ctx.LoadASCIIFromFile(&model, &err, &warn,
                                       input_filename.c_str());
    }

    if (ret) {
      std::cerr << "Problem while loading gltf:\n"
                << "error: " << err << "\nwarning: " << warn << '\n';
    }
  }

  // Setup window
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    return EXIT_FAILURE;
  }

#ifdef _DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

  GLFWwindow *window =
      glfwCreateWindow(1600, 900, "glTF Insight GUI", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // Enable vsync

  glfwSetKeyCallback(window, key_callback);

  // glad must be called after glfwMakeContextCurrent()

  if (!gladLoadGL()) {
    std::cerr << "Failed to initialize OpenGL context." << std::endl;
    return EXIT_FAILURE;
  }

  if (((GLVersion.major == 2) && (GLVersion.minor >= 1)) ||
      (GLVersion.major >= 3)) {
    // ok
  } else {
    std::cerr << "OpenGL 2.1 is not available." << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "OpenGL " << GLVersion.major << '.' << GLVersion.minor << '\n';

#ifdef _DEBUG
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback((GLDEBUGPROC)glDebugOutput, nullptr);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr,
                        GL_TRUE);
#endif
  glEnable(GL_DEPTH_TEST);

  const auto nb_textures = model.images.size();
  std::vector<GLuint> textures(nb_textures);
  glGenTextures(GLsizei(nb_textures), textures.data());

  for (size_t i = 0; i < textures.size(); ++i) {
    glBindTexture(GL_TEXTURE_2D, textures[i]);
    glTexImage2D(GL_TEXTURE_2D, 0,
                 model.images[i].component == 4 ? GL_RGBA : GL_RGB,
                 model.images[i].width, model.images[i].height, 0,
                 model.images[i].component == 4 ? GL_RGBA : GL_RGB,
                 GL_UNSIGNED_BYTE, model.images[i].image.data());
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  // Setup Dear ImGui context
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable
  // Keyboard Controls io.ConfigFlags |=
  // ImGuiConfigFlags_NavEnableGamepad;
  // // Enable Gamepad Controls

  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL2_Init();

  // Setup Style
  ImGui::StyleColorsDark();

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // sequence with default values
  gltf_insight::AnimSequence mySequence;
  mySequence.mFrameMin = -100;
  mySequence.mFrameMax = 1000;
  mySequence.myItems.push_back(
      gltf_insight::AnimSequence::AnimSequenceItem{0, 10, 30, false});
  mySequence.myItems.push_back(
      gltf_insight::AnimSequence::AnimSequenceItem{1, 20, 30, false});
  mySequence.myItems.push_back(
      gltf_insight::AnimSequence::AnimSequenceItem{3, 12, 60, false});
  mySequence.myItems.push_back(
      gltf_insight::AnimSequence::AnimSequenceItem{2, 61, 90, false});
  mySequence.myItems.push_back(
      gltf_insight::AnimSequence::AnimSequenceItem{4, 90, 99, false});

  bool show_imgui_demo = false;

  // We are bypassing the actual glTF scene here, we are interested in a
  // file that only represent a character with animations. Find the scene
  // node that has a mesh attached to it:
  const auto mesh_node_index = find_main_mesh_node(model);
  if (mesh_node_index < 0) {
    std::cerr << "The loaded gltf file doesn't have any findable mesh";
    return EXIT_SUCCESS;
  }

  // TODO (ybalrid) : refactor loading code outside of int main()
  // Get access to the data
  const auto &mesh_node = model.nodes[mesh_node_index];
  const auto &mesh = model.meshes[mesh_node.mesh];
  const auto &skin = model.skins[mesh_node.skin];
  const auto skeleton = skin.skeleton;
  const auto &primitives = mesh.primitives;
  // I tend to call a "primitive" here a submesh, not to mix them with what
  // OpenGL actually call a "primitive" (point, line, triangle, strip,
  // fan...)
  const auto nb_submeshes = primitives.size();

  const auto nb_joints = skin.joints.size();
  std::vector<glm::mat4> joint_matrices(nb_joints);

  // One : We need to know, for each joint, what is it's inverse bind
  // matrix
  std::map<int, int> joint_inverse_bind_matrix_map;
  for (int i = 0; i < nb_joints; ++i)
    joint_inverse_bind_matrix_map[skin.joints[i]] = i;

  // Two :  we need to get the inverse bind matrix array, as it is
  // necessary for skinning
  const auto &inverse_bind_matrices_accessor =
      model.accessors[skin.inverseBindMatrices];
  assert(inverse_bind_matrices_accessor.type == TINYGLTF_TYPE_MAT4);
  assert(inverse_bind_matrices_accessor.count == nb_joints);

  const auto nb_animations = model.animations.size();
  std::vector<animation> animations(nb_animations);

  load_animations(model, animations);

  const auto &inverse_bind_matrices_bufferview =
      model.bufferViews[inverse_bind_matrices_accessor.bufferView];
  const auto &inverse_bind_matrices_buffer =
      model.buffers[inverse_bind_matrices_bufferview.buffer];
  const size_t inverse_bind_matrices_stride =
      inverse_bind_matrices_accessor.ByteStride(
          inverse_bind_matrices_bufferview);
  const auto inverse_bind_matrices_data_start =
      inverse_bind_matrices_buffer.data.data() +
      inverse_bind_matrices_accessor.byteOffset +
      inverse_bind_matrices_bufferview.byteOffset;
  const size_t inverse_bind_matrices_component_size =
      tinygltf::GetComponentSizeInBytes(
          inverse_bind_matrices_accessor.componentType);
  assert(sizeof(double) >= inverse_bind_matrices_component_size);

  std::vector<glm::mat4> inverse_bind_matrices(nb_joints);

  for (size_t i = 0; i < nb_joints; ++i) {
    if (inverse_bind_matrices_component_size == sizeof(float)) {
      float temp[16];
      memcpy(
          temp,
          inverse_bind_matrices_data_start + i * inverse_bind_matrices_stride,
          inverse_bind_matrices_component_size * 16);
      inverse_bind_matrices[i] = glm::make_mat4(temp);
    }
    // TODO actually, in the glTF spec supports using doubles.
    // We are doing this here because in the tiny_gltf API, it's implied we
    // could have stored doubles. This is unrelated with the fact that
    // numbers in the JSON are read as doubles. Here we are talking about
    // the format where the data is stored inside the binary buffers that
    // comes with the glTF JSON part
    if (inverse_bind_matrices_component_size == sizeof(double)) {
      double temp[16], tempf[16];
      memcpy(
          temp,
          inverse_bind_matrices_data_start + i * inverse_bind_matrices_stride,
          inverse_bind_matrices_component_size * 16);
      for (int j = 0; j < 16; ++j) tempf[j] = float(temp[j]);
      inverse_bind_matrices[i] = glm::make_mat4(tempf);
    }
  }

  // Three : Load the skeleton graph. We need this to calculate the bones
  // world transform
  gltf_node mesh_skeleton_graph(gltf_node::node_type::mesh);
  populate_gltf_skeleton_subgraph(model, mesh_skeleton_graph, skeleton);

  // Four : Get an array that is in the same order as the bones in the
  // glTF to represent the whole skeletons. This is important for the
  // joint matrix calculation
  std::vector<gltf_node *> flat_bone_list;
  create_flat_bone_list(skin, nb_joints, mesh_skeleton_graph, flat_bone_list);
  assert(flat_bone_list.size() == nb_joints);

  // Five : For each animation loaded that is supposed to move the skeleton,
  // associate the animation channel targets with their gltf "node" here:
  for (auto &animation : animations) {
    animation.set_gltf_graph_targets(&mesh_skeleton_graph);

#if defined(DEBUG) || defined(_DEBUG)
    // Animation playing depend on these values being absolutely consistent:
    for (auto &channel : animation.channels) {
      // if this pointer is not null, it means that this channel is moving a
      // node we are displaying:
      if (channel.target_graph_node) {
        assert(channel.target_node ==
               channel.target_graph_node->gltf_model_node_index);
      }
    }
#endif
  }

  // For each submesh, we need to know the draw operation, the VAO to
  // bind, the textures to use and the element count. This array store all
  // of these
  std::vector<draw_call_submesh> draw_call_descriptor(nb_submeshes);

  // Create opengl objects
  std::vector<GLuint> VAOs(nb_submeshes);

  // We have 5 vertex attributes per vertex and one element array buffer
  std::vector<GLuint[6]> VBOs(nb_submeshes);
  glGenVertexArrays(GLsizei(nb_submeshes), VAOs.data());
  for (auto &VBO : VBOs) {
    glGenBuffers(6, VBO);
  }

  // CPU sise storage for all vertex attributes
  std::vector<std::vector<unsigned>> indices(nb_submeshes);
  std::vector<std::vector<float>> vertex_coord(nb_submeshes),
      texture_coord(nb_submeshes), normals(nb_submeshes), weights(nb_submeshes);
  std::vector<std::vector<unsigned short>> joints(nb_submeshes);

  // For each submesh of the mesh, load the data
  load_geometry(model, textures, primitives, draw_call_descriptor, VAOs, VBOs,
                indices, vertex_coord, texture_coord, normals, weights, joints);

  // not doing this seems to break imgui in opengl2 mode...
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  /*glm::mat4 model_matrix{1.f}*/
  glm::mat4 &model_matrix = mesh_skeleton_graph.local_xform;
  glm::mat4 view_matrix{1.f}, projection_matrix{1.f};
  int display_w, display_h;
  glm::vec3 camera_position{0, 0, 3.F};

  struct application_parameters {
    glm::vec3 &camera_position;
    bool button_states[3]{false};
    double last_mouse_x{0}, last_mouse_y{0};
    double rot_pitch{0}, rot_yaw{0};
    double rotation_scale = 0.2;
    application_parameters(glm::vec3 &cam_pos) : camera_position(cam_pos) {}
  } my_user_pointer{camera_position};

  glfwSetWindowUserPointer(window, &my_user_pointer);

  glfwSetMouseButtonCallback(
      window, [](GLFWwindow *window, int button, int action, int mods) {
        auto *param = reinterpret_cast<application_parameters *>(
            glfwGetWindowUserPointer(window));

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
          param->button_states[0] = true;
        if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
          param->button_states[1] = true;
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
          param->button_states[2] = true;
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_FALSE)
          param->button_states[0] = false;
        if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_FALSE)
          param->button_states[1] = false;
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_FALSE)
          param->button_states[2] = false;
      });

  glfwSetCursorPosCallback(window, [](GLFWwindow *window, double mouse_x,
                                      double mouse_y) {
    auto *param = reinterpret_cast<application_parameters *>(
        glfwGetWindowUserPointer(window));

    // mouse left pressed
    if (param->button_states[0] && !ImGui::GetIO().WantCaptureMouse &&
        !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
      param->rot_yaw -= param->rotation_scale * (mouse_x - param->last_mouse_x);
      param->rot_pitch -=
          param->rotation_scale * (mouse_y - param->last_mouse_y);

      param->rot_pitch = glm::clamp(param->rot_pitch, -90.0, +90.0);
    }

    param->last_mouse_x = mouse_x;
    param->last_mouse_y = mouse_y;
  });

  std::string vertex_shader_source_str = R"glsl(
#version 330

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec4 input_joints;
layout (location = 4) in vec4 input_weights;

uniform mat4 mvp;
uniform mat3 normal;

uniform mat4 joint_matrix[$nb_joints];

out vec3 interpolated_normal;
out vec2 interpolated_uv;
out vec4 interpolated_weights;

void main()
{
  mat4 skin_matrix =
    input_weights.x * joint_matrix[int(input_joints.x)]
  + input_weights.y * joint_matrix[int(input_joints.y)]
  + input_weights.z * joint_matrix[int(input_joints.z)]
  + input_weights.w * joint_matrix[int(input_joints.w)];

  gl_Position = mvp * skin_matrix * vec4(input_position, 1.0);

  interpolated_normal = normal * input_normal;
  interpolated_uv = input_uv;
  interpolated_weights = input_weights;
}
)glsl";

  const char *vertex_shader_no_skinning = R"glsl(
#version 330

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec4 input_joints;
layout (location = 4) in vec4 input_weights;

uniform mat4 mvp;
uniform mat3 normal;

out vec3 interpolated_normal;
out vec2 interpolated_uv;

void main()
{
  gl_Position = mvp * vec4(input_position, 1.0);

  interpolated_normal = normal * input_normal;
  interpolated_uv = input_uv;
}
)glsl";

  // Write in shader source code the value of `nb_joints`
  size_t index = vertex_shader_source_str.find("$nb_joints");
  if (index == std::string::npos) {
    std::cerr << "The skinned mesh vertex shader doesn't have the $nb_joints "
                 "token in it's source code anywhere. We cannot do skinning on "
                 "a shader that cannot receive the joint list.\n";
    return EXIT_FAILURE;
  }

  vertex_shader_source_str.replace(index, strlen("$nb_joints"),
                                   std::to_string(nb_joints));
  const char *vertex_shader_source = vertex_shader_source_str.c_str();

  const char *fragment_shader_source_textured = R"glsl(
#version 330

in vec2 interpolated_uv;
in vec3 interpolated_normal;
out vec4 output_color;
uniform sampler2D diffuse_texture;

void main()
{
  output_color = texture(diffuse_texture, interpolated_uv);
}
)glsl";

  const char *fragment_shader_source_draw_debug_color = R"glsl(
#version 330

out vec4 output_color;
uniform vec4 debug_color;

void main()
{
  output_color = debug_color;
}
)glsl";

  const char *fragment_shader_source_uv = R"glsl(
#version 330

out vec4 output_color;
in vec2 interpolated_uv;

void main()
{
  output_color = vec4(interpolated_uv, 0, 1);
}
)glsl";

  const char *fragment_shader_source_normals = R"glsl(
#version 330

in vec3 interpolated_normal;
out vec4 output_color;

void main()
{
  output_color = vec4(interpolated_normal, 1);
}

)glsl";

  const char *fragment_shader_weights = R"glsl(
#version 330

in vec4 interpolated_weights;
out vec4 output_color;

void main()
{
  output_color = interpolated_weights;
}

)glsl";

  std::map<std::string, shader> shaders;
  shaders["textured"] =
      shader("textured", vertex_shader_source, fragment_shader_source_textured);
  shaders["debug_color"] = shader("debug_color", vertex_shader_no_skinning,
                                  fragment_shader_source_draw_debug_color);
  shaders["debug_uv"] =
      shader("debug_uv", vertex_shader_source, fragment_shader_source_uv);
  shaders["debug_normals"] = shader("debug_normals", vertex_shader_source,
                                    fragment_shader_source_normals);
  shaders["no_skinning_tex"] =
      shader("no_skinning_tex", vertex_shader_no_skinning,
             fragment_shader_source_textured);
  shaders["weights"] =
      shader("weights", vertex_shader_source, fragment_shader_weights);

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    std::string shader_to_use;

    {
      ImGui::Begin("Hello, world!");  // Create a window called "Hello,
      // world!" and append into it.

      ImGui::Text("This is some useful text.");  // Display some text (you can
      // use a format strings too)

      ImGui::Checkbox("Show ImGui Demo Window?", &show_imgui_demo);
      if (show_imgui_demo) ImGui::ShowDemoWindow(&show_imgui_demo);

      ImGui::End();

      ImGui::Begin("Shader mode");
      std::vector<std::string> selections;
      static int selected_shader = 0;
      static bool first = true;
      int i = 0;
      for (const auto &shader : shaders) {
        selections.push_back(shader.first);
        if (first && shader.first == "textured") {
          selected_shader = i;
          first = false;
        }
        ++i;
      }
      ImGuiCombo("Choose shader", &selected_shader, selections);
      shader_to_use = selections[selected_shader];
      ImGui::End();
    }

    asset_images_window(textures);

    model_info_window(model);
    animation_window(animations);
    for (size_t i = 0; i < nb_submeshes; ++i) {
      skinning_data_window(i, weights[i], joints[i]);
    }

    //    {
    //      // let's create the sequencer
    //      static int selectedEntry = -1;
    //      static int firstFrame = 0;
    //      static bool expanded = true;
    //      static int currentFrame = 120;
    //      ImGui::SetNextWindowPos(ImVec2(10, 350));
    //
    //      ImGui::SetNextWindowSize(ImVec2(940, 480));
    //      ImGui::Begin("Sequencer");
    //
    //      ImGui::PushItemWidth(130);
    //      ImGui::InputInt("Frame Min", &mySequence.mFrameMin);
    //      ImGui::SameLine();
    //      ImGui::InputInt("Frame ", &currentFrame);
    //      ImGui::SameLine();
    //      ImGui::InputInt("Frame Max", &mySequence.mFrameMax);
    //      ImGui::PopItemWidth();
    //#if 0
    //      Sequencer(
    //          &mySequence, &currentFrame, &expanded, &selectedEntry,
    //          &firstFrame, ImSequencer::SEQUENCER_EDIT_STARTEND |
    //          ImSequencer::SEQUENCER_ADD |
    //              ImSequencer::SEQUENCER_DEL |
    //              ImSequencer::SEQUENCER_COPYPASTE |
    //              ImSequencer::SEQUENCER_CHANGE_FRAME);
    //#else
    //      Sequencer(&mySequence, &currentFrame, &expanded,
    //      &selectedEntry,
    //                &firstFrame, 0);
    //#endif
    //      // add a UI to edit that particular item
    //      if (selectedEntry != -1) {
    //        const gltf_insight::AnimSequence::AnimSequenceItem &item =
    //            mySequence.myItems[selectedEntry];
    //        ImGui::Text("I am a %s, please edit me",
    //                    gltf_insight::SequencerItemTypeNames[item.mType]);
    //        // switch (type) ....
    //      }
    //
    //      ImGui::End();
    //    }

    {
      // Rendering
      glfwGetFramebufferSize(window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
      glEnable(GL_DEPTH_TEST);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      projection_matrix = glm::perspective(
          45.f, float(display_w) / float(display_h), 1.f, 100.f);

      ImGui::Text("camera pitch %f yaw %f", my_user_pointer.rot_pitch,
                  my_user_pointer.rot_yaw);

      glm::quat camera_rotation(
          glm::vec3(glm::radians(my_user_pointer.rot_pitch), 0.f,
                    glm::radians(my_user_pointer.rot_yaw)));
      view_matrix =
          glm::lookAt(camera_rotation * camera_position, glm::vec3(0.f),
                      camera_rotation * glm::vec3(0, 1.f, 0));

      float matrixTranslation[3], matrixRotation[3], matrixScale[3];
      ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model_matrix),
                                            matrixTranslation, matrixRotation,
                                            matrixScale);
      ImGui::InputFloat3("Tr", matrixTranslation, 3);
      ImGui::InputFloat3("Rt", matrixRotation, 3);
      ImGui::InputFloat3("Sc", matrixScale, 3);
      static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
      static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

      if (ImGui::RadioButton("Translate",
                             mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
      ImGui::SameLine();
      if (ImGui::RadioButton("Rotate",
                             mCurrentGizmoOperation == ImGuizmo::ROTATE))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
      ImGui::SameLine();
      if (ImGui::RadioButton("Scale",
                             mCurrentGizmoOperation == ImGuizmo::SCALE))
        mCurrentGizmoOperation = ImGuizmo::SCALE;

      ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation,
                                              matrixScale,
                                              glm::value_ptr(model_matrix));

      ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
      ImGuizmo::Manipulate(glm::value_ptr(view_matrix),
                           glm::value_ptr(projection_matrix),
                           mCurrentGizmoOperation, mCurrentGizmoMode,
                           glm::value_ptr(model_matrix), NULL, NULL);

      update_mesh_skeleton_graph_transforms(mesh_skeleton_graph);
      glm::mat4 inverse_model = glm::inverse(model_matrix);
      for (size_t i = 0; i < joint_matrices.size(); ++i) {
        joint_matrices[i] = inverse_model * flat_bone_list[i]->world_xform *
                            inverse_bind_matrices[i];
      }

      glm::mat4 mvp = projection_matrix * view_matrix * model_matrix;
      glm::mat3 normal = glm::transpose(glm::inverse(model_matrix));

      shaders[shader_to_use].use();
      shaders[shader_to_use].set_uniform("joint_matrix", joint_matrices);
      shaders[shader_to_use].set_uniform("mvp", mvp);
      shaders[shader_to_use].set_uniform("normal", normal);
      shaders[shader_to_use].set_uniform("debug_color",
                                         glm::vec4(0.5f, 0.5f, 0.f, 1.f));

      for (const auto &draw_call_to_perform : draw_call_descriptor) {
        glBindTexture(GL_TEXTURE_2D, draw_call_to_perform.main_texture);
        glBindVertexArray(draw_call_to_perform.VAO);
        glDrawElements(draw_call_to_perform.draw_mode,
                       GLsizei(draw_call_to_perform.count), GL_UNSIGNED_INT, 0);
      }

      glBindVertexArray(0);
      glDisable(GL_DEPTH_TEST);

      if (ImGui::Begin("Skeleton drawing options")) {
        ImGui::Checkbox("Draw joint points", &draw_joint_point);
        ImGui::Checkbox("Draw Bone as segments", &draw_bone_segment);
        ImGui::Checkbox("Draw Bone's base axes", &draw_bone_axes);
        ImGui::Checkbox("Draw Skeleton's Mesh base", &draw_mesh_anchor_point);
      }
      ImGui::End();

      shaders["debug_color"].use();
      draw_bones(mesh_skeleton_graph, shaders["debug_color"].get_program(),
                 view_matrix, projection_matrix);

      glUseProgram(0);  // You may want this if using this code in an
      // OpenGL 3+ context where shaders may be bound, but prefer using
      // the GL3+ code.

      ImGui::Render();
      ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);
    }
  }

  // Cleanup
  ImGui_ImplOpenGL2_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}
