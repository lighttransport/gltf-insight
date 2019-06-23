#include "material.hh"

#include "shader.hh"

using namespace gltf_insight;

void gltf_insight::setup_fallback_textures() {
  static constexpr std::array<uint8_t, 16> pure_white_image_2{
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
  };

  static constexpr std::array<uint8_t, 12> pure_flat_normal_2{
      128, 128, 255, 128, 128, 255, 128, 128, 255, 128, 128, 255,
  };

  glGenTextures(1, &pure_white_texture);
  glBindTexture(GL_TEXTURE_2D, pure_white_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pure_white_image_2.data());

  glGenTextures(1, &pure_flat_normal_map);
  glBindTexture(GL_TEXTURE_2D, pure_flat_normal_map);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
               pure_white_image_2.data());
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
      textures_used = 5;
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

void material::bind_textures() {
  for (int i = 0; i < textures_used; ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, texture_slots[0]);
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
  shading_program.set_uniform("alpha_cuttoff", alpha_cuttoff);

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
