
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

// For OpenGL, we always include loader library first
#include "glad/include/glad/glad.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "ImCurveEdit.h"
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

static int find_mesh_with_node_in_children(const tinygltf::Model &model,
                                           int root) {
  const auto &root_node = model.nodes[root];
  if (root_node.mesh >= 0) return root;

  for (auto child : root_node.children) {
    const auto result = find_mesh_with_node_in_children(model, child);
    if (result > 0 && model.nodes[result].mesh >= 0) return result;
  }

  return -1;
}

static int find_main_mesh_node(const tinygltf::Model &model) {
  const auto &node_list =
      model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0].nodes;

  for (auto node : node_list) {
    const auto mesh_node = find_mesh_with_node_in_children(model, node);
    if (mesh_node >= 0) return mesh_node;
  }

  return -1;
}

static void describe_node_topology(const tinygltf::Model &model,
                                   int node_index) {
  std::string node_desc = "node [" + std::to_string(node_index) + "]";
  if (ImGui::TreeNode(node_desc.c_str())) {
    const auto &node = model.nodes[node_index];
    if (!node.name.empty()) ImGui::Text("name [%s]", node.name.c_str());

    for (auto child : node.children) {
      describe_node_topology(model, child);
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
        describe_node_topology(model, skin.skeleton);
    }

    // TODO check if the found node with a mesh has morph targets, and
    // describe them in this window here
  }
  ImGui::End();
}

