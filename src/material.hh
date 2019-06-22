#pragma once

// include glad first
#include <glad/glad.h>

#include <array>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>

class shader;

namespace gltf_insight {

/// Type of shader requested
enum class shading_type {
  // Standard PBR shader for glTF.
  pbr_metal_rough,
  // "The other" popular PBR workflow. This is from an extension
  pbr_specular_glossy,
  // No lighting. This is from an extension
  unlit,
};

/// Type of alpha blending requested
enum class alpha_coverage { opaque, mask, blend };

/// The maximum number of texture attachement our shader system can have
static constexpr size_t max_texture_slots = 5;

struct material {
  std::string name;
  // Hint about shader to use
  shading_type intended_shader;

  // These are texture attachment in an arbitrary order
  std::array<GLuint, max_texture_slots> texture_slots;
  std::array<bool, max_texture_slots> slot_used = {false};
  // Number of textures to attach
  size_t textures_used = 0;

  // attachement 0
  GLuint normal_texture;
  // attachement 1
  GLuint occlusion_texture;
  // attachement 2
  GLuint emissive_texture;

  glm::vec3 emissive_factor = glm::vec3(0.f, 0.f, 0.f);
  alpha_coverage alpha_mode = alpha_coverage::opaque;
  float alpha_cuttoff = 0.5;
  bool double_sided = false;

  union {
    struct {
      glm::vec4 base_color_factor = glm::vec4(1.f, 1.f, 1.f, 1.f);
      // attachement 3
      GLuint base_color_texture;
      // attachement 4
      GLuint metallic_roughness_texture;

      float metallic_factor = 1;
      float roughness_factor = 1;
    } pbr_metal_roughness;

    struct {
      glm::vec4 diffuse_factor = glm::vec4(1.f, 1.f, 1.f, 1.f);
      // attachement 3
      GLuint diffuse_texture;
      // attachement 4
      GLuint specular_glossiness_texture;

      glm::vec3 specular_factor = glm::vec3(1.f, 1.f, 1.f);
      float glossiness_factor = 1;
    } pbr_specular_glossiness;

    struct {
      glm::vec4 base_color_factor = glm::vec4(1.f, 1.f, 1.f, 1.f);
      // attachement 3
      GLuint base_color_texture;
    } unlit;

  } shader_inputs;

  void fill_material_slots();
  void bind_textures();
  void update_uniforms(shader& shading_program);
};

}  // namespace gltf_insight
