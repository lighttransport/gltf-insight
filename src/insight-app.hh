#pragma once

// This includes opengl for us, along side debuging callbacks
#include "gl_util.hh"

// imgui
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

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
#include "gui_util.hh"
#include "shader.hpp"
#include "tiny_gltf.h"
#include "tiny_gltf_util.h"

/// Main application class
class app {
 public:
  void unload() {
    asset_loaded = false;
    // TODO cleanup what you can !

    // loaded opengl objects
    if (!textures.empty()) glDeleteTextures(textures.size(), textures.data());
    textures.clear();

    if (!VAOs.empty()) {
      glDeleteVertexArrays(VAOs.size(), VAOs.data());
    }
    VAOs.clear();

    for (auto& VBO : VBOs) {
      glDeleteBuffers(VBO.size(), VBO.data());
    }
    VBOs.clear();

    shader_list.clear();
    shader_names.clear();
    shader_to_use.clear();
    found_textured_shader = false;

    // loaded CPU side objects
    joint_matrices.clear();
    joint_inverse_bind_matrix_map.clear();
    animations.clear();
    animation_names.clear();
    inverse_bind_matrices.clear();
    morph_targets.clear();
    indices.clear();
    vertex_coord.clear();
    texture_coord.clear();
    normals.clear();
    weights.clear();
    display_position.clear();
    display_normal.clear();
    joints.clear();
    draw_call_descriptors.clear();
    flat_joint_list.clear();

    empty_gltf_skeleton_subgraph(mesh_skeleton_graph);
    mesh_skeleton_graph.local_xform = glm::mat4{1.f};

    // other number counters and things
    selectedEntry = -1;
    currentFrame = 0;
    playing_state = true;
    camera_position = glm::vec3{0.f, 0.f, 7.f};
    selected_shader = 0;
    sequence.myItems.clear();

    // file input information
    input_filename.clear();

    // library resources
    model = tinygltf::Model();
  }

