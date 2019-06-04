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

inline std::string GetFilePathExtension(const std::string& FileName) {
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

void parse_command_line(int argc, char** argv, bool debug_output,
                        std::string& input_filename) {
  cxxopts::Options options("gltf-insignt", "glTF data insight tool");

  options.add_options()("d,debug", "Enable debugging",
                        cxxopts::value<bool>(debug_output))(
      "i,input", "Input glTF filename", cxxopts::value<std::string>())(
      "h,help", "Show help");

  options.parse_positional({"input", "output"});

  if (argc < 2) {
    std::cout << options.help({"", "group"}) << std::endl;
    exit(EXIT_FAILURE);
  }

  const auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help({"", "group"}) << std::endl;
    exit(EXIT_FAILURE);
  }

  if (!result.count("input")) {
    std::cerr << "Input file not specified." << std::endl;
    std::cout << options.help({"", "group"}) << std::endl;
    exit(EXIT_FAILURE);
  }

  input_filename = result["input"].as<std::string>();
}

void load_glTF_asset(tinygltf::TinyGLTF& gltf_ctx,
                     const std::string& input_filename,
                     tinygltf::Model& model) {
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
    ret =
        gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, input_filename.c_str());
  }

  if (!ret) {
    std::cerr << "Problem while loading gltf:\n"
              << "error: " << err << "\nwarning: " << warn << '\n';
  }
}

void load_all_textures(
    tinygltf::Model model,
    const std::vector<tinygltf::Image>::size_type nb_textures,
    std::vector<GLuint>& textures) {
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
}

int main(int argc, char** argv) {
  bool debug_output = false;
  std::string input_filename;
  tinygltf::Model model;
  tinygltf::TinyGLTF gltf_ctx;
  GLFWwindow* window;

  parse_command_line(argc, argv, debug_output, input_filename);
  initialize_glfw_opengl_window(window);
  load_glTF_asset(gltf_ctx, input_filename, model);

  const auto nb_textures = model.images.size();
  std::vector<GLuint> textures(nb_textures);
  load_all_textures(model, nb_textures, textures);
  initialize_imgui(window);
  auto io = ImGui::GetIO();

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // sequence with default values
  gltf_insight::AnimSequence mySequence;
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
  const auto& mesh_node = model.nodes[mesh_node_index];
  const auto& mesh = model.meshes[mesh_node.mesh];
  if (mesh_node.skin < 0) {
    std::cerr << "The loaded gltf file doesn't have any skin in the mesh\n";
    return EXIT_FAILURE;
  }
  const auto& skin = model.skins[mesh_node.skin];
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

  const auto& primitives = mesh.primitives;
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

  const auto nb_animations = model.animations.size();
  std::vector<animation> animations(nb_animations);

  load_animations(model, animations);

  for (int i = 0; i < nb_animations; ++i) {
    mySequence.myItems.push_back(gltf_insight::AnimSequence::AnimSequenceItem{
        0, 60 * int(animations[i].min_time), 60 * int(animations[i].max_time),
        false});
  }

  std::vector<glm::mat4> inverse_bind_matrices;
  load_inverse_bind_matrix_array(model, skin, nb_joints, inverse_bind_matrices);

  // Three : Load the skeleton graph. We need this to calculate the bones
  // world transform
  gltf_node mesh_skeleton_graph(gltf_node::node_type::mesh);
  populate_gltf_skeleton_subgraph(model, mesh_skeleton_graph, skeleton);

  // Four : Get an array that is in the same order as the bones in the
  // glTF to represent the whole skeletons. This is important for the
  // joint matrix calculation
  std::vector<gltf_node*> flat_bone_list;
  create_flat_bone_list(skin, nb_joints, mesh_skeleton_graph, flat_bone_list);
  // assert(flat_bone_list.size() == nb_joints);

  // Five : For each animation loaded that is supposed to move the skeleton,
  // associate the animation channel targets with their gltf "node" here:
  for (auto& animation : animations) {
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
  for (auto& target : morph_targets) {
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
  for (auto& VBO : VBOs) {
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
  glm::mat4& model_matrix = mesh_skeleton_graph.local_xform;
  glm::mat4 view_matrix{1.f}, projection_matrix{1.f};
  int display_w, display_h;
  glm::vec3 camera_position{0, 0, 3.F};

  // We need to pass this to glfw to have mouse control
  struct application_parameters {
    glm::vec3& camera_position;
    bool button_states[3]{false};
    double last_mouse_x{0}, last_mouse_y{0};
    double rot_pitch{0}, rot_yaw{0};
    double rotation_scale = 0.2;
    application_parameters(glm::vec3& cam_pos) : camera_position(cam_pos) {}
  } my_user_pointer{camera_position};

  glfwSetWindowUserPointer(window, &my_user_pointer);

  glfwSetMouseButtonCallback(
      window, [](GLFWwindow* window, int button, int action, int mods) {
        auto* param = reinterpret_cast<application_parameters*>(
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

  glfwSetCursorPosCallback(window, [](GLFWwindow* window, double mouse_x,
                                      double mouse_y) {
    auto* param = reinterpret_cast<application_parameters*>(
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
  {
    static bool first = true;
    int i = 0;
    for (const auto& shader : shaders) {
      shader_names.push_back(shader.first);
      if (first && shader.first == "textured") {
        selected_shader = i;
        first = false;
      }
      ++i;
    }
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
    static bool looping = true;
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
    ImGui::SameLine(), ImGui::Checkbox("looping", &looping);

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

    const auto saved_frame = currentFrame;
    Sequencer(&mySequence, &currentFrame, &expanded, &selectedEntry,
              &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME);
    if (saved_frame != currentFrame) {
      currentPlayTime = double(currentFrame) / 60.0;
      need_to_update_pose = true;
    }

    // add a UI to edit that particular item
    if (selectedEntry != -1) {
      const gltf_insight::AnimSequence::AnimSequenceItem& item =
          mySequence.myItems[selectedEntry];
      ImGui::Text("I am a %s, please edit me",
                  gltf_insight::SequencerItemTypeNames[item.mType]);
      // switch (type) ....
    }

    ImGui::End();

    // loop the sequencer now: TODO replace that true with a "is looping"
    // boolean
    if (looping && currentFrame > mySequence.mFrameMax) {
      currentFrame = mySequence.mFrameMin;
      currentPlayTime = double(currentFrame) / 60.0;
    }

    for (auto& anim : animations) {
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
        const auto& draw_call_to_perform = draw_call_descriptor[i];

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

      bone_display_window();

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
