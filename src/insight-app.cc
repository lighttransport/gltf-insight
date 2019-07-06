/*
MIT License

Copyright (c) 2019 Light Transport Entertainment Inc. And many contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "insight-app.hh"

// need matrix decomposition for 3D gizmo
#include "animation.hh"
#include "glm/gtx/matrix_decompose.hpp"

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

  // other number counters and things
  selectedEntry = -1;
  currentFrame = 0;
  playing_state = true;
  camera_position = glm::vec3{0.f, 0.f, 7.f};
  selected_shader = 0;
  sequence.myItems.clear();

  // file input information
  input_filename.clear();

  // mesh data
  empty_gltf_graph(gltf_scene_tree);
  loaded_meshes.clear();
  loaded_material.clear();

  // library resources
  model = tinygltf::Model();

  looping = true;
  selectedEntry = -1;
  firstFrame = 0;
  expanded = true;
}

void app::load_as_metal_roughness(size_t i, material& currently_loading,
                                  const tinygltf::Material gltf_material) {
  currently_loading.intended_shader = shading_type::pbr_metal_rough;
  auto& pbr_metal_rough = currently_loading.shader_inputs.pbr_metal_roughness;
  // tinygltf consider that "values" contains the standard pbr shader values
  for (auto& value : gltf_material.values) {
    if (value.first == "baseColorFactor") {
      const auto base_color = value.second.ColorFactor();
      pbr_metal_rough.base_color_factor.r = float(base_color[0]);
      pbr_metal_rough.base_color_factor.g = float(base_color[1]);
      pbr_metal_rough.base_color_factor.b = float(base_color[2]);
      pbr_metal_rough.base_color_factor.a = float(base_color[3]);
    }

    else if (value.first == "metallicFactor") {
      const auto factor = value.second.Factor();
      if (factor >= 0 && factor <= 1) {
        pbr_metal_rough.metallic_factor = float(factor);
      }
    }

    else if (value.first == "roughnessFactor") {
      const auto factor = value.second.Factor();
      if (factor >= 0 && factor <= 1) {
        pbr_metal_rough.roughness_factor = float(factor);
      }
    }

    else if (value.first == "baseColorTexture") {
      const auto index = value.second.TextureIndex();
      if (index < int(textures.size())) {
        pbr_metal_rough.base_color_texture = textures[size_t(index)];
      }
    }

    else if (value.first == "metallicRoughnessTexture") {
      const auto index = value.second.TextureIndex();
      if (index < int(textures.size())) {
        pbr_metal_rough.metallic_roughness_texture = textures[size_t(index)];
      }
    }

    else {
      std::cerr << "Warn: The value " << value.first
                << " is defined in material " << i
                << " in values but is ignored by loader\n";
    }
  }
}

void app::load() {
  load_glTF_asset();

  const auto nb_textures = model.images.size();
  textures.resize(nb_textures);
  load_all_textures(nb_textures);

  // TODO extract "load_materials" function
  loaded_material.resize(model.materials.size());
  for (size_t i = 0; i < model.materials.size(); ++i) {
    auto& currently_loading = loaded_material[i];
    load_sensible_default_material(currently_loading);
    const auto gltf_material = model.materials[i];

    // start by settings viable defaults!
    currently_loading.normal_texture = fallback_textures::pure_flat_normal_map;
    currently_loading.occlusion_texture = fallback_textures::pure_white_texture;
    currently_loading.emissive_texture = fallback_textures::pure_black_texture;

    currently_loading.name = gltf_material.name;

    for (auto& value : gltf_material.additionalValues) {
      if (value.first == "normalTexture") {
        const auto index = value.second.TextureIndex();
        if (index < int(textures.size()))
          currently_loading.normal_texture = textures[size_t(index)];
      }

      else if (value.first == "occlusionTexture") {
        const auto index = value.second.TextureIndex();
        if (index < int(textures.size()))
          currently_loading.occlusion_texture = textures[size_t(index)];
      }

      else if (value.first == "emissiveTexture") {
        const auto index = value.second.TextureIndex();
        if (index < int(textures.size()))
          currently_loading.emissive_texture = textures[size_t(index)];
      }

      else if (value.first == "emissiveFactor") {
        const auto color_value = value.second.ColorFactor();
        currently_loading.emissive_factor.r = float(color_value[0]);
        currently_loading.emissive_factor.g = float(color_value[1]);
        currently_loading.emissive_factor.b = float(color_value[2]);
      }

      else if (value.first == "alphaCutoff") {
        const auto factor = value.second.Factor();
        if ((factor >= 0.0) && (factor <= 1.0)) {
          currently_loading.alpha_cutoff = float(factor);
        }
      }

      else if (value.first == "alphaMode") {
        if (value.second.string_value == "OPAQUE") {
          currently_loading.alpha_mode = alpha_coverage::opaque;
        } else if (value.second.string_value == "MASK") {
          currently_loading.alpha_mode = alpha_coverage::mask;
        } else if (value.second.string_value == "BLEND") {
          currently_loading.alpha_mode = alpha_coverage::blend;
        } else {
          std::cerr << "Warn: couldn't understand the alphaMode from the "
                       "material...\n";
        }
      }

      else if (value.first == "doubleSided") {
        currently_loading.double_sided = value.second.bool_value;
      }

      else {
        std::cerr << "Warn: The value " << value.first
                  << " is defined in material " << i
                  << " in additionalValues but is ignored by loader\n";
      }
    }

    if (gltf_material.extensions.size() == 0) {
      // No extension = PBR MetalRough
      load_as_metal_roughness(i, currently_loading, gltf_material);
    }

    else {
      const auto unlit_ext_it =
          gltf_material.extensions.find("KHR_materials_unlit");
      const auto specular_glossiness =
          gltf_material.extensions.find("KHR_materials_pbrSpecularGlossiness");

      if (unlit_ext_it != gltf_material.extensions.end()) {
        currently_loading.intended_shader = shading_type::unlit;
        auto& unlit = currently_loading.shader_inputs.unlit;

        for (auto& value : gltf_material.values) {
          // Ignore pbrMetalRoughness fallback parameters
          if (value.first == "metallicFactor" ||
              value.first == "roughnessFactor" ||
              value.first == "metallicRoughnessTexture")
            continue;

          // Load the color
          if (value.first == "baseColorFactor") {
            const auto base_color = value.second.ColorFactor();
            unlit.base_color_factor.r = float(base_color[0]);
            unlit.base_color_factor.g = float(base_color[1]);
            unlit.base_color_factor.b = float(base_color[2]);
            unlit.base_color_factor.a = float(base_color[3]);
          }

          // Load the texture
          else if (value.first == "baseColorTexture") {
            const auto index = value.second.TextureIndex();
            if (index < int(textures.size())) {
              unlit.base_color_texture = textures[size_t(index)];
            }
          }
        }
      } else if (specular_glossiness != gltf_material.extensions.end()) {
        std::cerr << "Warn: Specular glossiness hasn't been implemented yet\n";

        // probe for metallic roughness material values
        if (gltf_material.values.find("metallicRoughnessTexture") !=
                gltf_material.values.end() ||
            gltf_material.values.find("roughnessFactor") !=
                gltf_material.values.end() ||
            gltf_material.values.find("metallicFactor") !=
                gltf_material.values.end()) {
          // We can use metallic roughness as a fallback, as per glTF extension
          // spec §KHR_materials_pbrSpecularGlossiness.best-practices
          load_as_metal_roughness(i, currently_loading, gltf_material);
        }

      } else {
        std::cerr << "Warn: we aren't currently looking at the material "
                     "extension(s) used in this file. Sorry... :-/\n";
      }
    }

    currently_loading.fill_material_texture_slots();
  }

  const auto scene_index = find_main_scene(model);
  const auto& scene = model.scenes[size_t(scene_index)];

  gltf_scene_tree.gltf_node_index = -1;

  if (scene.nodes.size() > 1)
    std::cerr << "Warn: The currently loading scene has multiple root node. We "
                 "create a virtual root node that parent all of them\n";

  // dummy "all parent" node
  for (size_t i = 0; i < scene.nodes.size(); ++i) {
    const auto root_index = scene.nodes[i];
    gltf_scene_tree.add_child();
    auto& root = *gltf_scene_tree.children.back();
    populate_gltf_graph(model, root, root_index);
  }

  set_mesh_attachement(model, gltf_scene_tree);
  auto meshes_indices = get_list_of_mesh_instances(gltf_scene_tree);

  loaded_meshes.resize(meshes_indices.size());
  std::cerr << "Loading " << meshes_indices.size() << " meshes from glTF\n";
  for (size_t i = 0; i < meshes_indices.size(); ++i) {
    std::cerr << "mesh " << i << "\n";

    loaded_meshes[i].instance = meshes_indices[i];
    auto& current_mesh = loaded_meshes[i];

    const auto skin_index =
        model.nodes[size_t(current_mesh.instance.node)].skin;
    if (skin_index >= 0) {
      current_mesh.skinned = true;
      const auto& gltf_skin = model.skins[size_t(skin_index)];
      current_mesh.nb_joints = int(gltf_skin.joints.size());
      create_flat_bone_list(gltf_skin, size_t(current_mesh.nb_joints),
                            gltf_scene_tree, current_mesh.flat_joint_list);

      for (auto joint : current_mesh.flat_joint_list)
        joint->skin_mesh_node =
            gltf_scene_tree.get_node_with_index(current_mesh.instance.node);

      current_mesh.joint_matrices.resize(size_t(current_mesh.nb_joints));
      load_inverse_bind_matrix_array(model, gltf_skin,
                                     size_t(current_mesh.nb_joints),
                                     current_mesh.inverse_bind_matrices);
      genrate_joint_inverse_bind_matrix_map(
          gltf_skin, size_t(current_mesh.nb_joints),
          current_mesh.joint_inverse_bind_matrix_map);
      std::cerr << " This is a skinned mesh with " << current_mesh.nb_joints
                << "joints \n";
    }

    const auto& gltf_mesh = model.meshes[size_t(current_mesh.instance.mesh)];

    if (!gltf_mesh.name.empty())
      current_mesh.name = gltf_mesh.name;
    else
      current_mesh.name = std::string("mesh_") + std::to_string(i);

    std::cerr << "Mesh name: " << current_mesh.name << "\n";

    const auto& gltf_mesh_primitives = gltf_mesh.primitives;
    const auto nb_submeshes = gltf_mesh_primitives.size();

    std::cerr << "Mesh contains " << nb_submeshes
              << "submeshes (glTF primitive)\n";

    current_mesh.draw_call_descriptors.resize(nb_submeshes);
    current_mesh.indices.resize(nb_submeshes);
    current_mesh.positions.resize(nb_submeshes);
    current_mesh.uvs.resize(nb_submeshes);
    current_mesh.colors.resize(nb_submeshes);
    current_mesh.normals.resize(nb_submeshes);
    current_mesh.weights.resize(nb_submeshes);
    current_mesh.joints.resize(nb_submeshes);
    current_mesh.VAOs.resize(nb_submeshes);
    current_mesh.VBOs.resize(nb_submeshes);

    // Create OpenGL objects for submehes
    glGenVertexArrays(GLsizei(nb_submeshes), current_mesh.VAOs.data());
    for (auto& VBO : current_mesh.VBOs) {
      // We have 5 vertex attributes per vertex + 1 element array.
      glGenBuffers(VBO_count, VBO.data());
    }

    // For each submesh of the mesh, load the data
    load_geometry(model, textures, gltf_mesh_primitives,
                  current_mesh.draw_call_descriptors, current_mesh.VAOs,
                  current_mesh.VBOs, current_mesh.indices,
                  current_mesh.positions, current_mesh.uvs, current_mesh.colors,
                  current_mesh.normals, current_mesh.weights,
                  current_mesh.joints);

    current_mesh.display_position = current_mesh.positions;
    current_mesh.display_normals = current_mesh.normals;

    current_mesh.soft_skinned_position = current_mesh.positions;
    current_mesh.soft_skinned_normals = current_mesh.normals;

    // cleanup opengl state
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    current_mesh.shader_list = std::unique_ptr<std::map<std::string, shader>>(
        new std::map<std::string, shader>);

    current_mesh.soft_skin_shader_list =
        std::unique_ptr<std::map<std::string, shader>>(
            new std::map<std::string, shader>);

    current_mesh.morph_targets.resize(nb_submeshes);
    current_mesh.materials.resize(nb_submeshes);
    std::cerr << "loading primitive data:\n";
    for (size_t s = 0; s < nb_submeshes; ++s) {
      std::cerr << "Submesh " << s << "\n";

      current_mesh.materials[s] = gltf_mesh.primitives[s].material;
      std::cerr << "using material index " << current_mesh.materials[s] << "\n";

      current_mesh.morph_targets[s].resize(
          gltf_mesh.primitives[s].targets.size());

      bool has_normals = false;
      bool has_tangents = false;

      load_morph_targets(model, gltf_mesh.primitives[s],
                         current_mesh.morph_targets[s], has_normals,
                         has_tangents);

      if (!has_normals) {
        for (auto& morph_target : current_mesh.morph_targets[s]) {
          morph_target.normal.resize(morph_target.position.size());
          for (size_t tri = 0; tri < current_mesh.indices[s].size() / 3;
               ++tri) {
            const auto i0 = current_mesh.indices[s][3 * i + 0];
            const auto i1 = current_mesh.indices[s][3 * i + 1];
            const auto i2 = current_mesh.indices[s][3 * i + 2];

            // moph the triangle to the full extent of that morph target
            const glm::vec3 v0 = glm::vec3(current_mesh.positions[s][i0 + 0],
                                           current_mesh.positions[s][i0 + 1],
                                           current_mesh.positions[s][i0 + 2]) +
                                 glm::vec3(morph_target.position[i0 + 0],
                                           morph_target.position[i0 + 1],
                                           morph_target.position[i0 + 2]);

            const glm::vec3 v1 = glm::vec3(current_mesh.positions[s][i1 + 0],
                                           current_mesh.positions[s][i1 + 1],
                                           current_mesh.positions[s][i1 + 2]) +
                                 glm::vec3(morph_target.position[i1 + 0],
                                           morph_target.position[i1 + 1],
                                           morph_target.position[i1 + 2]);

            const glm::vec3 v2 = glm::vec3(current_mesh.positions[s][i2 + 0],
                                           current_mesh.positions[s][i2 + 1],
                                           current_mesh.positions[s][i2 + 2]) +
                                 glm::vec3(morph_target.position[i2 + 0],
                                           morph_target.position[i2 + 1],
                                           morph_target.position[i2 + 2]);
            // generate normal vector
            const glm::vec3 morph_n =
                glm::normalize(glm::cross(v0 - v1, v1 - v2));
            const glm::vec3
                unmorph_n =  // we assume flat normals, so we can take the one
                             // from i0, i1 or i2, it doesn't change anything
                glm::vec3(current_mesh.normals[s][i0 + 0],
                          current_mesh.normals[s][i0 + 1],
                          current_mesh.normals[s][i0 + 2]);

            // calculate the delta
            const glm::vec3 n = morph_n - unmorph_n;

            morph_target.normal[i0 + 0] = n.x;
            morph_target.normal[i0 + 1] = n.y;
            morph_target.normal[i0 + 2] = n.z;
            morph_target.normal[i1 + 0] = n.x;
            morph_target.normal[i1 + 1] = n.y;
            morph_target.normal[i1 + 2] = n.z;
            morph_target.normal[i2 + 0] = n.x;
            morph_target.normal[i2 + 1] = n.y;
            morph_target.normal[i2 + 2] = n.z;
          }
        }
      }
    }

    current_mesh.nb_morph_targets = 0;
    for (auto& target : current_mesh.morph_targets) {
      current_mesh.nb_morph_targets =
          std::max<int>(int(target.size()), current_mesh.nb_morph_targets);
    }
    std::vector<std::string> target_names(
        size_t(current_mesh.nb_morph_targets));
    load_morph_target_names(gltf_mesh, target_names);
    gltf_scene_tree.pose.target_names = target_names;

    load_shaders(size_t(current_mesh.nb_joints), *current_mesh.shader_list);
    if (current_mesh.skinned)
      load_shaders(0, *current_mesh.soft_skin_shader_list);
  }

  const auto nb_animations = model.animations.size();
  animations.resize(nb_animations);
  load_animations(model, animations);
  fill_sequencer();

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
  for (size_t i = 1; i < loaded_meshes.size(); ++i) {
    nb_morph_targets =
        std::max(nb_morph_targets, loaded_meshes[i].nb_morph_targets);
  }

  gltf_scene_tree.pose.blend_weights.resize(size_t(nb_morph_targets));
  std::generate(gltf_scene_tree.pose.blend_weights.begin(),
                gltf_scene_tree.pose.blend_weights.end(), [] { return 0.f; });

  animation_names.resize(nb_animations);
  for (size_t i = 0; i < animations.size(); ++i)
    animation_names[i] = animations[i].name;

  asset_loaded = true;

  if (!loaded_meshes.empty()) {
    for (auto shader_it = loaded_meshes[0].shader_list->begin();
         shader_it != loaded_meshes[0].shader_list->end(); ++shader_it)
      shader_names.push_back(shader_it->first);

    for (size_t i = 0; i < shader_names.size(); ++i) {
      if (shader_names[i] == "unlit") {
        selected_shader = int(i);
        break;
      }
    }
  }
}

mesh::~mesh() {
  for (auto& VBO : VBOs) glDeleteBuffers(VBO_count, VBO.data());
  glDeleteVertexArrays(GLsizei(VAOs.size()), VAOs.data());

  displayed = true;
  skinned = false;
  // shader_list.reset(nullptr);

  nb_joints = 0;
  nb_morph_targets = 0;

  joints.clear();
  positions.clear();
  uvs.clear();
  normals.clear();
  weights.clear();
  display_position.clear();
  display_normals.clear();
  indices.clear();
  flat_joint_list.clear();
  joint_inverse_bind_matrix_map.clear();
  joint_matrices.clear();
  colors.clear();
  instance.mesh = -1;
  instance.node = -1;
  draw_call_descriptors.clear();
}

mesh& mesh::operator=(mesh&& o) {
  nb_joints = o.nb_joints;
  nb_morph_targets = o.nb_morph_targets;
  instance = o.instance;
  displayed = o.displayed;
  joint_matrices = std::move(o.joint_matrices);
  joint_inverse_bind_matrix_map = std::move(o.joint_inverse_bind_matrix_map);
  flat_joint_list = std::move(o.flat_joint_list);
  inverse_bind_matrices = std::move(o.inverse_bind_matrices);
  morph_targets = std::move(o.morph_targets);
  draw_call_descriptors = std::move(o.draw_call_descriptors);
  indices = std::move(o.indices);
  positions = std::move(o.positions);
  uvs = std::move(o.uvs);
  normals = std::move(o.normals);
  weights = std::move(o.weights);
  display_position = std::move(o.display_position);
  display_normals = std::move(o.display_normals);
  joints = std::move(o.joints);
  colors = std::move(o.colors);

  shader_list = std::move(o.shader_list);
  return *this;
}

mesh::mesh(mesh&& other) { *this = std::move(other); }

mesh::mesh() : instance() {}

glm::vec3 editor_lighting::get_directional_light_direction() const {
  return glm::normalize(non_normalized_direction);
}

void editor_lighting::show_control() {
  if (control_open) {
    if (ImGui::Begin("Editor light control", &control_open)) {
      ImGui::ColorEdit3("Light color", glm::value_ptr(color));
      ImGui::SliderFloat3("Light Direction Euler",
                          glm::value_ptr(non_normalized_direction), -1, 1);

      // Since what wer are already doing is not ideal to set a light direction
      // vector from an UI... Safeguard against null vector normalization
      // causing NANtastic things
      if (glm::length(non_normalized_direction) == 0.f)
        non_normalized_direction.y -= .001f;

      const auto dir = get_directional_light_direction();
      ImGui::Text("Actual direction is vec3(%.3f, %.3f, %.3f)", double(dir.x),
                  double(dir.y), double(dir.z));
    }
    ImGui::End();
  }
}

void app::load_sensible_default_material(material& material) {
  material.name = "dummy_fallback_material";
  material.normal_texture = fallback_textures::pure_flat_normal_map;
  material.occlusion_texture = fallback_textures::pure_white_texture;
  material.emissive_texture = fallback_textures::pure_black_texture;
  material.shader_inputs.pbr_metal_roughness.metallic_roughness_texture =
      fallback_textures::pure_white_texture;
  material.shader_inputs.pbr_metal_roughness.base_color_texture =
      fallback_textures::pure_white_texture;
  material.fill_material_texture_slots();
}

app::app(int argc, char** argv) {
  parse_command_line(argc, argv);

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

  logo = load_gltf_insight_icon();

  // load fallback material
  setup_fallback_textures();

  load_sensible_default_material(dummy_material);
}

app::~app() {
  unload();
  deinitialize_gui_and_window(window);
}

void app::run_file_menu() {
  if (ImGui::BeginMenu("File")) {
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
}

void app::run_edit_menu() {
  if (ImGui::BeginMenu("Edit")) {
    ImGui::Text("Selection Mode:");
    if (ImGui::RadioButton("Mesh", current_mode == manipulate_mesh))
      current_mode = manipulate_mesh;
    if (ImGui::RadioButton("Bone", current_mode == manipulate_joint))
      current_mode = manipulate_joint;
    ImGui::Separator();

    ImGui::Text("Transform type:");
    if (ImGui::RadioButton("Translate",
                           mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
      mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::RadioButton("Rotate",
                           mCurrentGizmoOperation == ImGuizmo::ROTATE))
      mCurrentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
      mCurrentGizmoOperation = ImGuizmo::SCALE;
    ImGui::Separator();

    ImGui::Text("Transform space:");

    if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
      mCurrentGizmoMode = ImGuizmo::WORLD;

    if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
      mCurrentGizmoMode = ImGuizmo::LOCAL;
    ImGui::Separator();
    ImGui::EndMenu();
  }
}

void app::run_view_menu() {
  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("Show ImGui Demo window", nullptr, &show_imgui_demo);
    ImGui::Separator();
    ImGui::Text("Toggle window display:");
    ImGui::MenuItem("Images", nullptr, &show_asset_image_window);
    ImGui::MenuItem("Model info", nullptr, &show_model_info_window);
    ImGui::MenuItem("Animations", nullptr, &show_animation_window);
    ImGui::MenuItem("Mesh Visibility", nullptr, &show_mesh_display_window);
    ImGui::MenuItem("Morph Target blend weights", nullptr,
                    &show_morph_target_window);
    ImGui::MenuItem("Camera parameters", nullptr,
                    &show_camera_parameter_window);
    ImGui::MenuItem("TransformWindow", nullptr, &show_transform_window);
    ImGui::MenuItem("Bone selector", nullptr, &show_bone_selector);
    ImGui::MenuItem("Timeline", nullptr, &show_timeline);
    ImGui::MenuItem("Shader selector", nullptr, &show_shader_selector_window);
    ImGui::MenuItem("Material info", nullptr, &show_material_window);
    ImGui::MenuItem("Bone display window", nullptr, &show_bone_display_window);
    ImGui::MenuItem("Scene outline", nullptr, &show_scene_outline_window);
    ImGui::Separator();
    ImGui::MenuItem("Show Gizmo", nullptr, &show_gizmo);
    ImGui::MenuItem("Editor light controls", nullptr,
                    &editor_light.control_open);
    ImGui::EndMenu();
  }
}

void app::run_debug_menu() {
  static bool wait_next_frame = false;
  bool write = false;
  if (ImGui::BeginMenu("DEBUG")) {
    if (ImGui::MenuItem("call unload()")) unload();
    if (ImGui::MenuItem("save test.obj NOW") || wait_next_frame) {
      if (!do_soft_skinning) {
        wait_next_frame = true;
      } else {
        wait_next_frame = false;
        write = true;
      }
    }
    ImGui::EndMenu();
  }

  if (write) {
    tinyobj::ObjWriter writer;

    // just one mesh and one shape for now

    writer.attrib_.vertices = loaded_meshes[0].soft_skinned_position[0];
    writer.attrib_.normals = loaded_meshes[0].soft_skinned_normals[0];
    writer.attrib_.texcoords = loaded_meshes[0].uvs[0];
    // const auto nb_ws = loaded_meshes[0].uvs[0].size() / 2;
    // std::vector<float> ws(nb_ws);
    // std::generate(ws.begin(), ws.end(), [] { return 0; });

    tinyobj::shape_t shape;
    shape.name = loaded_meshes[0].name;

    const auto nb_triangles = loaded_meshes[0].indices[0].size() / 3;
    shape.mesh.num_face_vertices.resize(nb_triangles);
    std::generate(shape.mesh.num_face_vertices.begin(),
                  shape.mesh.num_face_vertices.end(), [] { return 3; });

    for (size_t i = 0; i < loaded_meshes[0].indices[0].size(); ++i) {
      tinyobj::index_t index;
      index.vertex_index = loaded_meshes[0].indices[0][i];
      index.normal_index = loaded_meshes[0].indices[0][i];
      index.texcoord_index = loaded_meshes[0].indices[0][i];
      shape.mesh.indices.push_back(index);
    }

    writer.shapes_.push_back(shape);

    writer.SaveTofile("./out");
  }
}

void app::run_help_menu(bool& about_open) {
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("About...")) about_open = true;
    if (ImGui::MenuItem("See on GitHub"))
      open_url("https://github.com/lighttransport/gltf-insight/");
    ImGui::EndMenu();
  }
}

void app::run_menubar(bool& about_open) {
  ImGui::BeginMainMenuBar();
  run_file_menu();
  run_edit_menu();
  run_view_menu();
  run_debug_menu();
  run_help_menu(about_open);

  ImGui::EndMainMenuBar();
}

void app::create_transparent_docking_area(const ImVec2 pos, const ImVec2 size,
                                          std::string name) {
  using namespace ImGui;

  const auto window_name = name + "_window";
  const auto dockspace_name = name + "_dock";

  ImGuiDockNodeFlags dockspace_flags =
      ImGuiDockNodeFlags_PassthruCentralNode |
      ImGuiDockNodeFlags_NoDockingInCentralNode;

  SetNextWindowPos(pos);
  SetNextWindowSize(size);

  ImGuiWindowFlags host_window_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

  PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  Begin(window_name.c_str(), nullptr, host_window_flags);
  PopStyleVar(3);  // we had 3 things added on the stack

  const ImGuiID dockspace_id = GetID(dockspace_name.c_str());
  DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

  End();
}

void app::draw_mesh(const glm::vec3& world_camera_position, const mesh& mesh,
                    glm::mat3 normal_matrix, glm::mat4 model_matrix)

{
  for (size_t submesh = 0; submesh < mesh.draw_call_descriptors.size();
       ++submesh) {
    const auto& material_to_use =
        loaded_material[size_t(mesh.materials[submesh])];

    if (current_display_mode == display_mode::normal) {
      switch (material_to_use.intended_shader) {
        case shading_type::pbr_metal_rough:
          shader_to_use = "pbr_metal_rough";
          break;
        case shading_type::pbr_specular_glossy:
          shader_to_use = "pbr_metal_rough";
          {
            static bool first_print = true;
            if (first_print) {
              std::cout << "Warn: unimplemented specular_blossy shader mode "
                           "required.\n";
              first_print = false;
            }
          }
          break;
        case shading_type::unlit:
          shader_to_use = "unlit";
          break;
      }
    }

    material_to_use.bind_textures();

    auto& shader_list = mesh.skinned && do_soft_skinning
                            ? *mesh.soft_skin_shader_list
                            : *mesh.shader_list;

    const auto& active_shader = shader_list[shader_to_use];

    active_shader.use();
    material_to_use.set_shader_uniform(active_shader);
    update_uniforms(shader_list, editor_light.use_ibl, world_camera_position,
                    editor_light.color,
                    editor_light.get_directional_light_direction(),
                    active_joint, shader_to_use,
                    projection_matrix * view_matrix * model_matrix,
                    projection_matrix * view_matrix * model_matrix,
                    normal_matrix, mesh.joint_matrices);

    const auto& draw_call = mesh.draw_call_descriptors[submesh];
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    if (!material_to_use.double_sided) {
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
      glFrontFace(GL_CCW);
    } else {
      glDisable(GL_CULL_FACE);
    }

    if (material_to_use.alpha_mode == alpha_coverage::blend) {
      glDisable(GL_CULL_FACE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
    }

    perform_draw_call(draw_call);
  }
}

void app::draw_scene_recur(const glm::vec3& world_camera_position,
                           gltf_node& node,
                           std::vector<defered_draw>& alpha_models) {
  for (auto child : node.children)
    draw_scene_recur(world_camera_position, *child, alpha_models);

  if (node.type == gltf_node::node_type::mesh) {
    auto& mesh = loaded_meshes[size_t(node.gltf_mesh_id)];

    bool defer = false;
    for (auto material : mesh.materials)
      if (loaded_material[size_t(material)].alpha_mode ==
          alpha_coverage::blend) {
        defer = true;
        break;
      }

    if (!defer) {
      const glm::mat3 normal_matrix =
          glm::transpose(glm::inverse(node.world_xform));
      draw_mesh(world_camera_position, mesh, normal_matrix, node.world_xform);
    } else {
      alpha_models.push_back(node.get_ptr());
    }
  }
}

void gltf_insight::app::draw_scene(const glm::vec3& world_camera_position) {
  std::vector<defered_draw> alpha;
  draw_scene_recur(world_camera_position, gltf_scene_tree, alpha);

  if (!alpha.empty()) {
    {
      // sort by distance from camera
      if (alpha.size() > 1)
        std::sort(alpha.begin(), alpha.end(),
                  [&](const defered_draw& a, const defered_draw b) {
                    const glm::vec3 a_pos = world_camera_position -
                                            glm::vec3(a.node->world_xform[3]);
                    const glm::vec3 b_pos = world_camera_position -
                                            glm::vec3(b.node->world_xform[3]);

                    return glm::length2(a_pos) < glm::length2(b_pos);
                  });

      for (const auto& to_draw : alpha) {
        auto node = to_draw.node;
        const auto& mesh = loaded_meshes[size_t(node->gltf_mesh_id)];
        const glm::mat3 normal_matrix =
            glm::transpose(glm::inverse(node->world_xform));
        draw_mesh(world_camera_position, mesh, normal_matrix,
                  node->world_xform);
      }
    }
  }
}

void app::main_loop() {
  bool status = true;
  while (status) {
    status = main_loop_frame();
  }
}

bool app::main_loop_frame() {
  {
    // GUI
    gui_new_frame();

    static bool about_open = false;
    run_menubar(about_open);

    const auto display_size = ImGui::GetIO().DisplaySize;
    create_transparent_docking_area(
        ImVec2(0, 20),
        ImVec2(display_size.x,
               !show_timeline
                   ? display_size.y - 20
                   : display_size.y - 20 -
                         std::min(lower_docked_prop_size * display_size.y,
                                  lower_docked_max_px_size)),
        "main_dockspace");

    about_window(logo, &about_open);

    if (show_imgui_demo) {
      ImGui::ShowDemoWindow(&show_imgui_demo);
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
            std::cerr << "error occured during loading of " << input_filename
                      << ": " << e.what() << '\n';
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
      else if (ImGuiFileDialog::Instance()->FileDialog(
                   "Save as...", nullptr, true, ".", input_filename)) {
        if (ImGuiFileDialog::Instance()->IsOk) {
          auto save_as_filename =
              ImGuiFileDialog::Instance()->GetFilepathName();
          try {
            // Check file path extension for gltf or glb. Fix it if
            // necessary.

            // check if file exist, if so PROMPT BEFORE OVERWRITE

            // Serialize to file

            throw std::runtime_error("Not implemented yet!");
          } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            // display error here
          }
        } else {
        }
        save_file_dialog = false;
      }
    }
    camera_parameters_window(fovy, z_far, &show_camera_parameter_window);

    if (asset_loaded) {
      // Draw all windows
      scene_outline_window(gltf_scene_tree, &show_scene_outline_window);
      model_info_window(model, &show_model_info_window);
      asset_images_window(textures, &show_asset_image_window);
      animation_window(animations, &show_animation_window);
      // skinning_data_window(weights, joints);
      mesh_display_window(loaded_meshes, &show_mesh_display_window);
      morph_target_window(gltf_scene_tree,
                          loaded_meshes.front().nb_morph_targets,
                          &show_morph_target_window);
      shader_selector_window(shader_names, selected_shader, shader_to_use,
                             reinterpret_cast<int&>(current_display_mode),
                             &show_shader_selector_window);

      material_info_window(dummy_material, loaded_material,
                           &show_material_window);

      bone_display_window(&show_bone_display_window);

      editor_light.show_control();

      if (show_bone_selector) {
        if (ImGui::Begin("Bone selector", &show_bone_selector))
          ImGui::InputInt("Active joint", &active_joint, 1, 1);
        ImGui::End();
      }

      active_joint = glm::clamp(active_joint, 0, loaded_meshes[0].nb_joints);
    }

    // Animation player advances time and apply animation interpolation.
    // It also display the sequencer timeline and controls on screen
    run_animation_timeline(sequence, looping, selectedEntry, firstFrame,
                           expanded, currentFrame, currentPlayTime,
                           last_frame_time, playing_state, animations);
  }

  {
    // 3D rendering
    gl_new_frame(window, viewport_background_color, display_w, display_h);
    if (display_h && display_w)  // not zero please
      projection_matrix =
          glm::perspective(glm::radians(fovy),
                           float(display_w) / float(display_h), z_near, z_far);

    run_3D_gizmo(
        asset_loaded && (active_joint >= 0) &&
                (active_joint < int(loaded_meshes[0].flat_joint_list.size()))
            ? loaded_meshes[0].flat_joint_list[size_t(active_joint)]
            : nullptr);

    update_mesh_skeleton_graph_transforms(gltf_scene_tree);

    const glm::quat camera_rotation(
        glm::vec3(glm::radians(gui_parameters.rot_pitch),
                  glm::radians(gui_parameters.rot_yaw), 0.f));
    const auto world_camera_position = camera_rotation * camera_position;

    view_matrix = glm::lookAt(world_camera_position, glm::vec3(0.f),
                              camera_rotation * glm::vec3(0, 1.f, 0));

    bool gpu_dirty = false;
    if (ImGui::Checkbox("Software skinning", &do_soft_skinning)) {
      if (!do_soft_skinning)  // if we clicked on this, and we are not doing
                              // soft skinning anymore, gpu buffer contains
                              // pre-skinned garbage
      {
        gpu_dirty = true;
      }
    }
    if (asset_loaded) {
      int active_bone_gltf_node = -1;
      for (auto& a_mesh : loaded_meshes) {
        if (!a_mesh.displayed) continue;

        if ((active_joint >= 0) &&
            (active_joint < int(a_mesh.flat_joint_list.size())))
          active_bone_gltf_node =
              a_mesh.flat_joint_list[size_t(active_joint)]->gltf_node_index;

        // Calculate all the needed matrices to render the frame, this
        // includes the "model view projection" that transform the geometry to
        // the screen space, the normal matrix, and the joint matrix array
        // that is used to deform the skin with the bones
        compute_joint_matrices(gltf_scene_tree, root_node_model_matrix,
                               a_mesh.joint_matrices, a_mesh.flat_joint_list,
                               a_mesh.inverse_bind_matrices);

        for (size_t submesh = 0; submesh < a_mesh.draw_call_descriptors.size();
             ++submesh) {
          if (gltf_scene_tree.pose.blend_weights.size() > 0)
            perform_software_morphing(
                gltf_scene_tree, submesh, a_mesh.morph_targets,
                a_mesh.positions, a_mesh.normals, a_mesh.display_position,
                a_mesh.display_normals, a_mesh.VBOs,
                a_mesh.skinned ? !do_soft_skinning : true);

          // do not upload to GPU if soft skin is on

          if (a_mesh.skinned) {
            if (do_soft_skinning) {
              perform_software_skinning(
                  submesh, a_mesh.joint_matrices, a_mesh.display_position,
                  a_mesh.display_normals, a_mesh.joints, a_mesh.weights,
                  a_mesh.soft_skinned_position, a_mesh.soft_skinned_normals);
              // now, upload new mesh to GPU
              gpu_update_morphed_submesh(submesh, a_mesh.soft_skinned_position,
                                         a_mesh.soft_skinned_normals,
                                         a_mesh.VBOs);
            } else if (gpu_dirty) {
              gpu_update_morphed_submesh(submesh, a_mesh.display_position,
                                         a_mesh.display_normals, a_mesh.VBOs);
            }
          }
        }
      }

      draw_scene(world_camera_position);

      for (auto& mesh : loaded_meshes)
        draw_bone_overlay(gltf_scene_tree, active_bone_gltf_node, view_matrix,
                          projection_matrix, *loaded_meshes[0].shader_list,
                          mesh);
    }
  }
  // Render all ImGui, then swap buffers
  gl_gui_end_frame(window);
  return !glfwWindowShouldClose(window);
}

// Private methods here :

std::string app::GetFilePathExtension(const std::string& FileName) {
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

void app::parse_command_line(int argc, char** argv) {
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

void app::load_glTF_asset() {
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

void app::load_all_textures(size_t nb_textures) {
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
    std::map<int, int>& joint_inverse_bind_matrix_map) {
  for (size_t i = 0; i < nb_joints; ++i) {
    joint_inverse_bind_matrix_map[skin.joints[i]] = int(i);
  }
}

void app::cpu_compute_morphed_display_mesh(
    gltf_node mesh_skeleton_graph, size_t submesh_id,
    const std::vector<std::vector<morph_target>>& morph_targets,
    const std::vector<std::vector<float>>& vertex_coord,
    const std::vector<std::vector<float>>& normals,
    std::vector<std::vector<float>>& display_position,
    std::vector<std::vector<float>>& display_normal,

    size_t vertex) {
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
    std::vector<std::array<GLuint, VBO_count>>& VBOs) {
  // upload to GPU
  glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh_id][VBO_layout_position]);
  glBufferData(GL_ARRAY_BUFFER,
               GLsizeiptr(display_position[submesh_id].size() * sizeof(float)),
               display_position[submesh_id].data(), GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh_id][VBO_layout_normal]);
  glBufferData(GL_ARRAY_BUFFER,
               GLsizeiptr(display_normal[submesh_id].size() * sizeof(float)),
               display_normal[submesh_id].data(), GL_DYNAMIC_DRAW);

  //// TODO create #defines for these layout numbers, they are arbitrary
  // glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh_id][TANGENT_BUFFER]);
  // glBufferData(GL_ARRAY_BUFFER,
  //             display_tangent[submesh_id].size() * sizeof(float),
  //             display_tangent[submesh_id].data(), GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void app::perform_software_morphing(
    gltf_node mesh_skeleton_graph, size_t submesh_id,
    const std::vector<std::vector<morph_target>>& morph_targets,
    const std::vector<std::vector<float>>& vertex_coord,
    const std::vector<std::vector<float>>& normals,
    std::vector<std::vector<float>>& display_position,
    std::vector<std::vector<float>>& display_normal,
    std::vector<std::array<GLuint, VBO_count>>& VBOs, bool upload_to_gpu) {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif
  // evaluation cache:
  static std::vector<bool> clean;
  static std::vector<std::vector<float>> cached_weights;
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  if (mesh_skeleton_graph.pose.blend_weights.size() > 0 &&
      morph_targets[submesh_id].size() > 0) {
    assert(display_position[submesh_id].size() ==
           display_normal[submesh_id].size());

    // We are dynamically keeping a cache of the morph targets weights. CPU-side
    // evaluation of morphing is expensive, if the blending weights did not
    // change, we don't want to re-evaluate the mesh. We are keeping a cache of
    // the weights, and sending a dirty flags if we need to recompute and
    // re-upload the mesh to the GPU

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
      if (upload_to_gpu)
        gpu_update_morphed_submesh(submesh_id, display_position, display_normal,
                                   VBOs);
    }
  }
}

void app::perform_software_skinning(
    size_t submesh_id, const std::vector<glm::mat4>& joint_matrix,
    const std::vector<std::vector<float>>& positions,
    const std::vector<std::vector<float>>& normals,
    const std::vector<std::vector<unsigned short>>& joints,
    const std::vector<std::vector<float>>& weights,
    std::vector<std::vector<float>>& display_position,
    std::vector<std::vector<float>>& display_normal) {
  // Fetch the arrays for this primitive
  const auto& prim_positions = positions[submesh_id];
  const auto& prim_normals = normals[submesh_id];
  const auto& prim_joints = joints[submesh_id];
  const auto& prim_weights = weights[submesh_id];

  // We need the data sizes to match for what we do to work
  const auto vertex_count = prim_joints.size() / 4;
  assert(prim_positions.size() / 3 == vertex_count &&
         prim_normals.size() / 3 == vertex_count &&
         prim_joints.size() / 4 == vertex_count &&
         prim_weights.size() / 4 == vertex_count);

  for (size_t vertex = 0; vertex < vertex_count; ++vertex) {
    using namespace glm;

    vec3 output_position, output_normal;
    auto input_positions =
        vec3(prim_positions[3 * vertex + 0], prim_positions[3 * vertex + 1],
             prim_positions[3 * vertex + 2]);
    auto input_normals =
        vec3(prim_normals[3 * vertex + 0], prim_normals[3 * vertex + 1],
             prim_normals[3 * vertex + 2]);
    auto input_joints =
        vec4(prim_joints[4 * vertex + 0], prim_joints[4 * vertex + 1],
             prim_joints[4 * vertex + 2], prim_joints[4 * vertex + 3]);
    auto input_weights =
        vec4(prim_weights[4 * vertex + 0], prim_weights[4 * vertex + 1],
             prim_weights[4 * vertex + 2], prim_weights[4 * vertex + 3]);

    // TODO it is possible to support more than 4 joints per vertex, but not
    // required by glTF spec
    const mat4 skin_matrix =
        input_weights.x * joint_matrix[size_t(input_joints.x)] +
        input_weights.y * joint_matrix[size_t(input_joints.y)] +
        input_weights.z * joint_matrix[size_t(input_joints.z)] +
        input_weights.w * joint_matrix[size_t(input_joints.w)];

    auto skinned_position = skin_matrix * vec4(input_positions, 1.f);
    auto normal_skin_matrix = mat3(transpose(inverse(skin_matrix)));

    output_position = vec3(skinned_position) / skinned_position.w;
    output_normal = normal_skin_matrix * input_normals;

    memcpy(&display_position[submesh_id][3 * vertex],
           value_ptr(output_position), 3 * sizeof(float));
    memcpy(&display_normal[submesh_id][3 * vertex], value_ptr(output_normal),
           3 * sizeof(float));
  }
}

void app::draw_bone_overlay(gltf_node& mesh_skeleton_graph,
                            int active_joint_node,
                            const glm::mat4& _view_matrix,
                            const glm::mat4& _projection_matrix,
                            std::map<std::string, shader>& shaders,
                            const mesh& a_mesh) {
  if (!a_mesh.skinned) return;

  glBindVertexArray(0);
  glDisable(GL_DEPTH_TEST);

  // bone_display_window(&show_bone_display_window);
  shaders["debug_color"].use();
  draw_bones(mesh_skeleton_graph, active_joint_node,
             shaders["debug_color"].get_program(), _view_matrix,
             _projection_matrix, a_mesh);
}

void app::compute_joint_matrices(
    gltf_node& mesh_skeleton_graph, glm::mat4& model_matrix,
    std::vector<glm::mat4>& joint_matrices,
    std::vector<gltf_node*>& flat_joint_list,
    std::vector<glm::mat4>& inverse_bind_matrices) {
  (void)mesh_skeleton_graph;

  if (flat_joint_list.size() ==
      0) {  // TODO temp bodge trying to load a VRM file
    return;
  }

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

void app::run_animation_timeline(gltf_insight::AnimSequence& _sequence,
                                 bool& _looping, int& _selectedEntry,
                                 int& _firstFrame, bool& _expanded,
                                 int& _currentFrame, double& _currentPlayTime,
                                 double& _last_frame_time, bool& _playing_state,
                                 std::vector<animation>& _animations) {
  // let's create the sequencer
  double current_time = glfwGetTime();
  bool need_to_update_pose = false;
  if (_playing_state) {
    _currentPlayTime += current_time - _last_frame_time;
  }

  currentFrame = int(double(ANIMATION_FPS) * _currentPlayTime);

  lower_docked_prop_size = 0.25f;
  lower_docked_max_px_size = 300;
  timeline_window(_sequence, _playing_state, need_to_update_pose, _looping,
                  _selectedEntry, _firstFrame, _expanded, currentFrame,
                  _currentPlayTime, &show_timeline, lower_docked_prop_size,
                  lower_docked_max_px_size);

  // loop the sequencer now: TODO replace that true with a "is looping"
  // boolean
  if (_looping && _currentFrame > _sequence.mFrameMax) {
    _currentFrame = _sequence.mFrameMin;
    _currentPlayTime = double(_currentFrame) / double(ANIMATION_FPS);
  }

  for (auto& anim : _animations) {
    anim.set_time(float(_currentPlayTime));  // TODO handle timeline position
    // of animaiton sequence
    anim.playing = _playing_state;
    if (need_to_update_pose || _playing_state) anim.apply_pose();
  }

  last_frame_time = current_time;
}

void app::run_3D_gizmo(gltf_node* active_bone) {
  glm::vec3 vecTranslation, vecRotation, vecScale;
  glm::mat4 bone_world_xform;
  glm::mat4 bind_matrix(1.f), joint_matrix(1.f), delta_matrix(1.f),
      before_manipulate_matrix(1.f);
  gltf_node* mesh_node = nullptr;
  mesh* a_mesh = nullptr;

  // If we got a null pointer for the active bone, the mode *has* to be mesh
  // manipulation, because bone manipulation in that case would not make any
  // sense
  if (!active_bone) {
    current_mode = manipulate_mesh;
  }

  // Otherwise, there's a currently selected "active" bone. We need to calculate
  // the world matrix of how it's drawn according to the mesh it is attached to.
  // This is not the actual world xform of the gltf node that represnt it, but a
  // transform relative to the mesh calculated from it's bind matrix and joint
  // matrix. These matrices have already been loaded and/or computed, they
  // actually depend (for the joint matrix) of that world xoform, we retreive
  // them so we will not do the work twice:
  else {
    // Get the mesh
    a_mesh = &loaded_meshes[size_t(active_bone->skin_mesh_node->gltf_mesh_id)];
    mesh_node = gltf_scene_tree.get_node_with_index(a_mesh->instance.node);

    // Get the number of the node inside the mesh data (it's index for the
    // "joint matrices", "inverse bind matrices" and "flat bone list" arrays)
    const auto joint_index = size_t(
        a_mesh->joint_inverse_bind_matrix_map.at(active_bone->gltf_node_index));

    // Compute the world transform
    bind_matrix = glm::inverse(a_mesh->inverse_bind_matrices[joint_index]);
    joint_matrix = a_mesh->joint_matrices[joint_index];
    bone_world_xform = mesh_node->world_xform * joint_matrix * bind_matrix;
  }

  // The mode to be used is modifiable by the window we are going to display. If
  // that mode change, we need to wait before next frame to be able to
  // manipulate a transform reliably
  const auto saved_mode = current_mode;

  // Select the matrix to use depending on currently selected mode
  float* manipulated_matrix =
      glm::value_ptr(current_mode == manipulate_mesh ? root_node_model_matrix
                                                     : bone_world_xform);

  // We need to get "human understandable" numbers from that matrix for the
  // position/rotation/scale.
  ImGuizmo::DecomposeMatrixToComponents(
      manipulated_matrix, glm::value_ptr(vecTranslation),
      glm::value_ptr(vecRotation), glm::value_ptr(vecScale));

  // This is used to check if user manipulated these values in the GUI, to avoid
  // problems of numerical stability
  const glm::vec3 savedTr = vecTranslation, savedRot = vecRotation,
                  savedScale = vecScale;

  // Display the transform window
  transform_window(glm::value_ptr(vecTranslation), glm::value_ptr(vecRotation),
                   glm::value_ptr(vecScale), mCurrentGizmoOperation,
                   mCurrentGizmoMode, reinterpret_cast<int*>(&current_mode),
                   &show_gizmo, &show_transform_window);

  // If there's no active bone, make sure selected mode *stays* as mesh, even if
  // user's changed to bone mode
  if (!active_bone) {
    current_mode = manipulate_mesh;
  }

  // If the mode has been changed by the user, return. We will do manipulation
  // for the next frame
  if (current_mode != saved_mode)
    return;  // If we just gone from mesh to bone, we are not manipulating the

  // If any of these values has been changed in the GUI, recompose the
  // manipulated matrix from these TRS vectors
  if (savedTr != vecTranslation || savedRot != vecRotation ||
      savedScale != vecScale)
    ImGuizmo::RecomposeMatrixFromComponents(
        glm::value_ptr(vecTranslation), glm::value_ptr(vecRotation),
        glm::value_ptr(vecScale), manipulated_matrix);

  // If we display the gizmo
  if (show_gizmo) {
    // Get the screen geometry from ImGui and pass it to ImGuizmo:
    auto& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // Save this unchanged matrix, it is going to be useful
    before_manipulate_matrix = glm::make_mat4(manipulated_matrix);
    ImGuizmo::Manipulate(
        glm::value_ptr(view_matrix), glm::value_ptr(projection_matrix),
        mCurrentGizmoOperation, mCurrentGizmoMode, manipulated_matrix,
        static_cast<float*>(glm::value_ptr(delta_matrix)));
  }

  // If we are manipulating the mesh, the matrix has been updated as it should
  // have been. However, if we are manipulating a joint, we need to update the
  // "pose" information of that joint, not that joint transform matrix. The
  // animation system uses the pose data to calculate these xform, so they
  // would overriden immediately, plus the actual displayed bone location is
  // aligned with the mesh, it is not the actual xform of the joint node in
  // the glTF scene graph.
  if (current_mode == manipulate_joint) {
    // Pose is relative to the parent. Attempt to fetch a parent node xform. If
    // we don't find any parent node, assume parent is the world root, and thus
    // has an identity xform
    glm::mat4 parent_bone_world_xform(1.f);
    if (active_bone->parent) {
      parent_bone_world_xform = active_bone->parent->world_xform;
    }

    // The pose is expressed in the world referential, but skeleton and
    // manipulator are in the mesh's referential. The skeleton and mesh don't
    // necessary line-up!
    auto currently_posed =
        // The current pose of the bone : it's xform relative to the parent glTF
        // node
        glm::inverse(parent_bone_world_xform) * active_bone->world_xform *

        // The manipulation made with the gizmo by taking the new mesh-oriented
        // world xform, relative to the untouched xform
        glm::inverse(before_manipulate_matrix) * bone_world_xform;

    // The easiest way to detect that something was changed is to use this delta
    // matrix. We cannot use it to apply the transformation because it's not in
    // the right referential, however, if the object wasn't manipulated, it's an
    // identity matrix. We could apply it at every frame, but the result could
    // drift due to floating point numerical instability while
    // decomposing/recomposing TRS matrices that way
    if (delta_matrix != glm::mat4(1.f)) {
      // we actually don't need the skew and perpective ones, but GLM API does
      static glm::vec3 position(0.f), scale(1.f), skew(1.f);
      static glm::quat rotation(1.f, 0.f, 0.f, 0.f);
      static glm::vec4 perspective(1.f);
      glm::decompose(currently_posed, scale, rotation, position, skew,
                     perspective);

      gltf_node::animation_state& pose = active_bone->pose;
      pose.translation = position;
      pose.rotation = rotation;
      pose.scale = scale;
    }
  }
}

void app::fill_sequencer() {
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

#include <cstdlib>
#ifdef WIN32
#include <Windows.h>
#include <shellapi.h>
#endif
#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CFBundle.h>
#endif
void app::open_url(std::string url) {
#if defined(WIN32)
  (void)ShellExecuteA(nullptr, nullptr, url.c_str(), nullptr, nullptr, SW_SHOW);
#elif defined(__linux__)
  std::string command = "xdg-open " + url;
  (void)std::system(command.c_str());
#elif defined(__APPLE__)
  CFURLRef cfurl = CFURLCreateWithBytes(
      nullptr,                                      // allocator
      reinterpret_cast<const UInt8*>(url.c_str()),  // URLBytes
      static_cast<long>(url.length()),              // length
      kCFStringEncodingASCII,                       // encoding
      nullptr                                       // baseURL
  );
  LSOpenCFURLRef(cfurl, nullptr);
  CFRelease(cfurl);
#else
  std::cerr
      << "Warn: Cannot open URLs on this platform. We should have displayed "
      << url << ". Please send bug request on github\n";
#endif
}
