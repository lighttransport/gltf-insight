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
#pragma once

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

// include glad first
#
#ifndef __EMSCRIPTEN__
#include <glad/glad.h>
#else
#include "GLFW/glfw3.h"
#endif
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif


#include <array>
#include <string>


class shader;

namespace gltf_insight {

struct fallback_textures {
  static GLuint pure_white_texture;
  static GLuint pure_flat_normal_map;
  static GLuint pure_black_texture;
};

void setup_fallback_textures();

/// Type of shader requested
enum class shading_type {
  // Standard PBR shader for glTF.
  pbr_metal_rough,
  // "The other" popular PBR workflow. This is from an extension
  pbr_specular_glossy,
  // No lighting. This is from an extension
  unlit,

  // TODO add shader mode defined by extensions here
};

/// Type of alpha blending requested
enum class alpha_coverage : int { opaque = 0, mask = 1, blend = 2 };

static inline std::string to_string(alpha_coverage c) {
  switch (c) {
    case alpha_coverage::blend:
      return "blend";
    case alpha_coverage::opaque:
      return "opaque";
    case alpha_coverage::mask:
      return "mask";
  }
  return "error";
}

static inline std::string to_string(shading_type s) {
  switch (s) {
    case shading_type::pbr_metal_rough:
      return "PBR metallic roughness";
    case shading_type::pbr_specular_glossy:
      return "PBR specular glossiness";
    case shading_type::unlit:
      return "Unlit";
  }
  return "error";
}

/// The maximum number of texture attachement our shader system can have
static constexpr size_t max_texture_slots = 6;

struct material {
  std::string name = "not_set";
  // Hint about shader to use
  shading_type intended_shader = shading_type::pbr_metal_rough;

  // These are texture attachment in an arbitrary order
  std::array<GLuint, max_texture_slots> texture_slots;
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
  float alpha_cutoff = 0.5;
  bool double_sided = false;

  union shader_input_ {
    shader_input_() : pbr_metal_roughness() {}
    struct {
      glm::vec4 base_color_factor = glm::vec4(1.f, 1.f, 1.f, 1.f);
      // attachement 3
      GLuint base_color_texture;
      // attachement 4
      GLuint metallic_roughness_texture;

      float metallic_factor = 1.f;
      float roughness_factor = 1.f;
    } pbr_metal_roughness;

    struct {
      glm::vec4 diffuse_factor = glm::vec4(1.f, 1.f, 1.f, 1.f);
      // attachement 3
      GLuint diffuse_texture;
      // attachement 4
      GLuint specular_glossiness_texture;

      glm::vec3 specular_factor = glm::vec3(1.f, 1.f, 1.f);
      float glossiness_factor = 1.f;
    } pbr_specular_glossiness;

    struct {
      glm::vec4 base_color_factor = glm::vec4(1.f, 1.f, 1.f, 1.f);
      // attachement 3
      GLuint base_color_texture;
    } unlit;

  } shader_inputs;

  static GLuint brdf_lut;
  static void load_brdf_lut();

  void fill_material_texture_slots();
  void bind_textures() const;
  void set_shader_uniform(const shader& shading_program) const;
};

}  // namespace gltf_insight
