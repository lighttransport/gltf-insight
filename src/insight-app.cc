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

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
// These are due to static ctors/dtors only
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#if defined(GLTF_INSIGHT_WITH_NATIVEFILEDIALOG)
#include "nfd.h"
#endif

// need matrix decomposition for 3D gizmo
#include "animation.hh"
#include "glm/gtx/matrix_decompose.hpp"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <tuple>
using namespace gltf_insight;

color_identifier mesh::selection_id_counter = 0xFF000000;
glm::vec3 app::debug_start, app::debug_stop, app::active_poly_indices;

int app::active_mesh_index = -1;
int app::active_submesh_index = -1;
int app::active_vertex_index = -1;
int app::active_joint_index_model = -1;

float app::z_near = 0.1f;
float app::z_far = 100.f;

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

  active_poly_indices = glm::vec3(0);
  active_submesh_index = -1;
  active_mesh_index = -1;
  active_vertex_index = -1;
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
    } else {
      current_mesh.skinned = false;
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
    current_mesh.submesh_selection_ids.resize(nb_submeshes);
    std::generate(current_mesh.submesh_selection_ids.begin(),
                  current_mesh.submesh_selection_ids.end(), [] {
                    mesh::selection_id_counter.next();
                    return mesh::selection_id_counter;
                  });

    // Create OpenGL objects for submehes
    glGenVertexArrays(GLsizei(nb_submeshes), current_mesh.VAOs.data());
    for (auto& VBO : current_mesh.VBOs) {
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

bool mesh::raycast_submesh_camera_mouse(glm::mat4 world_xform, size_t submesh,
                                        glm::vec3 world_camera_position,
                                        glm::mat4 vp, float x, float y) const {
  constexpr size_t stride = 3 * sizeof(float);
  std::vector<float> world_positions(positions[submesh].size());
  auto* index_buffer = &indices[submesh];
  std::vector<unsigned> reserve_buffer;

  if (draw_call_descriptors[submesh].draw_mode != GL_TRIANGLES) {
    switch (draw_call_descriptors[submesh].draw_mode) {
      default:
        return false;

      case GL_TRIANGLE_FAN: {
        const unsigned first_index = (*index_buffer)[0];
        unsigned last_index_used = (*index_buffer)[1];
        const auto nb_triangles = index_buffer->size() - 2;
        reserve_buffer.resize(3 * (nb_triangles));
        for (size_t i = 0; i < nb_triangles; ++i) {
          reserve_buffer[3 * i + 0] = first_index;
          reserve_buffer[3 * i + 1] = last_index_used;
          last_index_used = (*index_buffer)[2 + i];
          reserve_buffer[3 * i + 2];
        }
        index_buffer = &reserve_buffer;
      } break;
      case GL_TRIANGLE_STRIP: {
        const auto nb_triangles = index_buffer->size() - 2;
        reserve_buffer.resize(3 * (nb_triangles));
        for (size_t i = 0; i < nb_triangles; ++i) {
          reserve_buffer[3 * i + 0] = (*index_buffer)[2 + i];
          reserve_buffer[3 * i + 1] = (*index_buffer)[1 + i];
          reserve_buffer[3 * i + 2] = (*index_buffer)[i];
        }
        index_buffer = &reserve_buffer;
      } break;
    }
  }

  const auto nb_triangles = index_buffer->size() / 3;
  (void)nb_triangles;

  for (size_t v = 0; v < world_positions.size() / 3; ++v) {
    const auto& model_vertex_buffer =
        skinned ? soft_skinned_position : display_position;

    glm::vec4 projected =
        world_xform *
        glm::vec4(glm::make_vec3(&model_vertex_buffer[submesh][3 * v]), 1.f);

    world_positions[3 * v + 0] = projected.x;
    world_positions[3 * v + 1] = projected.y;
    world_positions[3 * v + 2] = projected.z;
  }

  auto triangle_mesh = nanort::TriangleMesh<float>(
      world_positions.data(), index_buffer->data(), stride);
  auto triangle_sha_pred = nanort::TriangleSAHPred<float>(
      world_positions.data(), index_buffer->data(), stride);

  nanort::BVHBuildOptions<float> build_options;
  nanort::BVHAccel<float> accel;
  accel.Build(static_cast<unsigned int>(index_buffer->size()) / 3,
              triangle_mesh, triangle_sha_pred, build_options);

  nanort::Ray<float> mouse_ray;
  mouse_ray.org[0] = world_camera_position.x;
  mouse_ray.org[1] = world_camera_position.y;
  mouse_ray.org[2] = world_camera_position.z;

  auto inverse_vp = glm::inverse(vp);

  // flip Y axis
  y = 1.0f - y;
  glm::vec4 mouse_world_coordiantes =
      inverse_vp * glm::vec4(2.f * x - 1.f, 2.f * y - 1.f, -100.f, 1.f);
  mouse_world_coordiantes /= mouse_world_coordiantes.w;

  glm::vec3 mouse_ray_direction = glm::normalize(
      (glm::vec3(mouse_world_coordiantes) - world_camera_position));

  const auto debug_direction =
      world_camera_position + 50.f * mouse_ray_direction;

  app::debug_start = world_camera_position;
  app::debug_stop = debug_direction;

  mouse_ray.dir[0] = mouse_ray_direction.x;
  mouse_ray.dir[1] = mouse_ray_direction.y;
  mouse_ray.dir[2] = mouse_ray_direction.z;
  mouse_ray.min_t = app::z_near;
  mouse_ray.max_t = app::z_far;

  nanort::TriangleIntersector<float, nanort::TriangleIntersection<float>>
      triangle_intersector(world_positions.data(), index_buffer->data(),
                           3 * sizeof(float));
  nanort::TriangleIntersection<float> intersection;
  nanort::BVHTraceOptions options;
  options.cull_back_face = false;

  if (accel.Traverse(mouse_ray, triangle_intersector, &intersection, options)) {
    // std::cout << "raycast successful!\n";
    // std::cout << "primitive id is:" << intersection.prim_id << "\n";
    // std::cout << "number of triangles " << nb_triangles << "\n";
    app::active_poly_indices.x =
        float((*index_buffer)[size_t(intersection.prim_id) * 3]);
    app::active_poly_indices.y =
        float((*index_buffer)[size_t(intersection.prim_id) * 3 + 1]);
    app::active_poly_indices.z =
        float((*index_buffer)[size_t(intersection.prim_id) * 3 + 2]);

    glm::vec3 v0 = glm::make_vec3(
        &world_positions[3 * size_t(app::active_poly_indices.x)]);
    glm::vec3 v1 = glm::make_vec3(
        &world_positions[3 * size_t(app::active_poly_indices.y)]);
    glm::vec3 v2 = glm::make_vec3(
        &world_positions[3 * size_t(app::active_poly_indices.z)]);

    glm::vec3 hit = glm::make_vec3(mouse_ray.org) +
                    intersection.t * glm::make_vec3(mouse_ray.dir);

    const float d0 = glm::distance2(hit, v0), d1 = glm::distance2(hit, v1),
                d2 = glm::distance2(hit, v2);

    const float dmin = std::min(d0, std::min(d1, d2));

    int clicked_vertex = 0;
    if (dmin == d0) clicked_vertex = 0;
    if (dmin == d1) clicked_vertex = 1;
    if (dmin == d2) clicked_vertex = 2;

    app::active_vertex_index =
        int((*index_buffer)[size_t(intersection.prim_id) * 3 +
                            size_t(clicked_vertex)]);

    if (skinned) {
      const float* weight_array =
          &weights[submesh][3 * size_t(app::active_vertex_index)];
      float max_weight = weight_array[0];
      size_t index_max = 0;

      for (size_t i = 1; i < 4; ++i) {
        if (max_weight < weight_array[i]) {
          max_weight = weight_array[i];
          index_max = i;
        }
      }

      int most_important_bone =
          joints[submesh][4 * size_t(app::active_vertex_index) + index_max];

      if (max_weight != 0) {
        app::active_joint_index_model = most_important_bone;
      }
    }

    return true;
  }

  return false;
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
  soft_skinned_position = std::move(o.soft_skinned_position);
  soft_skinned_normals = std::move(o.soft_skinned_normals);
  joints = std::move(o.joints);
  colors = std::move(o.colors);

  shader_list = std::move(o.shader_list);
  soft_skin_shader_list = std::move(o.soft_skin_shader_list);

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

      // Since what we are already doing is not ideal to set a light direction
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

void app::async_worker::ui() {
  if (!running) return;

  ImGui::OpenPopup("obj_export");
  if (ImGui::BeginPopup("obj_export")) {
    ImGui::Text("%s", task_name.c_str());
    ImGui::ProgressBar(completion_percent);
    ImGui::EndPopup();
  }
}

void app::async_worker::work_for_one_frame() {
  if (!running) return;

  if (!sequence_to_export) {
    running = false;
    return;
  }

  the_app->playing_state = false;
  the_app->currentFrame = 0;
  the_app->currentPlayTime = 0;

  // compute time
  const auto max_frame = sequence_to_export->GetFrameMax();
  const auto min_frame = sequence_to_export->GetFrameMin();
  if (current_export_frame < min_frame) current_export_frame = min_frame;
  if (current_export_frame > max_frame) {
    running = false;
    sequence_to_export = nullptr;
  }
  completion_percent =
      float(current_export_frame - min_frame) / float(max_frame - min_frame);
  the_app->currentFrame = current_export_frame;
  const float current_animation_time =
      float(double(current_export_frame) / double(ANIMATION_FPS));

  // morph and skin
  for (auto& anim : the_app->animations) {
    anim.set_time(current_animation_time);
    anim.apply_pose();
  }
  update_mesh_skeleton_graph_transforms(the_app->gltf_scene_tree);
  for (auto& mesh : the_app->loaded_meshes) {
    the_app->compute_joint_matrices(the_app->root_node_model_matrix,
                                    mesh.joint_matrices, mesh.flat_joint_list,
                                    mesh.inverse_bind_matrices);
    for (size_t sm = 0; sm < mesh.indices.size(); ++sm) {
      the_app->perform_software_morphing(
          the_app->gltf_scene_tree, sm, mesh.morph_targets, mesh.positions,
          mesh.normals, mesh.display_position, mesh.display_normals, mesh.VBOs,
          false);
      the_app->perform_software_skinning(
          sm, mesh.joint_matrices, mesh.display_position, mesh.display_normals,
          mesh.joints, mesh.weights, mesh.soft_skinned_position,
          mesh.soft_skinned_normals);
    }
  }

  // increment for next call
  current_export_frame++;

  // export morphed skinned mesh
  char frame_number_str_with_leading_zeroes[6] = "";
  sprintf(frame_number_str_with_leading_zeroes, "%05d", current_export_frame);
  std::string file_name =
      "./export_obj/" + std::string(frame_number_str_with_leading_zeroes);
  the_app->write_deformed_meshes_to_obj(file_name);
}

app::async_worker::async_worker(app* a) : current_export_frame(0), the_app(a) {}

void app::async_worker::setup_new_sequence(AnimSequence* s) {
  current_export_frame = 0;
  sequence_to_export = s;
}

void app::async_worker::start_work() { running = true; }

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

static void drop_callabck(GLFWwindow* window, int nums, const char** paths) {
  if (nums > 0) {
    // TODO(LTE): Do we need a lock?
    gltf_insight::app* app =
        reinterpret_cast<gltf_insight::app*>(glfwGetWindowUserPointer(window));

    // Use the first one.
    // TODO(LTE): Search .gltf file from paths.

    app->unload();
    app->set_input_filename(paths[0]);

    std::cout << "D&D filename : " << app->get_input_filename() << "\n";

    try {
      app->load();
    } catch (const std::exception& e) {
      std::cerr << "error occured during loading of "
                << app->get_input_filename() << ": " << e.what() << '\n';
      app->unload();
    }
  }
}

void app::initialize_colorpick_framebuffer() {
  glGenFramebuffers(1, &color_pick_fbo);
  glGenTextures(1, &color_pick_screen_texture);
  glGenTextures(1, &color_pick_depth_buffer);
  glBindFramebuffer(GL_FRAMEBUFFER, color_pick_fbo);
  glBindTexture(GL_TEXTURE_2D, color_pick_screen_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLTFI_BUFFER_SIZE, GLTFI_BUFFER_SIZE,
               0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color_pick_screen_texture, 0);
  glBindTexture(GL_TEXTURE_2D, color_pick_depth_buffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, GLTFI_BUFFER_SIZE,
               GLTFI_BUFFER_SIZE, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                         GL_TEXTURE_2D, color_pick_depth_buffer, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

app::app(int argc, char** argv) {
  parse_command_line(argc, argv);

  initialize_glfw_opengl_window(window);
  // glfwSetWindowUserPointer(window, &gui_parameters);
  glfwSetWindowUserPointer(window, this);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetCursorPosCallback(window, cursor_pos_callback);

  // NOTE: We cannot use lambda function with [this] capture, so pass `this`
  // pointer through glfwSetWindowUserPointer.
  // https://stackoverflow.com/questions/39731561/use-lambda-as-glfwkeyfun
  glfwSetDropCallback(window, drop_callabck);

  initialize_imgui(window);
  (void)ImGui::GetIO();

  // load fallback material.
  // This must be called before loading glTF scene since
  // `setup_fallback_textures` creates GL texture ID for each fallback
  // textures(e.g. white tex).
  // TODO(LTE): Do not create fallback texture and add enabled/disabled flag to
  // each texture.
  setup_fallback_textures();
  load_sensible_default_material(dummy_material);
  logo = load_gltf_insight_icon();
  utility_buffers::init_static_buffers();

  if (!input_filename.empty()) {
    try {
      load();
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
      unload();
    }
  }

  initialize_colorpick_framebuffer();
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

#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#if defined(_WIN32)
#include <direct.h>
#endif

void app::write_deformed_meshes_to_obj(const std::string filename) {
  tinyobj::ObjWriter writer;
  // just one mesh and one shape for now

  // const auto nb_ws = loaded_meshes[0].uvs[0].size() / 2;
  // std::vector<float> ws(nb_ws);
  // std::generate(ws.begin(), ws.end(), [] { return 0; });

  tinyobj::shape_t shape;
  for (size_t mesh_idx = 0; mesh_idx < loaded_meshes.size(); ++mesh_idx) {
    shape.name = loaded_meshes[mesh_idx].name;

    int offset = 0;
    for (size_t submesh_idx = 0;
         submesh_idx < loaded_meshes[mesh_idx].indices.size(); ++submesh_idx) {
      writer.attrib_.vertices =
          loaded_meshes[mesh_idx].soft_skinned_position[submesh_idx];
      writer.attrib_.normals =
          loaded_meshes[mesh_idx].soft_skinned_normals[submesh_idx];
      writer.attrib_.texcoords = loaded_meshes[mesh_idx].uvs[submesh_idx];

      shape.mesh.num_face_vertices.resize(
          loaded_meshes[mesh_idx].indices[submesh_idx].size() / 3);
      std::generate(shape.mesh.num_face_vertices.begin(),
                    shape.mesh.num_face_vertices.end(), [] { return 3; });

      for (size_t i = 0;
           i < loaded_meshes[mesh_idx].indices[submesh_idx].size(); ++i) {
        tinyobj::index_t index;
        index.vertex_index =
            1 + int(loaded_meshes[mesh_idx].indices[submesh_idx][i]) + offset;
        index.normal_index =
            1 + int(loaded_meshes[mesh_idx].indices[submesh_idx][i]) + offset;
        index.texcoord_index =
            1 + int(loaded_meshes[mesh_idx].indices[submesh_idx][i]) + offset;
        shape.mesh.indices.push_back(index);
      }

      offset += int(loaded_meshes[mesh_idx].indices[submesh_idx].size());
    }
    writer.shapes_.push_back(shape);
  }

  const std::string path_to_last_dir =
      filename.substr(0, filename.find_last_of("/\\"));

  int nError = 0;
#if defined(_WIN32)
  nError = _mkdir(path_to_last_dir.c_str());  // can be used on Windows
#else
  mode_t nMode = 0733;  // UNIX style permissions
  nError =
      mkdir(path_to_last_dir.c_str(), nMode);  // can be used on non-Windows
#endif
  if (nError != 0 && errno != EEXIST) {
    (void)nError;
    std::cerr << "We attempted to create directory " << path_to_last_dir
              << " And there's an error that is not EEXIST\n";
  }

  const auto status = writer.SaveTofile(filename);
  if (!writer.Warning().empty())
    std::cout << "Warning :" << writer.Warning() << "\n";

  if (!status) {
    std::cout << "wrote " << filename << ".obj to disk\n";
  } else {
    std::cerr << "ERROR while writing " << filename << "\n";
    std::cerr << writer.error() << "\n";
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

  const std::string filename = "test";
  if (write) {
    write_deformed_meshes_to_obj(filename);
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

void app::draw_mesh(const glm::vec3& world_camera_location, const mesh& mesh,
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

    auto& active_shader_list = (mesh.skinned && do_soft_skinning)
                                   ? *mesh.soft_skin_shader_list
                                   : *mesh.shader_list;

    const auto& active_shader = active_shader_list[shader_to_use];

    material_to_use.set_shader_uniform(active_shader);
    update_uniforms(active_shader_list, editor_light.use_ibl,
                    world_camera_location, editor_light.color,
                    editor_light.get_directional_light_direction(),
                    active_joint_index_model, shader_to_use,
                    projection_matrix * view_matrix * model_matrix,
                    projection_matrix * view_matrix * model_matrix,
                    normal_matrix, mesh.joint_matrices, active_poly_indices);

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

void app::draw_scene_recur(const glm::vec3& world_camera_location,
                           gltf_node& node,
                           std::vector<defered_draw>& alpha_models) {
  for (auto child : node.children)
    draw_scene_recur(world_camera_location, *child, alpha_models);

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
      draw_mesh(world_camera_location, mesh, normal_matrix, node.world_xform);
    } else {
      alpha_models.push_back(node.get_ptr());
    }
  }
}

void app::draw_scene(const glm::vec3& world_camera_location) {
  std::vector<defered_draw> alpha;
  draw_scene_recur(world_camera_location, gltf_scene_tree, alpha);

  if (!alpha.empty()) {
    {
      // sort by distance from camera
      if (alpha.size() > 1)
        std::sort(alpha.begin(), alpha.end(),
                  [&](const defered_draw& a, const defered_draw b) {
                    const glm::vec3 a_pos = world_camera_location -
                                            glm::vec3(a.node->world_xform[3]);
                    const glm::vec3 b_pos = world_camera_location -
                                            glm::vec3(b.node->world_xform[3]);

                    return glm::length2(a_pos) < glm::length2(b_pos);
                  });

      for (const auto& to_draw : alpha) {
        const auto node = to_draw.node;
        const auto& mesh = loaded_meshes[size_t(node->gltf_mesh_id)];
        const glm::mat3 normal_matrix =
            glm::transpose(glm::inverse(node->world_xform));

        draw_mesh(world_camera_location, mesh, normal_matrix,
                  node->world_xform);
      }
    }
  }
}

void app::draw_color_select_map_recur(gltf_node& node) {
  for (auto child : node.children) draw_color_select_map_recur(*child);

  if (node.type == gltf_node::node_type::mesh) {
    auto& mesh = loaded_meshes[size_t(node.gltf_mesh_id)];

    for (size_t i = 0; i < mesh.draw_call_descriptors.size(); ++i) {
      const glm::vec4 id_color = mesh.submesh_selection_ids[i];

      update_uniforms(*mesh.shader_list, editor_light.use_ibl,
                      glm::vec3(0, 0, 0), editor_light.color,
                      editor_light.get_directional_light_direction(),
                      active_joint_index_model, "debug_color",
                      projection_matrix * view_matrix * node.world_xform,
                      projection_matrix * view_matrix * node.world_xform,
                      glm::inverse(glm::transpose(node.world_xform)),
                      mesh.joint_matrices, active_poly_indices);

      const auto& color_shader = (*mesh.shader_list)["debug_color"];
      color_shader.use();
      color_shader.set_uniform(
          "debug_color", glm::vec4(id_color.r, id_color.g, id_color.b, 1));

      perform_draw_call(mesh.draw_call_descriptors[i]);
    }
  }
}

void app::draw_color_select_map() {
  draw_color_select_map_recur(gltf_scene_tree);
}

#if defined(GLTF_INSIGHT_WITH_NATIVEFILEDIALOG)
static bool show_file_dialog(const std::string& title,
                             const std::string& file_filter,
                             std::string* filename)  // selected single filename
{
  (void)title;
  if (!filename) {
    return false;
  }

  nfdchar_t* outPath = nullptr;
  // TODO(LTE): Handle default file path
  nfdresult_t result =
      NFD_OpenDialog(file_filter.c_str(), /* default path */ nullptr, &outPath);

  if (result == NFD_OKAY) {
    (*filename) = std::string(outPath);
    free(outPath);
  } else if (result == NFD_CANCEL) {
    std::cout << "User pressed cancel\n";
  } else {
    std::cerr << "Error: " << NFD_GetError() << "\n";
  }

  return true;
}
#endif

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
#if defined(GLTF_INSIGHT_WITH_NATIVEFILEDIALOG)
      // TODO(LTE): Run into modal mode in ImGui while opening NFD window.
      std::string _filename;
      if (show_file_dialog("Open glTF...", "gltf,glb;vrm", &_filename)) {
        std::cout << "Input filename = " << _filename << "\n";

        unload();
        input_filename = _filename;

        try {
          load();
        } catch (const std::exception& e) {
          std::cerr << "error occured during loading of " << input_filename
                    << ": " << e.what() << '\n';
          unload();
        }
      }
      open_file_dialog = false;
#else
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
#endif
    }

    if (save_file_dialog) {
#if defined(GLTF_INSIGHT_WITH_NATIVEFILEDIALOG)
      if (!asset_loaded) {
        save_file_dialog = false;
      } else {
        // TODO(LTE): Run into modal mode in ImGui while opening NFD window.
        std::string _filename;
        if (show_file_dialog("Save glTF data as ...", "", &_filename)) {
          std::cout << "Save filename = " << _filename << "\n";
          std::cout << "TODO(LTE): Implement glTF save feature\n";
        }
        save_file_dialog = false;
      }
#else
      if (!asset_loaded) {
        save_file_dialog = false;
      } else if (ImGuiFileDialog::Instance()->FileDialog(
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
#endif
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
          ImGui::InputInt("Active joint", &active_joint_index_model, 1, 1);
        ImGui::End();
      }

      active_joint_index_model =
          glm::clamp(active_joint_index_model, 0, loaded_meshes[0].nb_joints);
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
        asset_loaded && (active_joint_index_model >= 0) &&
                (active_joint_index_model <
                 int(loaded_meshes[0].flat_joint_list.size()))
            ? loaded_meshes[0].flat_joint_list[size_t(active_joint_index_model)]
            : nullptr);

    update_mesh_skeleton_graph_transforms(gltf_scene_tree);

    const glm::quat camera_rotation(
        glm::vec3(glm::radians(gui_parameters.rot_pitch),
                  glm::radians(gui_parameters.rot_yaw), 0.f));

    world_camera_position = camera_rotation * camera_position;

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
      ImGui::Checkbox("Draw debug ray", &show_debug_ray);
      if (show_debug_ray) {
        ImGui::InputFloat3("debug_start", glm::value_ptr(debug_start));
        ImGui::InputFloat3("debug_stop", glm::value_ptr(debug_stop));

        auto& program = loaded_meshes[0].shader_list->operator[]("debug_color");
        program.use();
        program.set_uniform("mvp", projection_matrix * view_matrix);
        draw_line(program.get_program(), debug_start, debug_stop,
                  glm::vec4(1, 0, 0, 1), 5);
      }

      int active_bone_gltf_node = -1;
      for (auto& a_mesh : loaded_meshes) {
        if ((active_joint_index_model >= 0) &&
            (active_joint_index_model < int(a_mesh.flat_joint_list.size())))
          active_bone_gltf_node =
              a_mesh.flat_joint_list[size_t(active_joint_index_model)]
                  ->gltf_node_index;

        // Calculate all the needed matrices to render the frame, this
        // includes the "model view projection" that transform the geometry to
        // the screen space, the normal matrix, and the joint matrix array
        // that is used to deform the skin with the bones
        if (a_mesh.skinned)
          compute_joint_matrices(root_node_model_matrix, a_mesh.joint_matrices,
                                 a_mesh.flat_joint_list,
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
              gpu_update_submesh_buffers(submesh, a_mesh.soft_skinned_position,
                                         a_mesh.soft_skinned_normals,
                                         a_mesh.VBOs);
            } else if (gpu_dirty) {
              gpu_update_submesh_buffers(submesh, a_mesh.display_position,
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

    {
      ImGui::Begin("OBJ export animation sequence");
      ImGui::Text("Select an animation sequence, then hit the RUN button");
      static int animation_sequence_item = 0;
      std::vector<std::string> names;
      for (auto& s : sequence.myItems) names.push_back(s.name);
      ImGuiCombo("sequence", &animation_sequence_item, names);
      if (ImGui::Button("RUN")) {
        if (!animations.empty()) {
          // setup the exporter
          obj_export_worker.setup_new_sequence(&sequence);

          // make sure only the selected animation will play
          for (size_t i = 0; i < animations.size(); ++i) {
            animations[i].playing = size_t(animation_sequence_item) == i;
          }

          // Start the work
          obj_export_worker.start_work();
        }
      }
      ImGui::End();

      // We decouple the work from the render loop (or more true : we spread it
      // on multiple frames to keep the
      obj_export_worker.work_for_one_frame();
      obj_export_worker.ui();
    }
  }

  // TODO move that code elsewhere. Need better way to get the "clicked" event
  static bool last_frame_click = true;
  const bool current_frame_click = gui_parameters.button_states[0];
  const bool clicked = !last_frame_click && current_frame_click;
  last_frame_click = current_frame_click;

  // if clicked on anything that is not the GUI elements
  if (clicked && !(ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsOver() ||
                   ImGuizmo::IsUsing())) {
    // std::cout << "click on viewport detected\n";

    {  // TODO refactor extract me this
      glBindFramebuffer(GL_FRAMEBUFFER, color_pick_fbo);
      glClearColor(0, 0, 0, 0);
      glViewport(0, 0, GLTFI_BUFFER_SIZE, GLTFI_BUFFER_SIZE);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      draw_color_select_map();

      // get back to normal render buffer
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, display_w, display_h);
    }

    bool clicked_on_submesh = false;
    size_t mesh_id = 0, submesh_id = 0;

    {  // TODO refactor extract this
      // copy data to CPU memory
      static std::vector<uint32_t> img_data(GLTFI_BUFFER_SIZE *
                                            GLTFI_BUFFER_SIZE);
      glBindTexture(GL_TEXTURE_2D, color_pick_screen_texture);
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    reinterpret_cast<void*>(&img_data[0]));

      // check pixel color to find clicked object
      const float pixel_screen_map_x =
          (512 * float(gui_parameters.last_mouse_x) / float(display_w));
      const float pixel_screen_map_y =
          (512 * float(gui_parameters.last_mouse_y) / float(display_h));
      const size_t pixel_screen_map_x_clamp =
          size_t(glm::clamp(pixel_screen_map_x, 0.f, float(GLTFI_BUFFER_SIZE)));
      const size_t pixel_screen_map_y_clamp =
          size_t(glm::clamp(pixel_screen_map_y, 0.f, float(GLTFI_BUFFER_SIZE)));
      color_identifier id(
          img_data[(GLTFI_BUFFER_SIZE - pixel_screen_map_y_clamp) *
                       GLTFI_BUFFER_SIZE +
                   pixel_screen_map_x_clamp]);
      // std::cout << "color id is : " << int(id.content.RGBA.R) << ":"
      //          << int(id.content.RGBA.B) << ":" << int(id.content.RGBA.G)
      //          << ":" << int(id.content.RGBA.A) << "\n";

      // declare identifiers

      for (mesh_id = 0; mesh_id < loaded_meshes.size(); mesh_id++) {
        for (submesh_id = 0;
             submesh_id < loaded_meshes[mesh_id].submesh_selection_ids.size();
             submesh_id++) {
          if (loaded_meshes[mesh_id].submesh_selection_ids[submesh_id] == id) {
            clicked_on_submesh = true;
            break;
          }
        }
        if (clicked_on_submesh) break;
      }
    }

    // If we know who's been clicked :
    if (clicked_on_submesh) {
      // std::cout << "clicked on " << mesh_id << ":" << submesh_id << "\n";
      const auto& mesh = loaded_meshes[mesh_id];

      auto node = gltf_scene_tree.get_node_with_index(mesh.instance.node);
      if (node) {
        const auto world_xform = node->world_xform;
        // std::cout << mesh.name << std::endl;

        if (mesh.raycast_submesh_camera_mouse(
                world_xform, submesh_id, world_camera_position,
                projection_matrix * view_matrix,
                float(gui_parameters.last_mouse_x) / float(display_w),
                float(gui_parameters.last_mouse_y) / float(display_h))) {
          active_mesh_index = int(mesh_id);
          active_submesh_index = int(submesh_id);
        }
      }
    }
  }

  //{
  //   TODO refactor put this as an hidden debug feature
  //   ImGui::Begin("color picker debug");
  //   static bool show_color_map_render = false;
  //   ImGui::Checkbox("show colormap for picking showed in main buffer",
  //                  &show_color_map_render);
  //   ImGui::Image(ImTextureID(size_t(color_pick_screen_texture)),
  //               ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0),
  //               ImVec4(1, 1, 1, 1), ImVec4(0, 0, 0, 1));
  //   ImGui::End();

  //   if (show_color_map_render) {
  //    glClearColor(0, 0, 0, 0);
  //    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //    glViewport(0, 0, display_w, display_h);
  //    draw_color_select_map();
  //  }
  //}

  // if selection is valid:
  if (active_mesh_index >= 0 && active_mesh_index < int(loaded_meshes.size()))
    if (active_submesh_index >= 0 &&
        active_submesh_index <
            int(loaded_meshes[size_t(active_mesh_index)].positions.size()))
      if (active_vertex_index >= 0 &&
          active_vertex_index < int(loaded_meshes[size_t(active_mesh_index)]
                                        .indices[size_t(active_submesh_index)]
                                        .size())) {
        // Get the mesh
        const auto& mesh = loaded_meshes[size_t(active_mesh_index)];

        // Get the vertex buffer
        const auto& vertex_buffer =
            mesh.skinned
                ? mesh.soft_skinned_position[size_t(active_submesh_index)]
                : mesh.display_position[size_t(active_submesh_index)];

        // Get the world matrix
        const auto node =
            gltf_scene_tree.get_node_with_index(mesh.instance.node);
        const auto& world_xform = node->world_xform;

        // Configure shader
        shader& shader = (*mesh.shader_list)["debug_color"];
        const auto mvp = projection_matrix * view_matrix * world_xform;

        // Draw point
        shader.use();
        shader.set_uniform("mvp", mvp);

        int selection = active_vertex_index;

        if (ImGui::Begin("Selection Info")) {
          ImGui::Text("Active face vertex [%d, %d, %d]",
                      int(active_poly_indices.x), int(active_poly_indices.y),
                      int(active_poly_indices.z));

          ImGui::InputInt("Active Vertex Index", &selection, 1, 100);
          selection =
              glm::clamp<int>(selection, 0, int(vertex_buffer.size()) / 3);
          active_vertex_index = selection;

          glm::vec3 active_vertex_position =
              glm::make_vec3(&vertex_buffer[3 * size_t(active_vertex_index)]);

          glm::vec3 active_model_position =
              glm::make_vec3(&mesh.positions[size_t(active_submesh_index)]
                                            [3 * size_t(active_vertex_index)]);
          draw_point(active_vertex_position, 5, shader.get_program(),
                     glm::vec4(0.75, 0.25, 0, 0.8));

          glm::vec3 active_vertex_normal =
              glm::make_vec3(&mesh.normals[size_t(active_submesh_index)]
                                          [3 * size_t(active_vertex_index)]);

          ImGui::Text("Vertex Coordinates (%f, %f, %f)",
                      active_model_position.x, active_model_position.y,
                      active_model_position.z);
          ImGui::Text("Vertex Normal (%f, %f, %f)", active_vertex_normal.x,
                      active_vertex_normal.y, active_vertex_normal.z);

          // Mesh uv are optional
          if (mesh.uvs.size() > 0 &&
              mesh.uvs[size_t(active_submesh_index)].size() > 0) {
            glm::vec2 active_vertex_uv =
                glm::make_vec2(&mesh.uvs[size_t(active_submesh_index)]
                                        [2 * size_t(active_vertex_index)]);
            ImGui::Text("Vertex UV (%f, %f)", active_vertex_uv.x,
                        active_vertex_uv.y);
          }

          if (mesh.skinned) {
            ImGui::Text("Skinning info");
            glm::vec4 weight, joint;
            weight =
                glm::make_vec4(&mesh.weights[size_t(active_submesh_index)]
                                            [4 * size_t(active_vertex_index)]);
            joint =
                glm::make_vec4(&mesh.joints[size_t(active_submesh_index)]
                                           [4 * size_t(active_vertex_index)]);

            ImGui::Columns(2);
            ImGui::Text("Weight");
            ImGui::NextColumn();
            ImGui::Text("Joint");
            ImGui::NextColumn();
            ImGui::Separator();

            for (glm::vec4::length_type i = 0; i < 4; ++i) {
              ImGui::Text("%f", float(weight[i]));
              ImGui::NextColumn();
              ImGui::Text("%d", int(joint[i]));
              ImGui::NextColumn();
              ImGui::Separator();
            }
            ImGui::Columns();
          }
        }
        ImGui::End();
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
#if 0
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
#else
  using optparse::OptionParser;
  OptionParser parser = OptionParser().description("glTF data insight tool");

  parser.add_option("-d", "--debug")
      .action("store_true")
      .dest("debug")
      .help("Enable debugging");

  parser.add_option("-i", "--input")
      .dest("input")
      .help("Input glTF filename")
      .metavar("FILE");
  parser.add_option("-h", "--help")
      .action("store_true")
      .dest("help")
      .help("Show help");

  optparse::Values options = parser.parse_args(argc, argv);
  std::vector<std::string> args = parser.args();

  if (options.get("help")) {
    parser.print_help();
  }

  debug_output = false;
  if (options.get("debug")) {
    debug_output = true;
  }

  if (options.is_set("input")) {
    input_filename = options["input"];
  } else if (args.size() > 0) {
    input_filename = args[0];
  } else {
    input_filename.clear();
  }

#endif
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

    // TODO handle SRGB colorspace for accurate shading.
    glTexImage2D(GL_TEXTURE_2D, 0,
                 model.images[i].component == 4 ? GL_RGBA : GL_RGB,
                 model.images[i].width, model.images[i].height, 0,
                 model.images[i].component == 4 ? GL_RGBA : GL_RGB,
                 GL_UNSIGNED_BYTE, model.images[i].image.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

void app::gpu_update_submesh_buffers(
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

  // keep state clean
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
    // change, we don't want to re-evaluate the mesh.

    // We are keeping a cache of the weights, and setting a dirty flags if we
    // need to recompute it.

    // To transparently handle the loading of a different file, we resize these
    // arrays here
    if (cached_weights.size() != display_position.size()) {
      cached_weights.resize(display_position.size());
      clean.resize(display_position.size());

      // This sets all the dirty flags to "dirty"
      std::generate(clean.begin(), clean.end(), [] { return false; });
    }

    // If the number of blendshape doesn't match, we are just copying the array
    if (cached_weights[submesh_id].size() !=
        mesh_skeleton_graph.pose.blend_weights.size()) {
      clean[submesh_id] = false;
      cached_weights[submesh_id] = mesh_skeleton_graph.pose.blend_weights;
    }

    // ElseIf the size matches, we are comparing all the elements (using
    // std::vector<> operator==()), if they match, it means that mesh doesn't
    // need to be evaluated
    else if (cached_weights[submesh_id] ==
             mesh_skeleton_graph.pose.blend_weights) {
      clean[submesh_id] = true;
    }

    // Else, In that case, we are updating the cache, and setting the flag dirty
    else {
      clean[submesh_id] = false;
      cached_weights[submesh_id] = mesh_skeleton_graph.pose.blend_weights;
    }

    // If flag is found to be dirty
    if (!clean[submesh_id]) {
      // Blend each vertex between morph targets on the CPU:
      for (size_t vertex = 0; vertex < display_position[submesh_id].size();
           ++vertex) {
        cpu_compute_morphed_display_mesh(
            mesh_skeleton_graph, submesh_id, morph_targets, vertex_coord,
            normals, display_position, display_normal, vertex);
      }

      // If it is necessary to upload the new mesh data to the GPU, do it:
      if (upload_to_gpu)
        gpu_update_submesh_buffers(submesh_id, display_position, display_normal,
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
  // TODO only perform this computation if the joints have moved

  // Fetch the arrays for the current primitive
  const auto& prim_positions = positions[submesh_id];
  const auto& prim_normals = normals[submesh_id];
  const auto& prim_joints = joints[submesh_id];
  const auto& prim_weights = weights[submesh_id];
  const auto vertex_count = prim_joints.size() / 4;

  // We need the data sizes to match for what we do to work
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
    glm::mat4& model_matrix, std::vector<glm::mat4>& joint_matrices,
    std::vector<gltf_node*>& flat_joint_list,
    std::vector<glm::mat4>& inverse_bind_matrices) {
  // This compute the individual joint matrices that are uploaded to the
  // GPU. Detailed explanations about skinning can be found in this tutorial
  // https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_020_Skins.md#vertex-skinning-implementation
  // Please note that the code in the tutorial compute the inverse model
  // matrix for each joint, but this matrix doesn't vary here, so
  // we can put it out of the loop. I borrowed this slight optimization from
  // Sascha Willems's "vulkan-glTF-PBR" code...
  // https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.hpp

  const glm::mat4 inverse_model = glm::inverse(model_matrix);
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

  // for (size_t i = 0; i < _sequence.myItems.size(); ++i) {
  //  _sequence.myItems[i].currently_playing = animations[i].playing;
  //}

  timeline_window(_sequence, _playing_state, need_to_update_pose, _looping,
                  _selectedEntry, _firstFrame, _expanded, currentFrame,
                  _currentPlayTime, &show_timeline, lower_docked_prop_size,
                  lower_docked_max_px_size);

  for (size_t i = 0; i < _sequence.myItems.size(); ++i) {
    animations[i].playing = _sequence.myItems[i].currently_playing;
  }

  // loop the sequencer now: TODO replace that true with a "is looping"
  // boolean
  if (_looping && _currentFrame > _sequence.mFrameMax) {
    _currentFrame = _sequence.mFrameMin;
    _currentPlayTime = double(_currentFrame) / double(ANIMATION_FPS);
  }

  for (auto& anim : _animations) {
    if (!anim.playing) continue;
    anim.set_time(float(_currentPlayTime));  // TODO handle timeline position
    // of animaiton sequence
    // anim.playing = _playing_state;
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
