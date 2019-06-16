#include "insight-app.hh"

using namespace gltf_insight;

void app::unload() {
  asset_loaded = false;

  // loaded opengl objects
  glDeleteTextures(GLsizei(textures.size()), textures.data());

  textures.clear();

  shader_names.clear();
  shader_to_use.clear();
  found_textured_shader = false;

  // loaded CPU side objects
  animations.clear();
  animation_names.clear();

  // empty_gltf_graph(mesh_skeleton_graph);
  // mesh_skeleton_graph.local_xform = glm::mat4{1.f};

  // other number counters and things
  selectedEntry = -1;
  currentFrame = 0;
  playing_state = true;
  camera_position = glm::vec3{0.f, 0.f, 7.f};
  selected_shader = 0;
  sequence.myItems.clear();

  // file input information
  input_filename.clear();
  loaded_meshes.clear();

  // library resources
  model = tinygltf::Model();
  empty_gltf_graph(gltf_scene_tree);
}

void app::load() {
  load_glTF_asset(gltf_ctx, input_filename, model);

  const auto nb_textures = model.images.size();
  textures.resize(nb_textures);
  load_all_textures(model, nb_textures, textures);

  const auto scene = find_main_scene(model);

  // TODO it looks like by the spec that, actually, a gltf scene could have...
  // multiple root nodes?
  const auto root_index = model.scenes[scene].nodes[0];
  gltf_scene_tree.gltf_node_index = root_index;

  populate_gltf_graph(model, gltf_scene_tree, root_index);
  set_mesh_attachement(model, gltf_scene_tree);
  auto meshes_indices = get_list_of_mesh(gltf_scene_tree);

  loaded_meshes.resize(meshes_indices.size());
  for (size_t i = 0; i < meshes_indices.size(); ++i) {
    loaded_meshes[i].instance = meshes_indices[i];
    auto& current_mesh = loaded_meshes[i];
    const auto skin_index = model.nodes[current_mesh.instance.node].skin;
    if (skin_index >= 0) {
      current_mesh.skinned = true;
      const auto& gltf_skin = model.skins[skin_index];
      current_mesh.nb_joints = int(gltf_skin.joints.size());
      create_flat_bone_list(gltf_skin, current_mesh.nb_joints, gltf_scene_tree,
                            current_mesh.flat_joint_list);
      current_mesh.joint_matrices.resize(current_mesh.nb_joints);
      load_inverse_bind_matrix_array(model, gltf_skin, current_mesh.nb_joints,
                                     current_mesh.inverse_bind_matrices);
      genrate_joint_inverse_bind_matrix_map(
          gltf_skin, current_mesh.nb_joints,
          current_mesh.joint_inverse_bind_matrix_map);
    }

    const auto& gltf_mesh = model.meshes[current_mesh.instance.mesh];

    if (!gltf_mesh.name.empty())
      current_mesh.name = gltf_mesh.name;
    else
      current_mesh.name = std::string("mesh_") + std::to_string(i);

    const auto& gltf_mesh_primitives = gltf_mesh.primitives;
    const auto nb_submeshes = gltf_mesh_primitives.size();

    current_mesh.draw_call_descriptors.resize(nb_submeshes);
    current_mesh.indices.resize(nb_submeshes);
    current_mesh.vertex_coord.resize(nb_submeshes);
    current_mesh.texture_coord.resize(nb_submeshes);
    current_mesh.normals.resize(nb_submeshes);
    current_mesh.weights.resize(nb_submeshes);
    current_mesh.joints.resize(nb_submeshes);
    current_mesh.VAOs.resize(nb_submeshes);
    current_mesh.VBOs.resize(nb_submeshes);

    // Create OpenGL objects for submehes
    glGenVertexArrays(GLsizei(nb_submeshes), current_mesh.VAOs.data());
    for (auto& VBO : current_mesh.VBOs) {
      // We have 5 vertex attributes per vertex + 1 element array.
      glGenBuffers(6, VBO.data());
    }

    // For each submesh of the mesh, load the data
    load_geometry(model, textures, gltf_mesh_primitives,
                  current_mesh.draw_call_descriptors, current_mesh.VAOs,
                  current_mesh.VBOs, current_mesh.indices,
                  current_mesh.vertex_coord, current_mesh.texture_coord,
                  current_mesh.normals, current_mesh.weights,
                  current_mesh.joints);

    current_mesh.display_position = current_mesh.vertex_coord;
    current_mesh.display_normal = current_mesh.normals;

    // cleanup opengl state
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    current_mesh.shader_list = std::unique_ptr<std::map<std::string, shader>>(
        new std::map<std::string, shader>);

    current_mesh.morph_targets.resize(nb_submeshes);
    for (int s = 0; s < nb_submeshes; ++s) {
      current_mesh.morph_targets[s].resize(
          gltf_mesh.primitives[s].targets.size());

      load_morph_targets(model, gltf_mesh.primitives[s],
                         current_mesh.morph_targets[s]);
    }

    current_mesh.nb_morph_targets = 0;
    for (auto& target : current_mesh.morph_targets) {
      current_mesh.nb_morph_targets =
          std::max<int>(int(target.size()), current_mesh.nb_morph_targets);
    }
    std::vector<std::string> target_names(current_mesh.nb_morph_targets);
    load_morph_target_names(gltf_mesh, target_names);
    gltf_scene_tree.pose.target_names = target_names;

    load_shaders(current_mesh.nb_joints, *current_mesh.shader_list);
  }

  const auto nb_animations = model.animations.size();
  animations.resize(nb_animations);
  load_animations(model, animations);
  fill_sequencer(sequence, animations);

  for (auto& animation : animations) {
    animation.set_gltf_graph_targets(&gltf_scene_tree);

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

  // TODO this is ... mh... per node?
  auto nb_morph_targets = loaded_meshes[0].nb_morph_targets;
  for (int i = 1; i < loaded_meshes.size(); ++i) {
    nb_morph_targets =
        std::max(nb_morph_targets, loaded_meshes[i].nb_morph_targets);
  }

  gltf_scene_tree.pose.blend_weights.resize(nb_morph_targets);
  std::generate(gltf_scene_tree.pose.blend_weights.begin(),
                gltf_scene_tree.pose.blend_weights.end(), [] { return 0.f; });

  animation_names.resize(nb_animations);
  for (size_t i = 0; i < animations.size(); ++i)
    animation_names[i] = animations[i].name;

  asset_loaded = true;
}

mesh::~mesh() {
  for (auto& VBO : VBOs) glDeleteBuffers(6, VBO.data());
  glDeleteVertexArrays(GLsizei(VAOs.size()), VAOs.data());

  displayed = true, skinned = false;
  // shader_list.reset(nullptr);

  nb_joints = 0;
  nb_morph_targets = 0;

  joints.clear();
  vertex_coord.clear();
  texture_coord.clear();
  normals.clear();
  weights.clear();
  display_position.clear();
  display_normal.clear();
  indices.clear();
  flat_joint_list.clear();
  joint_inverse_bind_matrix_map.clear();
  joint_matrices.clear();
  instance.mesh = -1;
  instance.node = -1;
  draw_call_descriptors.clear();
}

mesh& mesh::operator=(mesh&& o) throw() {
  nb_joints = o.nb_joints;
  nb_morph_targets = o.nb_morph_targets;
  instance = o.instance;
  joint_matrices = std::move(o.joint_matrices);
  joint_inverse_bind_matrix_map = std::move(o.joint_inverse_bind_matrix_map);
  flat_joint_list = std::move(o.flat_joint_list);
  inverse_bind_matrices = std::move(o.inverse_bind_matrices);
  morph_targets = std::move(o.morph_targets);
  draw_call_descriptors = std::move(o.draw_call_descriptors);
  indices = std::move(o.indices);
  vertex_coord = std::move(o.vertex_coord);
  texture_coord = std::move(o.texture_coord);
  normals = std::move(o.normals);
  weights = std::move(o.weights);
  display_position = std::move(o.display_position);
  display_normal = std::move(o.display_normal);
  o.joints = std::move(o.joints);

  shader_list = std::move(o.shader_list);
  return *this;
}

mesh::mesh(mesh&& other) throw() { *this = std::move(other); }

mesh::mesh() : instance() {}

app::app(int argc, char** argv) {
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
      std::cerr << e.what() << '\n';
      unload();
    }
  }
}

