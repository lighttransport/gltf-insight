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
#include "material.hh"

#include "shader.hh"

using namespace gltf_insight;

GLuint gltf_insight::fallback_textures::pure_white_texture = 0;
GLuint gltf_insight::fallback_textures::pure_black_texture = 0;
GLuint gltf_insight::fallback_textures::pure_flat_normal_map = 0;

void gltf_insight::setup_fallback_textures() {
  static constexpr std::array<uint8_t, 16> pure_white_image_2{
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
  };

  static constexpr std::array<uint8_t, 16> pure_black_image_2{
      0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255,
  };

  static constexpr std::array<uint8_t, 16> pure_flat_normal_2{
      128, 128, 255, 255, 128, 128, 255, 255,
      128, 128, 255, 255, 128, 128, 255, 255};

  glGenTextures(1, &fallback_textures::pure_white_texture);
  glBindTexture(GL_TEXTURE_2D, fallback_textures::pure_white_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pure_white_image_2.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &fallback_textures::pure_black_texture);
  glBindTexture(GL_TEXTURE_2D, fallback_textures::pure_black_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pure_black_image_2.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &fallback_textures::pure_flat_normal_map);
  glBindTexture(GL_TEXTURE_2D, fallback_textures::pure_flat_normal_map);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pure_flat_normal_2.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void material::fill_material_texture_slots() {
  texture_slots[0] = normal_texture;
  texture_slots[1] = occlusion_texture;
  texture_slots[2] = emissive_texture;

  switch (intended_shader) {
    case shading_type::pbr_metal_rough:
      texture_slots[3] = shader_inputs.pbr_metal_roughness.base_color_texture;
      texture_slots[4] =
          shader_inputs.pbr_metal_roughness.metallic_roughness_texture;
      texture_slots[5] = brdf_lut;
      textures_used = 6;
      break;

    case shading_type::pbr_specular_glossy:
      texture_slots[3] = shader_inputs.pbr_specular_glossiness.diffuse_texture;
      texture_slots[4] =
          shader_inputs.pbr_specular_glossiness.specular_glossiness_texture;
      textures_used = 5;
      break;

    case shading_type::unlit:
      texture_slots[3] = shader_inputs.unlit.base_color_texture;
      textures_used = 4;
      break;
  }
}

void material::bind_textures() const {
  for (int i = 0; i < int(textures_used); ++i) {
    glActiveTexture(GLenum(GL_TEXTURE0 + i));
    glBindTexture(GL_TEXTURE_2D, texture_slots[size_t(i)]);
  }
}

void material::set_shader_uniform(const shader& shading_program) const {
  // Bind program to opengl state machine
  shading_program.use();

  // set generic material uniform values
  shading_program.set_uniform("normal_texture", 0);
  shading_program.set_uniform("occlusion_texture", 1);
  shading_program.set_uniform("emissive_texture", 2);
  shading_program.set_uniform("emissive_factor", emissive_factor);
  shading_program.set_uniform("alpha_mode", int(alpha_mode));
  shading_program.set_uniform("alpha_cutoff", alpha_cutoff);

  // set shader specific material uniform values
  switch (intended_shader) {
    case shading_type::pbr_metal_rough:
      shading_program.set_uniform("base_color_texture", 3);
      shading_program.set_uniform("metallic_roughness_texture", 4);
      shading_program.set_uniform(
          "base_color_factor",
          shader_inputs.pbr_metal_roughness.base_color_factor);
      shading_program.set_uniform(
          "metallic_factor", shader_inputs.pbr_metal_roughness.metallic_factor);
      shading_program.set_uniform(
          "roughness_factor",
          shader_inputs.pbr_metal_roughness.roughness_factor);
      break;

    case shading_type::pbr_specular_glossy:
      shading_program.set_uniform("diffuse_texture", 3);
      shading_program.set_uniform("specular_glossiness_texture", 4);
      shading_program.set_uniform(
          "diffuse_factor",
          shader_inputs.pbr_specular_glossiness.diffuse_factor);
      shading_program.set_uniform(
          "specular_factor",
          shader_inputs.pbr_specular_glossiness.specular_factor);
      shading_program.set_uniform(
          "glossiness_factor",
          shader_inputs.pbr_specular_glossiness.glossiness_factor);
      break;

    case shading_type::unlit:
      shading_program.set_uniform("base_color_texture", 3);
      shading_program.set_uniform("base_color_factor",
                                  shader_inputs.unlit.base_color_factor);

      break;
  }
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

GLuint material::brdf_lut = 0;
#include "brdflut.png_inc.hh"
#include "stb_image.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

void material::load_brdf_lut() {
  int x, y, c;
  stbi_uc* brdf_lut_data =
      stbi_load_from_memory(brdflut_png, int(brdflut_png_len), &x, &y, &c, 4);
  glGenTextures(1, &brdf_lut);
  glBindTexture(GL_TEXTURE_2D, brdf_lut);
  glTexImage2D(GL_TEXTURE_2D, 0, c == 4 ? GL_RGBA : GL_RGB, x, y, 0,
               c == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, brdf_lut_data);
  stbi_image_free(brdf_lut_data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}
