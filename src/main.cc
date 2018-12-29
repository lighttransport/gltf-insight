
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "cxxopts.hpp"

#include "imgui.h"

// imgui
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl2.h"

#include "glad/include/glad/glad.h"

#include "GLFW/glfw3.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "tiny_gltf.h"
#include "tiny_gltf_util.h"

static bool ImGuiCombo(const char* label, int* current_item,
                       const std::vector<std::string>& items) {
    return ImGui::Combo(label, current_item,
        [](void* data, int idx_i, const char** out_text){
            size_t idx = static_cast<size_t>(idx_i);
            const std::vector<std::string>* str_vec =
              reinterpret_cast<std::vector<std::string>*>(data);
            if (idx_i < 0 || str_vec->size() <= idx) {
              return false;
            }
            *out_text = str_vec->at(idx).c_str();
            return true;
        },
        reinterpret_cast<void*>(const_cast<std::vector<std::string> *>(&items)),
        static_cast<int>(items.size()),
        static_cast<int>(items.size()));
}

static void error_callback(int error, const char *description) {
  std::cerr << "GLFW Error : " << error << ", " << description << std::endl;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
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

static void animation_window(const tinygltf::Model &model)
{
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

  int idx = 0;
  ImGuiCombo("animations", &idx, animation_names);

  const tinygltf::Animation &animation = model.animations[size_t(idx)];

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
      int sampler_idx = 0;
      ImGuiCombo("samplers", &sampler_idx, sampler_names);

      const tinygltf::AnimationSampler &sampler = animation.samplers[sampler_idx];

      ImGui::Text("input  [%d]", sampler.input);
      ImGui::Text("output [%d]", sampler.output);

      float min_v, max_v;
      if (tinygltf::util::GetAnimationSamplerInputMinMax(sampler, model, &min_v, &max_v)) {
        ImGui::Text("Keyframe range : %f, %f [secs]", min_v, max_v);

        int count = tinygltf::util::GetAnimationSamplerInputCount(sampler, model);
        ImGui::Text("# of key frames : %d", count);

        const tinygltf::Accessor &accessor = model.accessors[sampler.input];
        for (int k = 0; k < count; k++) {
          float value;
          if (tinygltf::util::DecodeScalarAnimationValue(size_t(k), accessor, model, &value)) {
            // TODO(syoyo): Use table.
            ImGui::Text("%d : %f", k, value);
          }
        }

      } else {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.1f, 1.0f), "input accessor must have min/max property");
      }
    }

    ImGui::TreePop();
  }

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
      int channel_idx = 0;
      ImGuiCombo("channels", &channel_idx, channel_names);

      const tinygltf::AnimationChannel &channel = animation.channels[channel_idx];

      ImGui::Text("sampler [%d]", channel.sampler);
      ImGui::Text("target node [%d] path [%s]", channel.target_node, channel.target_path.c_str());

      const tinygltf::AnimationSampler &sampler = animation.samplers[channel.sampler];
      const tinygltf::Accessor &accessor = model.accessors[sampler.output];
      int count = tinygltf::util::GetAnimationSamplerOutputCount(sampler, model);

      if (channel.target_path.compare("weights") == 0) {
        for (int k = 0; k < count; k++) {
          float value;
          if (tinygltf::util::DecodeScalarAnimationValue(size_t(k), accessor, model, &value)) {
            // TODO(syoyo): Use table.
            ImGui::Text("%d : %f", k, value);
          }
        }
      } else if (channel.target_path.compare("translate") == 0) {
        for (int k = 0; k < count; k++) {
          float xyz[3];
          if (tinygltf::util::DecodeTranslationAnimationValue(size_t(k), accessor, model, xyz)) {
            // TODO(syoyo): Use table.
            ImGui::Text("%d : (%f, %f, %f)", k, xyz[0], xyz[1], xyz[2]);
          }
        }
      } else if (channel.target_path.compare("scale") == 0) {
        for (int k = 0; k < count; k++) {
          float xyz[3];
          if (tinygltf::util::DecodeScaleAnimationValue(size_t(k), accessor, model, xyz)) {
            // TODO(syoyo): Use table.
            ImGui::Text("%d : (%f, %f, %f)", k, xyz[0], xyz[1], xyz[2]);
          }
        }
      } else if (channel.target_path.compare("rotation") == 0) {
        
        for (int k = 0; k < count; k++) {
          float xyzw[4];
          if (tinygltf::util::DecodeRotationAnimationValue(size_t(k), accessor, model, xyzw)) {
            // TODO(syoyo): Use table.
            ImGui::Text("%d : (%f, %f, %f, %f)", k, xyzw[0], xyzw[1], xyzw[2], xyzw[3]);
          }
        }
      } else {
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.1f, 1.0f), "??? Unknown target path name [%s]", channel.target_path.c_str());
      }
    }

    ImGui::TreePop();
  }


  ImGui::End();
}

static std::string GetFilePathExtension(const std::string &FileName) {
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

int main(int argc, char **argv) {

  cxxopts::Options options("gltf-insignt", "glTF data insight tool");

  bool debug_output = false;

  options.add_options()
    ("d,debug", "Enable debugging", cxxopts::value<bool>(debug_output))
    ("i,input", "Input glTF filename", cxxopts::value<std::string>())
    ("h,help", "Show help")
    ;

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
    ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn, input_filename.c_str());
  } else {
    std::cout << "Reading ASCII glTF" << std::endl;
    // assume ascii glTF.
    ret = gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, input_filename.c_str());
  }


  

  // Setup window
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    return EXIT_FAILURE;
  }

  GLFWwindow *window = glfwCreateWindow(1600, 900, "glTF Insight GUI", nullptr, nullptr);
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
      ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

      ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

      ImGui::End();
    }

    animation_window(model);

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
