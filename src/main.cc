
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

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/matrix.hpp"
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

  std::cout << "---------------" << std::endl;
  std::cout << "Debug message (" << id << "): " << message << std::endl;

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
  }
  std::cout << std::endl;

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
  }
  std::cout << std::endl;

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
  }
  std::cout << std::endl;
  std::cout << std::endl;
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
  std::vector<GLuint> textures;
  glGenTextures(nb_textures, textures.data());

  for (size_t i = 0; i < textures.size(); ++i) {
    glBindTexture(GL_TEXTURE_2D, textures[i]);
    glTexImage2D(GL_TEXTURE_2D, 0,
                 model.images[i].component == 4 ? GL_RGBA : GL_RGB,
                 model.images[i].width, model.images[i].height, 0,
                 model.images[i].component == 4 ? GL_RGBA : GL_RGB,
                 GL_UNSIGNED_BYTE, model.images[i].image.data());
    glGenerateMipmap(GL_UNSIGNED_BYTE);
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

  const auto mesh_node_index = find_main_mesh_node(model);
  if (mesh_node_index < 0) {
    std::cerr << "The loaded gltf file doesn't have any findable mesh";
    return EXIT_SUCCESS;
  }

  const auto &mesh_node = model.nodes[mesh_node_index];
  const auto &mesh = model.meshes[mesh_node.mesh];
  const auto &primitives = mesh.primitives;
  const auto nb_submeshes = primitives.size();

  std::vector<draw_call_submesh> draw_call_descriptor(nb_submeshes);
  std::vector<GLuint> VAOs(nb_submeshes);
  std::vector<GLuint[4]> VBOs(nb_submeshes);
  glGenVertexArrays(nb_submeshes, VAOs.data());
  for (auto &VBO : VBOs) {
    glGenBuffers(4, VBO);
  }
  std::vector<std::vector<unsigned>> indices(nb_submeshes);
  std::vector<std::vector<float>> vertex_coord(nb_submeshes),
      texture_coord(nb_submeshes), normals(nb_submeshes);

  for (size_t submesh = 0; submesh < nb_submeshes; ++submesh) {
    const auto &primitive = primitives[submesh];
    draw_call_descriptor[submesh].VAO = VAOs[submesh];
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
    {
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
            texture_coord[submesh][i * 2 + j] = float(temp[j]);  // downcast to
                                                                 // float
          }
        } else if (byte_size_of_component == sizeof(float)) {
          memcpy(&texture_coord[submesh][i * 2],
                 texture_start_pointer + i * texture_stride,
                 byte_size_of_component * 2);
        }
      }
    }

    {
      glBindVertexArray(VAOs[submesh]);
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][0]);
      glBufferData(GL_ARRAY_BUFFER,
                   vertex_coord[submesh].size() * sizeof(float),
                   vertex_coord[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(0);

      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][1]);
      glBufferData(GL_ARRAY_BUFFER, normals[submesh].size() * sizeof(float),
                   normals[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(1);

      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][2]);
      glBufferData(GL_ARRAY_BUFFER,
                   texture_coord[submesh].size() * sizeof(float),
                   texture_coord[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(2);

      // TODO Weight data? Tangent? Bitangent?

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBOs[submesh][3]);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   indices[submesh].size() * sizeof(unsigned),
                   indices[submesh].data(), GL_STATIC_DRAW);
      glBindVertexArray(0);
    }

    const auto &material = model.materials[primitive.material];
    draw_call_descriptor[submesh].main_texture =
        material.values.at("baseColorTexture").TextureIndex();
  }

  // not doing this seems to break imgui in opengl2 mode...
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  glm::mat4 model_matrix{1.f}, view_matrix{1.f}, projection_matrix{1.f}, mvp,
      normal;
  glm::vec3 camera_position{1, 0, 5};
  int display_w, display_h;

  const char *vertex_shader_source = R"glsl(
#version 330 core

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;

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

  const char *fragment_shader_source = R"glsl(
#version 330 core

in vec2 interpolated_uv;
in vec3 interpolated_normal;

out vec4 output_color;

uniform sampler2D diffuse_texture;

void main()
{
  //output_color = texture(diffuse_texture, interpolated_uv);

  output_color.rg = interpolated_uv;
  output_color.ba = vec2(0,1);
}
)glsl";

  GLint vertex_shader, fragment_shader;
  vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(vertex_shader, 1,
                 static_cast<const GLchar *const *>(&vertex_shader_source),
                 nullptr);
  glShaderSource(fragment_shader, 1,
                 static_cast<const GLchar *const *>(&fragment_shader_source),
                 nullptr);

  glCompileShader(vertex_shader);
  glCompileShader(fragment_shader);

  // check shader compilation
  GLint success = 0;
  GLchar info_log[512];

  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex_shader, sizeof info_log, nullptr, info_log);
    std::cerr << info_log << '\n';
    return EXIT_FAILURE;
  }
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment_shader, sizeof info_log, nullptr, info_log);
    std::cerr << info_log << '\n';
    return EXIT_FAILURE;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
    std::cerr << info_log << '\n';
    return EXIT_FAILURE;
  }

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

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    {
      ImGui::Begin("Hello, world!");  // Create a window called "Hello,
                                      // world!" and append into it.

      ImGui::Text("This is some useful text.");  // Display some text (you can
                                                 // use a format strings too)

      ImGui::Checkbox("Show ImGui Demo Window?", &show_imgui_demo);
      if (show_imgui_demo) ImGui::ShowDemoWindow(&show_imgui_demo);

      ImGui::SliderFloat3("Camera position", glm::value_ptr(camera_position),
                          -10, 10);

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
      glfwGetFramebufferSize(window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      projection_matrix = glm::perspective(
          45.f, float(display_w) / float(display_h), 1.f, 100.f);

      view_matrix =
          glm::lookAt(camera_position, glm::vec3(0.f), glm::vec3(0, 1.f, 0));

      // ImGuizmo::Manipulate(glm::value_ptr(view_matrix),
      //                     glm::value_ptr(projection_matrix),
      //                     ImGuizmo::OPERATION::)

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

      ImGuiIO &io = ImGui::GetIO();
      ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
      ImGuizmo::Manipulate(glm::value_ptr(view_matrix),
                           glm::value_ptr(projection_matrix),
                           mCurrentGizmoOperation, mCurrentGizmoMode,
                           glm::value_ptr(model_matrix), NULL, NULL);

      glm::mat4 mvp = projection_matrix * view_matrix * model_matrix;
      glm::mat3 normal = glm::transpose(glm::inverse(model_matrix));

      glUseProgram(program);
      glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE,
                         glm::value_ptr(mvp));
      glUniformMatrix3fv(glGetUniformLocation(program, "normal"), 1, GL_FALSE,
                         glm::value_ptr(normal));

      for (const auto &draw_call_to_perform : draw_call_descriptor) {
        glBindTexture(GL_TEXTURE_2D, draw_call_to_perform.main_texture);
        glBindVertexArray(draw_call_to_perform.VAO);
        glDrawElements(draw_call_to_perform.draw_mode,
                       draw_call_to_perform.count, GL_UNSIGNED_INT, 0);
      }
      glBindVertexArray(0);
      glUseProgram(0);  // You may want this if using this code in an
      // OpenGL 3+ context where shaders may be bound, but prefer using the
      // GL3+ code.

      // TODO render overlays on top of the meshes (bones...)

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