app::~app() {
  unload();
  deinitialize_gui_and_window(window);
}

void app::main_loop() {
  while (!glfwWindowShouldClose(window)) {
    {
      // GUI
      gui_new_frame();

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

      if (ImGui::BeginMenu("Edit")) {
        ImGui::Text("Transform type:");
        if (ImGui::RadioButton("Translate",
                               mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
          mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::RadioButton("Rotate",
                               mCurrentGizmoOperation == ImGuizmo::ROTATE))
          mCurrentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::RadioButton("Scale",
                               mCurrentGizmoOperation == ImGuizmo::SCALE))
          mCurrentGizmoOperation = ImGuizmo::SCALE;
        ImGui::Separator();
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Show ImGui Demo window", nullptr, &show_imgui_demo);
        ImGui::Separator();
        ImGui::Text("Toggle window display:");
        ImGui::MenuItem("Images", 0, &show_asset_image_window);
        ImGui::MenuItem("Model info", 0, &show_model_info_window);
        ImGui::MenuItem("Animations", 0, &show_animation_window);
        ImGui::MenuItem("Mesh Visibility", 0, &show_mesh_display_window);
        ImGui::MenuItem("Morph Target blend weights", 0,
                        &show_morph_target_window);
        ImGui::MenuItem("Camera parameters", 0, &show_camera_parameter_window);
        ImGui::MenuItem("TransformWindow", 0, &show_transform_window);
        ImGui::MenuItem("Timeline", 0, &show_timeline);
        ImGui::Separator();
        ImGui::MenuItem("Show Gizmo", 0, &show_gizmo);

        ImGui::EndMenu();
      }

      if (open_file_dialog) {
        if (ImGuiFileDialog::Instance()->FileDialog(
                "Open glTF...", ".gltf\0.glb\0.vrm\0.*\0\0")) {
          if (ImGuiFileDialog::Instance()->IsOk) {
            unload();
            input_filename = ImGuiFileDialog::Instance()->GetFilepathName();
            try {
              load();
            } catch (const std::exception& e) {
              std::cerr << e.what() << '\n';
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
              std::cerr << e.what() << '\n';
              // display error here
            }
          } else {
          }
          save_file_dialog = false;
        }
      }

      if (show_imgui_demo) {
        ImGui::ShowDemoWindow(&show_imgui_demo);
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

        selected_shader = 0;
        shader_to_use = "textured";

        // shader_selector_window(shader_names, selected_shader, shader_to_use);
        asset_images_window(textures, &show_asset_image_window);
        model_info_window(model, &show_model_info_window);
        animation_window(animations, &show_animation_window);
        // skinning_data_window(weights, joints);
        mesh_display_window(loaded_meshes, &show_mesh_display_window);
        morph_target_window(gltf_scene_tree,
                            loaded_meshes.front().nb_morph_targets,
                            &show_morph_target_window);
        camera_parameters_window(fovy, z_far, &show_camera_parameter_window);

        // Animation player advances time and apply animation interpolation.
        // It also display the sequencer timeline and controls on screen
        run_animation_timeline(sequence, looping, selectedEntry, firstFrame,
                               expanded, currentFrame, currentPlayTime,
                               last_frame_time, playing_state, animations);
      }
    }

    {
      // 3D rendering
      gl_new_frame(window, viewport_background_color, display_w, display_h);
      if (display_h && display_w)  // not zero please
        projection_matrix = glm::perspective(
            glm::radians(fovy), float(display_w) / float(display_h), z_near,
            z_far);

      run_3D_gizmo(view_matrix, projection_matrix, model_matrix,
                   camera_position, gui_parameters);

      update_mesh_skeleton_graph_transforms(gltf_scene_tree);

      const glm::quat camera_rotation(
          glm::vec3(glm::radians(gui_parameters.rot_pitch),
                    glm::radians(gui_parameters.rot_yaw), 0.f));

      view_matrix =
          glm::lookAt(camera_rotation * camera_position, glm::vec3(0.f),
                      camera_rotation * glm::vec3(0, 1.f, 0));

      if (asset_loaded) {
        for (auto& a_mesh : loaded_meshes) {
          if (!a_mesh.displayed) continue;
          // Calculate all the needed matrices to render the frame, this
          // includes the "model view projection" that transform the geometry to
          // the screen space, the normal matrix, and the joint matrix array
          // that is used to deform the skin with the bones
          precompute_hardware_skinning_data(
              gltf_scene_tree, model_matrix, a_mesh.joint_matrices,
              a_mesh.flat_joint_list, a_mesh.inverse_bind_matrices);

          glm::mat4 mvp = projection_matrix * view_matrix * model_matrix;
          glm::mat3 normal = glm::transpose(glm::inverse(model_matrix));
          update_uniforms(*a_mesh.shader_list, shader_to_use, mvp, normal,
                          a_mesh.joint_matrices);

          // Draw all of the submeshes of the object
          for (size_t submesh = 0;
               submesh < a_mesh.draw_call_descriptors.size(); ++submesh) {
            const auto& draw_call = a_mesh.draw_call_descriptors[submesh];
            perform_software_morphing(gltf_scene_tree, submesh,
                                      a_mesh.morph_targets, a_mesh.vertex_coord,
                                      a_mesh.normals, a_mesh.display_position,
                                      a_mesh.display_normal, a_mesh.VBOs);
            glEnable(GL_DEPTH_TEST);
            glFrontFace(GL_CCW);
            perform_draw_call(draw_call);
          }
          // Then draw 2D bones and joints on top of that

          draw_bone_overlay(gltf_scene_tree, a_mesh.flat_joint_list,
                            view_matrix, projection_matrix,
                            *loaded_meshes[0].shader_list);
        }
      }
    }
    // Render all ImGui, then swap buffers
    gl_gui_end_frame(window);
  }
}
// Private methods here :

