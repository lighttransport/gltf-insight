#pragma once

#include "GLFW/glfw3.h"
#include "ImCurveEdit.h"
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

void asset_images_window(const std::vector<GLuint> &textures) {
  if (ImGui::Begin("glTF Images", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Number of textures [%zu]", textures.size());
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

    ImGui::Text("Main node with a mesh [%d]", main_node_index);
    const auto &main_node = model.nodes[main_node_index];

    if (main_node.skin >= 0) {
      // mesh.weights
      ImGui::Text("Main mesh uses skin [%d]", main_node.skin);
      const auto &skin = model.skins[main_node.skin];
      ImGui::Text("Skin [%d] skeleton root node [%d]", main_node.skin,
                  skin.skeleton);
      ImGui::Text("Skin joint count [%zu]", skin.joints.size());
      // if (ImGui::CollapsingHeader("Skeleton topology"))
      //  describe_node_topology_in_imgui_tree(model, skin.skeleton);
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

  ImGui::Text("Min time [%f]", animation.min_time);
  ImGui::Text("Max time [%f]", animation.max_time);

  //
  // channels
  //
  if (ImGui::CollapsingHeader("channels")) {
    ImGui::Text("Animation has [%zu] channels", animation.channels.size());
    static int channel_idx = 0;
    ImGui::InputInt("channel", &channel_idx, 1, 1);
    channel_idx = std::max<int>(
        std::min<int>(channel_idx, int(animation.channels.size()) - 1), 0);
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
        ImGui::Separator(), ImGui::Text("Frame");
        ImGui::NextColumn(), ImGui::Text("Weight Value");
        ImGui::NextColumn(), ImGui::Separator();
        for (auto keyframe : channel.keyframes) {
          {
            ImGui::Text("%d", keyframe.first);
            ImGui::NextColumn(),
                ImGui::Text("%f", keyframe.second.motion.weight);
            ImGui::NextColumn(), ImGui::Separator();
          }
        }
        ImGui::Columns();
        break;
      case animation::channel::path::translation:
        ImGui::Columns(4);
        ImGui::Separator(), ImGui::Text("Frame");
        ImGui::NextColumn(), ImGui::Text("X");
        ImGui::NextColumn(), ImGui::Text("Y");
        ImGui::NextColumn(), ImGui::Text("Z");
        ImGui::NextColumn(), ImGui::Separator();
        for (auto keyframe : channel.keyframes) {
          ImGui::Text("%d", keyframe.first);
          ImGui::NextColumn();
          auto v = keyframe.second.motion.translation;
          ImGui::Text("%f", v.x);
          ImGui::NextColumn(), ImGui::Text("%f", v.y);
          ImGui::NextColumn(), ImGui::Text("%f", v.z);
          ImGui::NextColumn(), ImGui::Separator();
        }
        ImGui::Columns();
        break;
      case animation::channel::path::scale:
        ImGui::Columns(4);
        ImGui::Separator(), ImGui::Text("Frame");
        ImGui::NextColumn();
        ImGui::Text("X"), ImGui::NextColumn();
        ImGui::Text("Y"), ImGui::NextColumn();
        ImGui::Text("Z"), ImGui::NextColumn();
        ImGui::Separator();
        for (auto keyframe : channel.keyframes) {
          ImGui::Text("%d", keyframe.first);
          ImGui::NextColumn();
          auto v = keyframe.second.motion.scale;
          ImGui::Text("%f", v.x);
          ImGui::NextColumn(), ImGui::Text("%f", v.y);
          ImGui::NextColumn(), ImGui::Text("%f", v.z);
          ImGui::NextColumn(), ImGui::Separator();
        }
        ImGui::Columns();
        break;
      case animation::channel::path::rotation:
        ImGui::Columns(5);
        ImGui::Separator(), ImGui::Text("Frame");
        ImGui::NextColumn(), ImGui::Text("X");
        ImGui::NextColumn(), ImGui::Text("Y");
        ImGui::NextColumn(), ImGui::Text("Z");
        ImGui::NextColumn(), ImGui::Text("W");
        ImGui::NextColumn(), ImGui::Separator();
        for (auto keyframe : channel.keyframes) {
          ImGui::Text("%d", keyframe.first);
          ImGui::NextColumn();
          auto quat = keyframe.second.motion.rotation;
          ImGui::Text("%f", quat.x);
          ImGui::NextColumn(), ImGui::Text("%f", quat.y);
          ImGui::NextColumn(), ImGui::Text("%f", quat.z);
          ImGui::NextColumn(), ImGui::Text("%f", quat.w);
          ImGui::NextColumn(), ImGui::Separator();
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

    ImGui::Text("Animation has [%zu] samplers", animation.samplers.size());
    static int sampler_idx = 0;
    ImGui::InputInt("sampler", &sampler_idx, 1, 1);
    sampler_idx = std::max<int>(
        std::min<int>(sampler_idx, int(animation.samplers.size()) - 1), 0);
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
    ImGui::Text("# of key frames : %zu", sampler.keyframes.size());

    ImGui::Columns(2), ImGui::Text("Frame Number");
    ImGui::NextColumn(), ImGui::Text("Timestamp");
    ImGui::NextColumn(), ImGui::Separator();
    for (auto keyframe : sampler.keyframes) {
      ImGui::Text("%d", keyframe.first);
      ImGui::NextColumn(), ImGui::Text("%f", keyframe.second);
      ImGui::NextColumn(), ImGui::Separator();
    }
    ImGui::Columns();
  }

  ImGui::End();
}

void skinning_data_window(
    const std::vector<std::vector<float>> &weights,
    const std::vector<std::vector<unsigned short>> &joints) {
  ImGui::Begin("Skinning data", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  int submesh_id;
  ImGui::InputInt("Submesh #", &submesh_id, 1, 1);
  submesh_id = glm::clamp<int>(submesh_id, 0, weights.size());
  ImGui::Text("Submesh[%zu]", submesh_id);
  ImGui::BeginChild("##scrollable_data_region", ImVec2(600, 800), false,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  const auto vertex_count = weights[submesh_id].size() / 4;
  assert(vertex_count == joints[submesh_id].size() / 4);

  const auto &weight = weights[submesh_id];
  const auto &joint = joints[submesh_id];

  ImGui::Columns(5);
  ImGui::Text("Vertex Index");
  ImGui::NextColumn(), ImGui::Text("0");
  ImGui::NextColumn(), ImGui::Text("1");
  ImGui::NextColumn(), ImGui::Text("2");
  ImGui::NextColumn(), ImGui::Text("3");
  ImGui::NextColumn();
  ImGui::Separator();

  for (size_t i = 0; i < vertex_count; ++i) {
    ImGui::Text("%zu", i);
    ImGui::NextColumn(), ImGui::Text("%f * %d", weight[i * 4], joint[i * 4]);
    ImGui::NextColumn(),
        ImGui::Text("%f * %d", weight[i * 4 + 1], joint[i * 4 + 1]);
    ImGui::NextColumn(),
        ImGui::Text("%f * %d", weight[i * 4 + 2], joint[i * 4 + 2]);
    ImGui::NextColumn(),
        ImGui::Text("%f * %d", weight[i * 4 + 3], joint[i * 4 + 3]);
    ImGui::NextColumn(), ImGui::Separator();
  }
  ImGui::Columns();
  ImGui::EndChild();
  ImGui::End();
}

void morph_window(gltf_node &mesh_skeleton_graph, int nb_morph_targets) {
  if (ImGui::Begin("Morph Target blend weights")) {
    for (int w = 0; w < nb_morph_targets; ++w) {
      const std::string name = "Morph Target [" + std::to_string(w) + "]";
      ImGui::SliderFloat(
          name.c_str(), &mesh_skeleton_graph.pose.blend_weights[w], 0, 1, "%f");
      mesh_skeleton_graph.pose.blend_weights[w] =
          glm::clamp(mesh_skeleton_graph.pose.blend_weights[w], 0.f, 1.f);
    }
  }
  ImGui::End();
}

void initialize_glfw_opengl_window(GLFWwindow *&window) {
  // Setup window
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    exit(EXIT_FAILURE);
  }

#ifdef _DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

  window = glfwCreateWindow(1600, 900, "glTF Insight GUI", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // Enable vsync
  glfwSetKeyCallback(window, key_callback);

  // glad must be called after glfwMakeContextCurrent()
  if (!gladLoadGL()) {
    std::cerr << "Failed to initialize OpenGL context." << std::endl;
    exit(EXIT_FAILURE);
  }

  if (((GLVersion.major == 2) && (GLVersion.minor >= 1)) ||
      (GLVersion.major >= 3)) {
    // ok
  } else {
    std::cerr << "OpenGL 2.1 is not available." << std::endl;
    exit(EXIT_FAILURE);
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
}

void initialize_imgui(GLFWwindow *window) {
  // Setup Dear ImGui context
  ImGui::CreateContext();
  auto io = ImGui::GetIO();
  (void)io;
  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL2_Init();
  // Setup Style
  ImGui::StyleColorsDark();
}
