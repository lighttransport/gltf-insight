#include "material.hh"

#include "shader.hh"

using namespace gltf_insight;

void material::fill_material_slots() {
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

void material::update_uniforms(shader& shading_program) {
  shading_program.use();
  shading_program.set_uniform("normal_texture", 0);
  shading_program.set_uniform("occlusion_texture", 1);
  shading_program.set_uniform("emissive_texture", 2);
  shading_program.set_uniform("emissive_factor", emissive_factor);
  shading_program.set_uniform("alpha_mode", int(alpha_mode));
  shading_program.set_uniform("alpha_cuttoff", alpha_cuttoff);
}