std::string app::GetFilePathExtension(const std::string& FileName) {
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

void app::parse_command_line(int argc, char** argv, bool debug_output,
                             std::string& input_filename) const {
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

void app::load_glTF_asset(tinygltf::TinyGLTF& gltf_ctx,
                          const std::string& input_filename,
                          tinygltf::Model& model) {
  std::string err;
  std::string warn;
  const std::string ext = GetFilePathExtension(input_filename);

  bool ret = false;
  if (ext.compare("glb") == 0 || ext.compare("vrm") == 0) {
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

    throw std::runtime_error("error: " + err);
  }
}

void app::load_all_textures(tinygltf::Model model, size_t nb_textures,
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

void app::genrate_joint_inverse_bind_matrix_map(
    const tinygltf::Skin& skin, const std::vector<int>::size_type nb_joints,
    std::map<int, int> joint_inverse_bind_matrix_map) {
  for (int i = 0; i < nb_joints; ++i)
    joint_inverse_bind_matrix_map[skin.joints[i]] = i;
}

void app::cpu_compute_morphed_display_mesh(
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

void app::gpu_update_morphed_submesh(
    size_t submesh_id, std::vector<std::vector<float>>& display_position,
    std::vector<std::vector<float>>& display_normal,
    std::vector<std::array<GLuint, 6>>& VBOs) {
  // upload to GPU
  glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh_id][0]);
  glBufferData(GL_ARRAY_BUFFER,
               display_position[submesh_id].size() * sizeof(float),
               display_position[submesh_id].data(), GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh_id][1]);
  glBufferData(GL_ARRAY_BUFFER,
               display_normal[submesh_id].size() * sizeof(float),
               display_normal[submesh_id].data(), GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void app::perform_software_morphing(
    gltf_node mesh_skeleton_graph, size_t submesh_id,
    const std::vector<std::vector<morph_target>>& morph_targets,
    const std::vector<std::vector<float>>& vertex_coord,
    const std::vector<std::vector<float>>& normals,
    std::vector<std::vector<float>>& display_position,
    std::vector<std::vector<float>>& display_normal,
    std::vector<std::array<GLuint, 6>>& VBOs) {
  // evaluation cache:
  static std::vector<bool> clean;
  static std::vector<std::vector<float>> cached_weights;

  if (mesh_skeleton_graph.pose.blend_weights.size() > 0 &&
      morph_targets[submesh_id].size() > 0) {
    assert(display_position[submesh_id].size() ==
           display_normal[submesh_id].size());

    // We are dynamically keeping a cache of the morph targets weights. CPU-side
    // evaluation of morphing is expensive, if the blending weights did not
    // change, we don't want to re-evaluate the mesh. We are keeping a cache of
    // the weights, and sedding a dirty flags if we need to recompute and
    // reupload the mesh to the GPU

    // To transparently handle the loading of a different file, we resize these
    // arrays here
    if (cached_weights.size() != display_position.size()) {
      cached_weights.resize(display_position.size());
      clean.resize(display_position.size());
      std::generate(clean.begin(), clean.end(), [] { return false; });
    }

    // If the number of blendshape doesn't match, we are just copying the array
    if (cached_weights[submesh_id].size() !=
        mesh_skeleton_graph.pose.blend_weights.size()) {
      clean[submesh_id] = false;
      cached_weights[submesh_id] = mesh_skeleton_graph.pose.blend_weights;
    }

    // If the size matches, we are comparing all the elements, if they match, it
    // means that mesh doesn't need to be evaluated
    else if (cached_weights[submesh_id] ==
             mesh_skeleton_graph.pose.blend_weights) {
      clean[submesh_id] = true;
    }

    // In that case, we are updating the cache, and setting the flag dirty
    else {
      clean[submesh_id] = false;
      cached_weights[submesh_id] = mesh_skeleton_graph.pose.blend_weights;
    }

    // If flag is dirty
    if (!clean[submesh_id]) {
      // Blend between morph targets on the CPU:
      for (size_t vertex = 0; vertex < display_position[submesh_id].size();
           ++vertex) {
        cpu_compute_morphed_display_mesh(
            mesh_skeleton_graph, submesh_id, morph_targets, vertex_coord,
            normals, display_position, display_normal, vertex);
      }

      // Upload the new mesh data to the GPU
      gpu_update_morphed_submesh(submesh_id, display_position, display_normal,
                                 VBOs);
    }
  }
}

void app::draw_bone_overlay(gltf_node& mesh_skeleton_graph,
                            const std::vector<gltf_node*> flat_bone_list,
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

void app::precompute_hardware_skinning_data(
    gltf_node& mesh_skeleton_graph, glm::mat4& model_matrix,
    std::vector<glm::mat4>& joint_matrices,
    std::vector<gltf_node*>& flat_joint_list,
    std::vector<glm::mat4>& inverse_bind_matrices) {
  // This goes through the skeleton hierarchy, and compute the global
  // transform of each joint
  // update_mesh_skeleton_graph_transforms(mesh_skeleton_graph);

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

void app::run_animation_timeline(gltf_insight::AnimSequence& sequence,
                                 bool& looping, int& selectedEntry,
                                 int& firstFrame, bool& expanded,
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
  timeline_window(sequence, playing_state, need_to_update_pose, looping,
                  selectedEntry, firstFrame, expanded, currentFrame,
                  currentPlayTime, &show_timeline);

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

void app::run_3D_gizmo(glm::mat4& view_matrix,
                       const glm::mat4& projection_matrix,
                       glm::mat4& model_matrix, glm::vec3& camera_position,
                       application_parameters& my_user_pointer) {
  float vecTranslation[3], vecRotation[3], vecScale[3];

  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model_matrix),
                                        vecTranslation, vecRotation, vecScale);

  transform_window(vecTranslation, vecRotation, vecScale,
                   mCurrentGizmoOperation, &show_gizmo, &show_transform_window);

  ImGuizmo::RecomposeMatrixFromComponents(vecTranslation, vecRotation, vecScale,
                                          glm::value_ptr(model_matrix));

  auto& io = ImGui::GetIO();
  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
  if (show_gizmo)
    ImGuizmo::Manipulate(glm::value_ptr(view_matrix),
                         glm::value_ptr(projection_matrix),
                         mCurrentGizmoOperation, mCurrentGizmoMode,
                         glm::value_ptr(model_matrix), NULL, NULL);
}

void app::fill_sequencer(gltf_insight::AnimSequence& sequence,
                         const std::vector<animation>& animations) {
  for (auto& animation : animations) {
    // TODO change animation sequencer to use seconds instead of frames
    sequence.myItems.push_back(gltf_insight::AnimSequence::AnimSequenceItem{
        0, int(ANIMATION_FPS * animation.min_time),
        int(ANIMATION_FPS * animation.max_time), false, animation.name});
  }

  if (sequence.myItems.size() > 0) {
    auto max_time = sequence.myItems[0].mFrameEnd;
    for (auto& item : sequence.myItems) {
      max_time = std::max(max_time, item.mFrameEnd);
    }
    sequence.mFrameMax = max_time;
  }
}