static void animation_window(const tinygltf::Model &model) {
  // TODO(syoyo): Cache values

  ImGui::Begin("Animations");

  if (model.animations.size() == 0) {
    ImGui::Text("No animations in glTF");
    ImGui::End();
    return;
  }

  std::vector<std::string> animation_names;

  for (size_t i = 0; i < model.animations.size(); i++) {
    const tinygltf::Animation &animation = model.animations[i];
    std::string name = animation.name;
    if (name.empty()) {
      name = "animation_" + std::to_string(i);
    }
    animation_names.push_back(name);
  }

  static int idx = 0;
  ImGuiCombo("animations", &idx, animation_names);

  const tinygltf::Animation &animation = model.animations[size_t(idx)];

  //
  // channels
  //
  if (ImGui::TreeNode("channels")) {
    std::vector<std::string> channel_names;

    for (size_t i = 0; i < animation.channels.size(); i++) {
      std::string name = "channel_" + std::to_string(i);
      channel_names.push_back(name);
    }

    if (channel_names.empty()) {
      ImGui::Text("??? no channels in animation.");
    } else {
      static int channel_idx = 0;
      ImGuiCombo("channels", &channel_idx, channel_names);

      const tinygltf::AnimationChannel &channel =
          animation.channels[channel_idx];

      ImGui::Text("sampler [%d]", channel.sampler);
      ImGui::Text("target node [%d] path [%s]", channel.target_node,
                  channel.target_path.c_str());

      const tinygltf::AnimationSampler &sampler =
          animation.samplers[channel.sampler];
      ImGui::Text("interpolation mode [%s]", sampler.interpolation.c_str());

      const tinygltf::Accessor &accessor = model.accessors[sampler.output];
      int count =
          tinygltf::util::GetAnimationSamplerOutputCount(sampler, model);

      if (channel.target_path.compare("weights") == 0) {
        ImGui::Columns(2);
        ImGui::Separator();
        ImGui::Text("Frame");
        ImGui::NextColumn();
        ImGui::Text("Weight Value");
        ImGui::NextColumn();
        ImGui::Separator();
        for (int k = 0; k < count; k++) {
          float value;
          if (tinygltf::util::DecodeScalarAnimationValue(size_t(k), accessor,
                                                         model, &value)) {
            ImGui::Text("%d", k);
            ImGui::NextColumn();
            ImGui::Text("%f", value);
            ImGui::NextColumn();
            ImGui::Separator();
          }
        }
        ImGui::Columns();
      } else if (channel.target_path.compare("translation") == 0) {
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
        for (int k = 0; k < count; k++) {
          float xyz[3];
          if (tinygltf::util::DecodeTranslationAnimationValue(
                  size_t(k), accessor, model, xyz)) {
            ImGui::Text("%d", k);
            ImGui::NextColumn();
            ImGui::Text("%f", xyz[0]);
            ImGui::NextColumn();
            ImGui::Text("%f", xyz[1]);
            ImGui::NextColumn();
            ImGui::Text("%f", xyz[2]);
            ImGui::NextColumn();
            ImGui::Separator();
          }
        }
        ImGui::Columns();
      } else if (channel.target_path.compare("scale") == 0) {
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
        for (int k = 0; k < count; k++) {
          float xyz[3];
          if (tinygltf::util::DecodeScaleAnimationValue(size_t(k), accessor,
                                                        model, xyz)) {
            ImGui::Text("%d", k);
            ImGui::NextColumn();
            ImGui::Text("%f", xyz[0]);
            ImGui::NextColumn();
            ImGui::Text("%f", xyz[1]);
            ImGui::NextColumn();
            ImGui::Text("%f", xyz[2]);
            ImGui::NextColumn();
            ImGui::Separator();
          }
        }
        ImGui::Columns();
      } else if (channel.target_path.compare("rotation") == 0) {
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
        for (int k = 0; k < count; k++) {
          float xyzw[4];
          if (tinygltf::util::DecodeRotationAnimationValue(size_t(k), accessor,
                                                           model, xyzw)) {
            ImGui::Text("%d", k);
            ImGui::NextColumn();
            ImGui::Text("%f", xyzw[0]);
            ImGui::NextColumn();
            ImGui::Text("%f", xyzw[1]);
            ImGui::NextColumn();
            ImGui::Text("%f", xyzw[2]);
            ImGui::NextColumn();
            ImGui::Text("%f", xyzw[3]);
            ImGui::NextColumn();
            ImGui::Separator();
          }
        }
        ImGui::Columns();
      } else {
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.1f, 1.0f),
                           "??? Unknown target path name [%s]",
                           channel.target_path.c_str());
      }
    }

    ImGui::TreePop();
  }

  //
  // samplers
  //
  if (ImGui::TreeNode("samplers")) {
    std::vector<std::string> sampler_names;

    for (size_t i = 0; i < animation.samplers.size(); i++) {
      std::string name = "sampler_" + std::to_string(i);
      sampler_names.push_back(name);
    }

    if (sampler_names.empty()) {
      ImGui::Text("??? no samplers in animation.");
    } else {
      static int sampler_idx = 0;
      ImGuiCombo("samplers", &sampler_idx, sampler_names);

      const tinygltf::AnimationSampler &sampler =
          animation.samplers[sampler_idx];

      ImGui::Text("input  [%d]", sampler.input);
      ImGui::Text("output [%d]", sampler.output);
      ImGui::Text("Interpolation method [%s]", sampler.interpolation.c_str());

      float min_v, max_v;
      if (tinygltf::util::GetAnimationSamplerInputMinMax(sampler, model, &min_v,
                                                         &max_v)) {
        ImGui::Text("Keyframe range : %f, %f [secs]", min_v, max_v);

        int count =
            tinygltf::util::GetAnimationSamplerInputCount(sampler, model);
        ImGui::Text("# of key frames : %d", count);

        const tinygltf::Accessor &accessor = model.accessors[sampler.input];
        ImGui::Columns(2);
        ImGui::Text("Frame Number");
        ImGui::NextColumn();
        ImGui::Text("Timestamp");
        ImGui::NextColumn();
        ImGui::Separator();
        for (int k = 0; k < count; k++) {
          float value;
          if (tinygltf::util::DecodeScalarAnimationValue(size_t(k), accessor,
                                                         model, &value)) {
            ImGui::Text("%d", k);
            ImGui::NextColumn();
            ImGui::Text("%f", value);
            ImGui::NextColumn();
            ImGui::Separator();
          }
        }
        ImGui::Columns();

      } else {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.1f, 1.0f),
                           "input accessor must have min/max property");
      }
    }

    ImGui::TreePop();
  }

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
  std::string err;
  std::string warn;
  std::string ext = GetFilePathExtension(input_filename);

  bool ret = false;
  if (ext.compare("glb") == 0) {
    std::cout << "Reading binary glTF" << std::endl;
    // assume binary glTF.
    ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn,
                                      input_filename.c_str());
  } else {
    std::cout << "Reading ASCII glTF" << std::endl;
    // assume ascii glTF.
    ret =
        gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, input_filename.c_str());
  }

  // Setup window
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    return EXIT_FAILURE;
  }

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

  // Setup Dear ImGui context
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard
  // Controls io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable
  // Gamepad Controls

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
  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
    // your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    // data to your main application. Generally you may always pass all inputs
    // to dear imgui, and hide them from your application based on those two
    // flags.
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
      ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!"
                                      // and append into it.

      ImGui::Text("This is some useful text.");  // Display some text (you can
                                                 // use a format strings too)

      ImGui::Checkbox("Show ImGui Demo Window?", &show_imgui_demo);
      if (show_imgui_demo) ImGui::ShowDemoWindow(&show_imgui_demo);

      ImGui::End();
    }

    model_info_window(model);
    animation_window(model);

    {
      // let's create the sequencer
      static int selectedEntry = -1;
      static int firstFrame = 0;
      static bool expanded = true;
      static int currentFrame = 120;
      ImGui::SetNextWindowPos(ImVec2(10, 350));

      ImGui::SetNextWindowSize(ImVec2(940, 480));
      ImGui::Begin("Sequencer");

      ImGui::PushItemWidth(130);
      ImGui::InputInt("Frame Min", &mySequence.mFrameMin);
      ImGui::SameLine();
      ImGui::InputInt("Frame ", &currentFrame);
      ImGui::SameLine();
      ImGui::InputInt("Frame Max", &mySequence.mFrameMax);
      ImGui::PopItemWidth();
#if 0
      Sequencer(
          &mySequence, &currentFrame, &expanded, &selectedEntry, &firstFrame,
          ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_ADD |
              ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_COPYPASTE |
              ImSequencer::SEQUENCER_CHANGE_FRAME);
#else
      Sequencer(&mySequence, &currentFrame, &expanded, &selectedEntry,
                &firstFrame, 0);
#endif
      // add a UI to edit that particular item
      if (selectedEntry != -1) {
        const gltf_insight::AnimSequence::AnimSequenceItem &item =
            mySequence.myItems[selectedEntry];
        ImGui::Text("I am a %s, please edit me",
                    gltf_insight::SequencerItemTypeNames[item.mType]);
        // switch (type) ....
      }

      ImGui::End();
    }

    {
      // Rendering
      ImGui::Render();
      int display_w, display_h;
      glfwGetFramebufferSize(window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
      glClear(GL_COLOR_BUFFER_BIT);
      // glUseProgram(0); // You may want this if using this code in an OpenGL
      // 3+ context where shaders may be bound, but prefer using the GL3+ code.
      ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

      glfwMakeContextCurrent(window);
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
