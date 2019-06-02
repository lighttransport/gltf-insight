#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

// This includes opengl for us, along side debuging callbacks
#include "gl_util.hh"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

// imgui
#include "gui_util.hh"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

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
#include "shader.hpp"
#include "tiny_gltf.h"
#include "tiny_gltf_util.h"

// skeleton_index is the first node that will be added as a child of
// "graph_root" e.g: a gltf node has a mesh. That mesh has a skin, and that
// skin as a node index as "skeleton". You need to pass that "skeleton"
// integer to this function as skeleton_index. This returns a flat array of
// the bones to be used by the skinning code
void populate_gltf_skeleton_subgraph(const tinygltf::Model &model,
                                     gltf_node &graph_root,
                                     int skeleton_index) {
  const auto &skeleton_node = model.nodes[skeleton_index];

  // Holder for the data.
  glm::mat4 xform(1.f);
  glm::vec3 translation(0.f), scale(1.f, 1.f, 1.f);
  glm::quat rotation(1.f, 0.f, 0.f, 0.f);

  // A node can store both a transform matrix and separate translation, rotation
  // and scale. We need to load them if they are present. tiny_gltf signal this
  // by either having empty vectors, or vectors of the expected size.
  const auto &node_matrix = skeleton_node.matrix;
  if (node_matrix.size() == 16)  // 4x4 matrix
  {
    double tmp[16];
    float tmpf[16];
    memcpy(tmp, skeleton_node.matrix.data(), 16 * sizeof(double));

    // Convert a double array to a float array
    for (int i = 0; i < 16; ++i) {
      tmpf[i] = float(tmp[i]);
    }

    // Both glm matrices and this float array have the same data layout. We can
    // pass the pointer to glm::make_mat4
    xform = glm::make_mat4(tmpf);
  }

  // Do the same for translation rotation and scale.
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

  // In the case that the node has both the matrix and the individual vectors,
  // both transform has to be applied.
  xform = xform * reconstructed_matrix;

  graph_root.add_child_bone(xform);
  auto &new_bone = *graph_root.children.back().get();
  new_bone.gltf_model_node_index = skeleton_index;

  for (int child : skeleton_node.children) {
    populate_gltf_skeleton_subgraph(model, new_bone, child);
  }
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
  // assert(bone_array.size() == skin_object.joints.size());
  for (size_t counter = 0;
       counter < std::min(bone_array.size(), skin_object.joints.size());
       ++counter) {
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

// This is useful because mesh.skeleton isn't required to point to the skeleton
// root. We still want to find the skeleton root, so we are going to search for
// it by hand.
int find_skeleton_root(const tinygltf::Model &model,
                       const std::vector<int> &joints, int start_node = 0) {
  // Get the node to get the children
  const auto &node = model.nodes[start_node];

  for (int child : node.children) {
    // If we are part of the skeleton, return our parent
    for (int joint : joints) {
      if (joint == child) return joint;
    }
    // try to find in children, if found return the child's child's parent (so,
    // here, the retunred value by find_skeleton_root)
    const int result = find_skeleton_root(model, joints, child);
    if (result != -1) return result;
  }

  // Here it means we found nothing, return "nothing"
  return -1;
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
  mySequence.mFrameMin = 0;
  mySequence.mFrameMax = 100;
  /*
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
  */
  bool show_imgui_demo = false;

  // We are bypassing the actual glTF scene here, we are interested in a
  // file that only represent a character with animations. Find the scene
  // node that has a mesh attached to it:
  const auto mesh_node_index = find_main_mesh_node(model);
  if (mesh_node_index < 0) {
    std::cerr << "The loaded gltf file doesn't have any findable mesh\n";
    return EXIT_FAILURE;
  }

  // TODO (ybalrid) : refactor loading code outside of int main()
  // Get access to the data
  const auto &mesh_node = model.nodes[mesh_node_index];
  const auto &mesh = model.meshes[mesh_node.mesh];
  if (mesh_node.skin < 0) {
    std::cerr << "The loaded gltf file doesn't have any skin in the mesh\n";
    return EXIT_FAILURE;
  }
  const auto &skin = model.skins[mesh_node.skin];
  auto skeleton = skin.skeleton;

  while (skeleton == -1) {
    for (int node : model.scenes[0].nodes) {
      skeleton = find_skeleton_root(model, skin.joints, node);
      if (skeleton != -1) {
        // todo check skeleton root
        break;
      }
    }
  }

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

  for (int i = 0; i < nb_animations; ++i) {
    mySequence.myItems.push_back(gltf_insight::AnimSequence::AnimSequenceItem{
        0, 60 * int(animations[i].min_time), 60 * int(animations[i].max_time),
        false});
  }

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
    // TODO actually, in the glTF spec there's no mention of  supports for
    // doubles. We are doing this here because in the tiny_gltf API, it's
    // implied we could have stored doubles. This is unrelated with the fact
    // that numbers in the JSON are read as doubles. Here we are talking about
    // the format where the data is stored inside the binary buffers that
    // comes with the glTF JSON part. Maybe remove the "double" types from
    // tiny_gltf?
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
  // assert(flat_bone_list.size() == nb_joints);

  // Five : For each animation loaded that is supposed to move the skeleton,
  // associate the animation channel targets with their gltf "node" here:
  for (auto &animation : animations) {
    animation.set_gltf_graph_targets(&mesh_skeleton_graph);

#if 0 && (defined(DEBUG) || defined(_DEBUG))
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

  // Load morph targets:
  // TODO we can probably only have one loop going through the submeshes
  std::vector<std::vector<morph_target>> morph_targets(nb_submeshes);
  for (int i = 0; i < nb_submeshes; ++i) {
    morph_targets[i].resize(mesh.primitives[i].targets.size());
    load_morph_targets(model, mesh.primitives[i], morph_targets[i]);
  }

  // Set the number of weights to animate
  int nb_morph_targets = 0;
  for (auto &target : morph_targets) {
    nb_morph_targets = std::max<int>(target.size(), nb_morph_targets);
  }

  // Create an array of blend weights in the main node, and fill it with zeroes
  mesh_skeleton_graph.pose.blend_weights.resize(nb_morph_targets);
  std::generate(mesh_skeleton_graph.pose.blend_weights.begin(),
                mesh_skeleton_graph.pose.blend_weights.end(),
                [] { return 0.f; });

  // TODO technically there's a "default" pose of the glTF asset in therm of the
  // value of theses weights. They are set to zero by default, but still.

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

  // create a copy of the data for morphing. We will repeatidly upload theses
  // arrays to the GPU, while preserving the value of the original vertex
  // coordinates. Morph targets are only deltas from the default position.
  auto display_position = vertex_coord;
  auto display_normal = normals;

  // Not doing this seems to break imgui in opengl2 mode...
  // TODO maybe just move to the OpenGL 3.x code. This could help debuging as
  // tools like nvidia nsights are less usefull on old style opengl
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // Objects to store application display state
  glm::mat4 &model_matrix = mesh_skeleton_graph.local_xform;
  glm::mat4 view_matrix{1.f}, projection_matrix{1.f};
  int display_w, display_h;
  glm::vec3 camera_position{0, 0, 3.F};

  // We need to pass this to glfw to have mouse control
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

  std::map<std::string, shader> shaders;
  load_shaders(nb_joints, shaders);

  // Main loop
  double last_frame_time = glfwGetTime();
  bool playing_state = true;

  std::vector<std::string> animation_names(animations.size());
  for (size_t i = 0; i < animations.size(); ++i)
    animation_names[i] = animations[i].name;

  std::vector<std::string> shader_names;
  static int selected_shader = 0;
  static bool first = true;
  int i = 0;
  for (const auto &shader : shaders) {
    shader_names.push_back(shader.first);
    if (first && shader.first == "textured") {
      selected_shader = i;
      first = false;
    }
    ++i;
  }

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    std::string shader_to_use;

    ImGui::Begin("Utilities");
    ImGui::Checkbox("Show ImGui Demo Window?", &show_imgui_demo);
    if (show_imgui_demo) ImGui::ShowDemoWindow(&show_imgui_demo);
    ImGui::End();

    ImGui::Begin("Shader mode");
    ImGuiCombo("Choose shader", &selected_shader, shader_names);
    shader_to_use = shader_names[selected_shader];
    ImGui::End();

    asset_images_window(textures);
    model_info_window(model);
    animation_window(animations);
    skinning_data_window(weights, joints);

    morph_window(mesh_skeleton_graph, nb_morph_targets);

    bool need_to_update_pose = false;
    // let's create the sequencer
    static int selectedEntry = -1;
    static int firstFrame = 0;
    static bool expanded = true;
    static int currentFrame;
    static double currentPlayTime = 0;
    double current_time = glfwGetTime();
    if (playing_state) {
      currentPlayTime += current_time - last_frame_time;
    }
    last_frame_time = current_time;
    currentFrame = int(60 * currentPlayTime);

    ImGui::Begin("Sequencer");
    if (ImGui::Button(playing_state ? "Pause" : "Play")) {
      playing_state = !playing_state;
    }

    ImGui::PushItemWidth(130);
    ImGui::InputInt("Frame Min", &mySequence.mFrameMin);
    ImGui::SameLine();
    if (ImGui::InputInt("Frame ", &currentFrame)) {
      currentPlayTime = double(currentFrame) / 60.0;
      need_to_update_pose = true;
    }
    ImGui::SameLine();
    ImGui::InputInt("Frame Max", &mySequence.mFrameMax);
    ImGui::PopItemWidth();
#if 0
        Sequencer(
                  &mySequence, &currentFrame, &expanded, &selectedEntry,
                  &firstFrame, ImSequencer::SEQUENCER_EDIT_STARTEND |
                  ImSequencer::SEQUENCER_ADD |
                  ImSequencer::SEQUENCER_DEL |
                  ImSequencer::SEQUENCER_COPYPASTE |
                  ImSequencer::SEQUENCER_CHANGE_FRAME);
#else
    const auto saved_frame = currentFrame;
    Sequencer(&mySequence, &currentFrame, &expanded, &selectedEntry,
              &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME);
    if (saved_frame != currentFrame) {
      currentPlayTime = double(currentFrame) / 60.0;
      need_to_update_pose = true;
    }
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

    // loop the sequencer now: TODO replace that true with a "is looping"
    // boolean
    if (true && currentFrame > mySequence.mFrameMax) {
      currentFrame = mySequence.mFrameMin;
      currentPlayTime = double(currentFrame) / 60.0;
    }

    for (auto &anim : animations) {
      anim.set_time(float(currentPlayTime));  // TODO handle timeline position
                                              // of animaiton sequence
      anim.playing = playing_state;
      if (need_to_update_pose || playing_state) anim.apply_pose();
    }

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

      last_frame_time = glfwGetTime();

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

      for (size_t i = 0; i < draw_call_descriptor.size(); ++i) {
        const auto &draw_call_to_perform = draw_call_descriptor[i];

        // If mesh has morph targets
        if (mesh_skeleton_graph.pose.blend_weights.size() > 0) {
          assert(display_position[i].size() == display_normal[i].size());
          // Blend between morph targets on the CPU:
          for (size_t v = 0; v < display_position[i].size(); ++v) {
            // Get the base vector
            display_position[i][v] = vertex_coord[i][v];
            display_normal[i][v] = normals[i][v];
            // Accumulate the delta, v = v0 + w0 * m0 + w1 * m1 + w2 * m2 ...
            for (size_t w = 0;
                 w < mesh_skeleton_graph.pose.blend_weights.size(); ++w) {
              const float weight = mesh_skeleton_graph.pose.blend_weights[w];
              display_position[i][v] +=
                  weight * morph_targets[i][w].position[v];
              display_normal[i][v] += weight * morph_targets[i][w].normal[v];
            }

            // upload to GPU
            glBindBuffer(GL_ARRAY_BUFFER, VBOs[i][0]);
            glBufferData(GL_ARRAY_BUFFER,
                         display_position[i].size() * sizeof(float),
                         display_position[i].data(), GL_STREAM_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, VBOs[i][1]);
            glBufferData(GL_ARRAY_BUFFER,
                         display_normal[i].size() * sizeof(float),
                         display_normal[i].data(), GL_STREAM_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
          }
        }

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