  void load() {
    load_glTF_asset(gltf_ctx, input_filename, model);

    const auto nb_textures = model.images.size();
    textures.resize(nb_textures);
    load_all_textures(model, nb_textures, textures);

    // We are bypassing the actual glTF scene here, we are interested in a
    // file that only represent a character with animations. Find the scene
    // node that has a mesh attached to it:
    const auto mesh_node_index = find_main_mesh_node(model);
    if (mesh_node_index < 0) {
      std::cerr << "The loaded gltf file doesn't have any findable mesh\n";
      exit(EXIT_FAILURE);
    }

    // TODO (ybalrid) : refactor loading code outside of int main()
    // Get access to the data
    const auto& mesh_node = model.nodes[mesh_node_index];
    const auto& mesh = model.meshes[mesh_node.mesh];
    if (mesh_node.skin < 0) {
      std::cerr << "The loaded gltf file doesn't have any skin in the mesh\n";
      exit(EXIT_FAILURE);
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
    joint_matrices.resize(nb_joints);

    // One : We need to know, for each joint, what is it's inverse bind
    // matrix
    genrate_joint_inverse_bind_matrix_map(skin, nb_joints,
                                          joint_inverse_bind_matrix_map);

    // Two : We load the actual animation data. We also initialize the
    // animation sequencer
    const auto nb_animations = model.animations.size();
    animations.resize(nb_animations);
    load_animations(model, animations);
    fill_sequencer(sequence, animations);

    // Three : Load the skeleton graph. We need this to calculate the bones
    // world transform
    load_inverse_bind_matrix_array(model, skin, nb_joints,
                                   inverse_bind_matrices);
    populate_gltf_skeleton_subgraph(model, mesh_skeleton_graph, skeleton);

    // Four : Get an array that is in the same order as the bones in the
    // glTF to represent the whole skeletons. This is important for the
    // joint matrix calculation
    create_flat_bone_list(skin, nb_joints, mesh_skeleton_graph,
                          flat_joint_list);
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

    // Six : Load morph targets:
    // TODO we can probably only have one loop going through the submeshes
    morph_targets.resize(nb_submeshes);
    for (int i = 0; i < nb_submeshes; ++i) {
      morph_targets[i].resize(mesh.primitives[i].targets.size());
      load_morph_targets(model, mesh.primitives[i], morph_targets[i]);
    }

    // Set the number of weights to animate
    nb_morph_targets = 0;
    for (auto& target : morph_targets) {
      nb_morph_targets = std::max<int>(target.size(), nb_morph_targets);
    }

    // Create an array of blend weights in the main node, and fill it with
    // zeroes
    mesh_skeleton_graph.pose.blend_weights.resize(nb_morph_targets);
    std::generate(mesh_skeleton_graph.pose.blend_weights.begin(),
                  mesh_skeleton_graph.pose.blend_weights.end(),
                  [] { return 0.f; });

    // TODO technically there's a "default" pose of the glTF asset in therm of
    // the value of theses weights. They are set to zero by default, but
    // still.

    // For each submesh, we need to know the draw operation, the VAO to
    // bind, the textures to use and the element count. This array store all
    // of these
    draw_call_descriptors.resize(nb_submeshes);

    // Create opengl objects
    VAOs.resize(nb_submeshes);

    VBOs.resize(nb_submeshes);
    glGenVertexArrays(GLsizei(nb_submeshes), VAOs.data());
    for (auto& VBO : VBOs) {
      // We have 5 vertex attributes per vertex + 1 element array.
      glGenBuffers(6, VBO.data());
    }

    // CPU sise storage for all vertex attributes
    indices.resize(nb_submeshes);
    vertex_coord.resize(nb_submeshes);
    texture_coord.resize(nb_submeshes);
    normals.resize(nb_submeshes);
    weights.resize(nb_submeshes);
    joints.resize(nb_submeshes);

    // For each submesh of the mesh, load the data
    load_geometry(model, textures, primitives, draw_call_descriptors, VAOs,
                  VBOs, indices, vertex_coord, texture_coord, normals, weights,
                  joints);

    // create a copy of the data for morphing. We will repeatidly upload
    // theses arrays to the GPU, while preserving the value of the original
    // vertex coordinates. Morph targets are only deltas from the default
    // position.
    display_position = vertex_coord;
    display_normal = normals;

    // Not doing this seems to break imgui in opengl2 mode...
    // TODO maybe just move to the OpenGL 3.x code. This could help debuging
    // as tools like nvidia nsights are less usefull on old style opengl
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    load_shaders(nb_joints, shader_list);

    // Main loop

    animation_names.resize(nb_animations);
    for (size_t i = 0; i < animations.size(); ++i)
      animation_names[i] = animations[i].name;

    {
      found_textured_shader = true;
      int i = 0;
      for (const auto& shader : shader_list) {
        shader_names.push_back(shader.first);
        if (found_textured_shader && shader.first == "textured") {
          selected_shader = i;
          found_textured_shader = false;
        }
        ++i;
      }
    }

    asset_loaded = true;
  }

  app(int argc, char** argv) {
    parse_command_line(argc, argv, debug_output, input_filename);

    initialize_glfw_opengl_window(window);
    glfwSetWindowUserPointer(window, &gui_parameters);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    initialize_imgui(window);
    (void)ImGui::GetIO();

    if (!input_filename.empty()) {
      try {
        load();
      } catch (const std::exception& e) {
        unload();
      }
    }
  }

  ~app() {
    unload();
    deinitialize_gui_and_window(window);
  }

  void main_loop() {
    while (!glfwWindowShouldClose(window)) {
      {  // GUI
        gui_new_frame();

        ImGui::Begin("Icon test");
        ImGui::Text("%s %s %s", ICON_II_ANDROID_FOLDER_OPEN,
                    ICON_II_ANDROID_DOCUMENT, ICON_II_ANDROID_ARROW_FORWARD);
        ImGui::End();

        // TODO put main menu in it's own function wen done
        ImGui::BeginMainMenuBar();
        if (ImGui::BeginMenu("File")) {
          // TODO use ImGuiFileDialog
          if (ImGui::MenuItem("Open glTF...")) {
            open_file_dialog = true;
          }
          if (ImGui::MenuItem("Save as...")) {
            save_file_dialog = true;
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Quit")) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
          }
          ImGui::EndMenu();
        }

        if (open_file_dialog) {
          if (ImGuiFileDialog::Instance()->FileDialog("Open glTF...",
                                                      ".gltf\0.glb\0\0")) {
            if (ImGuiFileDialog::Instance()->IsOk) {
              unload();
              input_filename = ImGuiFileDialog::Instance()->GetFilepathName();
              try {
                load();
              } catch (const std::exception& e) {
                unload();
              }
            } else {
            }
            open_file_dialog = false;
          }
        }

        if (save_file_dialog) {
          if (!asset_loaded)
            save_file_dialog = false;
          else if (ImGuiFileDialog::Instance()->FileDialog("Save as...", "")) {
            if (ImGuiFileDialog::Instance()->IsOk) {
              auto save_as_filename =
                  ImGuiFileDialog::Instance()->GetFilepathName();
              try {
                // Check file path extension for gltf or glb. Fix it if
                // necessary.
                // Serialize to file
              } catch (const std::exception& e) {
                // display error here
              }
            } else {
            }
            save_file_dialog = false;
          }
        }

#if defined(DEBUG) || defined(_DEBUG)
        if (ImGui::BeginMenu("DEBUG")) {
          if (ImGui::MenuItem("call unload()")) unload();
          ImGui::EndMenu();
        }
#endif
        ImGui::EndMainMenuBar();

        if (asset_loaded) {
          // Draw all windows
          utilities_window(show_imgui_demo);
          shader_selector_window(shader_names, selected_shader, shader_to_use);
          asset_images_window(textures);
          model_info_window(model);
          animation_window(animations);
          skinning_data_window(weights, joints);
          morph_target_window(mesh_skeleton_graph, nb_morph_targets);
          camera_parameters_window(fovy, z_far);

          // Animation player advances time and apply animation interpolation.
          // It also display the sequencer timeline and controls on screen
          run_animation_player(sequence, looping, selectedEntry, firstFrame,
                               expanded, currentFrame, currentPlayTime,
                               last_frame_time, playing_state, animations);
        }
      }

      {  // 3D rendering
        gl_new_frame(window, viewport_background_color, display_w, display_h);
        if (display_h && display_w)  // not zero please
          projection_matrix = glm::perspective(
              glm::radians(fovy), float(display_w) / float(display_h), z_near,
              z_far);

        run_3D_gizmo(view_matrix, projection_matrix, model_matrix,
                     camera_position, gui_parameters);

        if (asset_loaded) {
          // Calculate all the needed matrices to render the frame, this
          // includes the "model view projection" that transform the geometry to
          // the screen space, the normal matrix, and the joint matrix array
          // that is used to deform the skin with the bones
          precompute_hardware_skinning_data(mesh_skeleton_graph, model_matrix,
                                            joint_matrices, flat_joint_list,
                                            inverse_bind_matrices);

          glm::mat4 mvp = projection_matrix * view_matrix * model_matrix;
          glm::mat3 normal = glm::transpose(glm::inverse(model_matrix));
          update_uniforms(shader_list, shader_to_use, mvp, normal,
                          joint_matrices);

          // Draw all of the submeshes of the object
          for (size_t submesh = 0; submesh < draw_call_descriptors.size();
               ++submesh) {
            const auto& draw_call = draw_call_descriptors[submesh];
            perform_software_morphing(mesh_skeleton_graph, submesh,
                                      morph_targets, vertex_coord, normals,
                                      display_position, display_normal, VBOs);
            perform_draw_call(draw_call);
          }

          // Then draw 2D bones and joints on top of that
          draw_bone_overlay(mesh_skeleton_graph, view_matrix, projection_matrix,
                            shader_list);
        }
      }
      // Render all ImGui, then swap buffers
      gl_gui_end_frame(window);
    }
  }

 private:
  bool open_file_dialog = false;
  bool save_file_dialog = false;
  bool asset_loaded = false;
  bool found_textured_shader = false;
  gltf_node mesh_skeleton_graph{gltf_node::node_type::mesh};
  ImVec4 viewport_background_color = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  tinygltf::Model model;
  tinygltf::TinyGLTF gltf_ctx;

  // display parameters
  std::map<std::string, shader> shader_list;
  std::vector<std::string> shader_names;
  int selected_shader = 0;
  std::string shader_to_use;
  glm::mat4 view_matrix{1.f}, projection_matrix{1.f};
  glm::mat4& model_matrix = mesh_skeleton_graph.local_xform;
  int display_w, display_h;
  glm::vec3 camera_position{0, 0, 7.f};
  float fovy = 45.f;
  float z_near = 1.f;
  float z_far = 100.f;

  // OpenGL objects
  std::vector<GLuint> VAOs;
  std::vector<std::array<GLuint, 6>> VBOs;

  // user interface state
  bool debug_output = false;
  bool show_imgui_demo = false;
  std::string input_filename;
  GLFWwindow* window{nullptr};
  application_parameters gui_parameters{camera_position};

  // Sequecer state
  gltf_insight::AnimSequence sequence;
  bool looping = true;
  int selectedEntry = -1;
  int firstFrame = 0;
  bool expanded = true;
  int currentFrame;
  double currentPlayTime = 0;
  double last_frame_time = 0;
  bool playing_state = true;

  // loaded data
  std::vector<GLuint> textures;
  std::vector<glm::mat4> joint_matrices;
  std::map<int, int> joint_inverse_bind_matrix_map;
  std::vector<gltf_node*> flat_joint_list;
  std::vector<animation> animations;
  std::vector<std::string> animation_names;
  std::vector<glm::mat4> inverse_bind_matrices;
  std::vector<std::vector<morph_target>> morph_targets;
  std::vector<draw_call_submesh> draw_call_descriptors;
  std::vector<std::vector<unsigned>> indices;
  std::vector<std::vector<float>> vertex_coord, texture_coord, normals, weights,
      display_position, display_normal;
  std::vector<std::vector<unsigned short>> joints;
  int nb_morph_targets;

  // hidden methods

  inline std::string GetFilePathExtension(const std::string& FileName) {
    if (FileName.find_last_of(".") != std::string::npos)
      return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
  }

  inline void parse_command_line(int argc, char** argv, bool debug_output,
                                 std::string& input_filename) {
    cxxopts::Options options("gltf-insignt", "glTF data insight tool");

    options.add_options()("d,debug", "Enable debugging",
                          cxxopts::value<bool>(debug_output))(
        "i,input", "Input glTF filename", cxxopts::value<std::string>())(
        "h,help", "Show help");

    options.parse_positional({"input", "output"});

    // if (argc < 2) {
    //  std::cout << options.help({"", "group"}) << std::endl;
    //  exit(EXIT_FAILURE);
    //}

    const auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help({"", "group"}) << std::endl;
      exit(EXIT_FAILURE);
    }

    if (result.count("input")) {
      input_filename = result["input"].as<std::string>();
    } else {
      input_filename.clear();
    }
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
      ret = gltf_ctx.LoadASCIIFromFile(&model, &err, &warn,
                                       input_filename.c_str());
    }

    if (!ret) {
      std::cerr << "Problem while loading gltf:\n"
                << "error: " << err << "\nwarning: " << warn << '\n';

      throw std::runtime_error("error: " + err);
    }
  }

  void load_all_textures(tinygltf::Model model, size_t nb_textures,
                         std::vector<GLuint>& textures) {
    glGenTextures(GLsizei(nb_textures), textures.data());

    for (size_t i = 0; i < nb_textures; ++i) {
      glBindTexture(GL_TEXTURE_2D, textures[i]);
      glTexImage2D(GL_TEXTURE_2D, 0,
                   model.images[i].component == 4 ? GL_RGBA : GL_RGB,
                   model.images[i].width, model.images[i].height, 0,
                   model.images[i].component == 4 ? GL_RGBA : GL_RGB,
                   GL_UNSIGNED_BYTE, model.images[i].image.data());
      glGenerateMipmap(GL_TEXTURE_2D);
      // TODO set texture sampling and filtering parameters
    }
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  void genrate_joint_inverse_bind_matrix_map(
      const tinygltf::Skin& skin, const std::vector<int>::size_type nb_joints,
      std::map<int, int> joint_inverse_bind_matrix_map) {
    for (int i = 0; i < nb_joints; ++i)
      joint_inverse_bind_matrix_map[skin.joints[i]] = i;
  }

  void cpu_compute_morphed_display_mesh(
      gltf_node mesh_skeleton_graph, size_t submesh_id,
      const std::vector<std::vector<morph_target>>& morph_targets,
      const std::vector<std::vector<float>>& vertex_coord,
      const std::vector<std::vector<float>>& normals,
      std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal, size_t vertex) {
    // Get the base vector
    display_position[submesh_id][vertex] = vertex_coord[submesh_id][vertex];
    display_normal[submesh_id][vertex] = normals[submesh_id][vertex];
    // Accumulate the delta, v = v0 + w0 * m0 + w1 * m1 + w2 * m2 ...
    for (size_t w = 0; w < mesh_skeleton_graph.pose.blend_weights.size(); ++w) {
      const float weight = mesh_skeleton_graph.pose.blend_weights[w];
      display_position[submesh_id][vertex] +=
          weight * morph_targets[submesh_id][w].position[vertex];
      display_normal[submesh_id][vertex] +=
          weight * morph_targets[submesh_id][w].normal[vertex];
    }
  }

  void gpu_update_morphed_submesh(
      size_t submesh_id, std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal,
      std::vector<std::array<GLuint, 6>>& VBOs) {
    // upload to GPU
    glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh_id][0]);
    glBufferData(GL_ARRAY_BUFFER,
                 display_position[submesh_id].size() * sizeof(float),
                 display_position[submesh_id].data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh_id][1]);
    glBufferData(GL_ARRAY_BUFFER,
                 display_normal[submesh_id].size() * sizeof(float),
                 display_normal[submesh_id].data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  void perform_software_morphing(
      gltf_node mesh_skeleton_graph, size_t submesh_id,
      const std::vector<std::vector<morph_target>>& morph_targets,
      const std::vector<std::vector<float>>& vertex_coord,
      const std::vector<std::vector<float>>& normals,
      std::vector<std::vector<float>>& display_position,
      std::vector<std::vector<float>>& display_normal,
      std::vector<std::array<GLuint, 6>>& VBOs) {
    // If mesh has morph targets
    if (mesh_skeleton_graph.pose.blend_weights.size() > 0) {
      assert(display_position[submesh_id].size() ==
             display_normal[submesh_id].size());
      // Blend between morph targets on the CPU:
      for (size_t vertex = 0; vertex < display_position[submesh_id].size();
           ++vertex) {
        cpu_compute_morphed_display_mesh(
            mesh_skeleton_graph, submesh_id, morph_targets, vertex_coord,
            normals, display_position, display_normal, vertex);
      }
      gpu_update_morphed_submesh(submesh_id, display_position, display_normal,
                                 VBOs);
    }
  }

  void draw_bone_overlay(gltf_node& mesh_skeleton_graph,
                         const glm::mat4& view_matrix,
                         const glm::mat4& projection_matrix,
                         std::map<std::string, shader>& shaders) {
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);

    bone_display_window();
    shaders["debug_color"].use();
    draw_bones(mesh_skeleton_graph, shaders["debug_color"].get_program(),
               view_matrix, projection_matrix);
  }

  void precompute_hardware_skinning_data(
      gltf_node& mesh_skeleton_graph, glm::mat4& model_matrix,
      std::vector<glm::mat4>& joint_matrices,
      std::vector<gltf_node*>& flat_joint_list,
      std::vector<glm::mat4>& inverse_bind_matrices) {
    // This goes through the skeleton hierarchy, and compute the global
    // transform of each joint
    update_mesh_skeleton_graph_transforms(mesh_skeleton_graph);

    // This compute the individual joint matrices that are uploaded to the
    // GPU. Detailed explanations about skinning can be found in this tutorial
    // https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_020_Skins.md#vertex-skinning-implementation
    // Please note that the code in the tutorial compute the inverse model
    // matrix for each joint, but this matrix doesn't vary here, so
    // we can put it out of the loop. I borrowed this slight optimization from
    // Sascha Willems's "vulkan-glTF-PBR" code...
    // https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.hpp
    glm::mat4 inverse_model = glm::inverse(model_matrix);
    for (size_t i = 0; i < joint_matrices.size(); ++i) {
      joint_matrices[i] = inverse_model * flat_joint_list[i]->world_xform *
                          inverse_bind_matrices[i];
    }
  }

  void run_animation_player(gltf_insight::AnimSequence& sequence, bool& looping,
                            int& selectedEntry, int& firstFrame, bool& expanded,
                            int& currentFrame, double& currentPlayTime,
                            double& last_frame_time, bool& playing_state,
                            std::vector<animation>& animations) {
    // let's create the sequencer
    double current_time = glfwGetTime();
    bool need_to_update_pose = false;
    if (playing_state) {
      currentPlayTime += current_time - last_frame_time;
    }

    currentFrame = int(ANIMATION_FPS * currentPlayTime);
    sequencer_window(sequence, playing_state, need_to_update_pose, looping,
                     selectedEntry, firstFrame, expanded, currentFrame,
                     currentPlayTime);

    // loop the sequencer now: TODO replace that true with a "is looping"
    // boolean
    if (looping && currentFrame > sequence.mFrameMax) {
      currentFrame = sequence.mFrameMin;
      currentPlayTime = double(currentFrame) / ANIMATION_FPS;
    }

    for (auto& anim : animations) {
      anim.set_time(float(currentPlayTime));  // TODO handle timeline position
      // of animaiton sequence
      anim.playing = playing_state;
      if (need_to_update_pose || playing_state) anim.apply_pose();
    }

    last_frame_time = current_time;
  }

  void run_3D_gizmo(glm::mat4& view_matrix, const glm::mat4& projection_matrix,
                    glm::mat4& model_matrix, glm::vec3& camera_position,
                    application_parameters& my_user_pointer) {
    float vecTranslation[3], vecRotation[3], vecScale[3];
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

    ImGuizmo::DecomposeMatrixToComponents(
        glm::value_ptr(model_matrix), vecTranslation, vecRotation, vecScale);

    transform_window(view_matrix, camera_position, my_user_pointer,
                     vecTranslation, vecRotation, vecScale,
                     mCurrentGizmoOperation);

    ImGuizmo::RecomposeMatrixFromComponents(
        vecTranslation, vecRotation, vecScale, glm::value_ptr(model_matrix));

    auto io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(glm::value_ptr(view_matrix),
                         glm::value_ptr(projection_matrix),
                         mCurrentGizmoOperation, mCurrentGizmoMode,
                         glm::value_ptr(model_matrix), NULL, NULL);
  }

  void fill_sequencer(gltf_insight::AnimSequence& sequence,
                      const std::vector<animation>& animations) {
    for (auto& animation : animations) {
      // TODO change animation sequencer to use seconds instead of frames
      sequence.myItems.push_back(gltf_insight::AnimSequence::AnimSequenceItem{
          0, int(ANIMATION_FPS * animation.min_time),
          int(ANIMATION_FPS * animation.max_time), false, animation.name});
    }

    auto max_time = sequence.myItems[0].mFrameEnd;
    for (auto& item : sequence.myItems) {
      max_time = std::max(max_time, item.mFrameEnd);
    }
    sequence.mFrameMax = max_time;
  }
};
