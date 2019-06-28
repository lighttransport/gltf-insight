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
#include "gl_util.hh"

#include <cstring>
#include <iostream>

void glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity,
                   GLsizei length, const GLchar* message, void* userParam) {
  // ignore non-significant error/warning codes
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

  std::cout << "---------------" << '\n';
  std::cout << "Debug message (" << id << "): " << message << '\n';

  switch (source) {
    case GL_DEBUG_SOURCE_API:
      std::cout << "Source: API";
      break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      std::cout << "Source: Window System";
      break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      std::cout << "Source: Shader Compiler";
      break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      std::cout << "Source: Third Party";
      break;
    case GL_DEBUG_SOURCE_APPLICATION:
      std::cout << "Source: Application";
      break;
    case GL_DEBUG_SOURCE_OTHER:
      std::cout << "Source: Other";
      break;
    default:
      std::cout << "Source: Unknown";
  }
  std::cout << '\n';

  switch (type) {
    case GL_DEBUG_TYPE_ERROR:
      std::cout << "Type: Error";
      break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      std::cout << "Type: Deprecated Behaviour";
      break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      std::cout << "Type: Undefined Behaviour";
      break;
    case GL_DEBUG_TYPE_PORTABILITY:
      std::cout << "Type: Portability";
      break;
    case GL_DEBUG_TYPE_PERFORMANCE:
      std::cout << "Type: Performance";
      break;
    case GL_DEBUG_TYPE_MARKER:
      std::cout << "Type: Marker";
      break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
      std::cout << "Type: Push Group";
      break;
    case GL_DEBUG_TYPE_POP_GROUP:
      std::cout << "Type: Pop Group";
      break;
    case GL_DEBUG_TYPE_OTHER:
      std::cout << "Type: Other";
      break;
    default:
      std::cout << "Type: Unknown";
  }
  std::cout << '\n';

  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      std::cout << "Severity: high";
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      std::cout << "Severity: medium";
      break;
    case GL_DEBUG_SEVERITY_LOW:
      std::cout << "Severity: low";
      break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      std::cout << "Severity: notification";
      break;
    default:
      std::cout << "Severity: Unknown";
  }
  std::cout << "\n\n";
}

void draw_space_origin_point(float point_size, GLuint shader,
                             const glm::vec4& color) {
  glUniform4f(glGetUniformLocation(shader, "debug_color"), color.r, color.g,
              color.b, color.a);
  // set the size
  glPointSize(point_size);

  // Since we're not even drawing a polygon, it's probably simpler to do
  // it with old-style opengl
  glBegin(GL_POINTS);
  glVertex4f(0, 0, 0, 1);
  glEnd();
}

void draw_space_base(GLuint shader, const float line_width,
                     const float axis_scale) {
  glLineWidth(line_width);
  glUniform4f(glGetUniformLocation(shader, "debug_color"), 1, 0, 0, 1);
  glBegin(GL_LINES);
  glVertex4f(0, 0, 0, 1);
  glVertex4f(axis_scale, 0, 0, 1);
  glEnd();

  glUniform4f(glGetUniformLocation(shader, "debug_color"), 0, 1, 0, 1);
  glBegin(GL_LINES);
  glVertex4f(0, 0, 0, 1);
  glVertex4f(0, axis_scale, 0, 1);
  glEnd();

  glUniform4f(glGetUniformLocation(shader, "debug_color"), 0, 0, 1, 1);
  glBegin(GL_LINES);
  glVertex4f(0, 0, 0, 1);
  glVertex4f(0, 0, axis_scale, 1);
  glEnd();
}

void load_shaders(const size_t nb_joints,
                  std::map<std::string, shader>& shaders) {
  //"paste in" the bundled shader code
#include "base_color_map.frag_inc.hh"
#include "draw_debug_color.frag_inc.hh"
#include "emissive_map.frag_inc.hh"
#include "metallic_roughness_map.frag_inc.hh"
#include "no_skinning.vert_inc.hh"
#include "normal_map.frag_inc.hh"
#include "normals.frag_inc.hh"
#include "occlusion_map.frag_inc.hh"
#include "pbr_metallic_roughness.frag_inc.hh"
#include "perturbed_normal.frag_inc.hh"
#include "skinning_template.vert_inc.hh"
#include "unlit.frag_inc.hh"
#include "uv.frag_inc.hh"
#include "vertex_color.frag_inc.hh"
#include "weights.frag_inc.hh"
#include "world_fragment.frag_inc.hh"

  // print some warnings
  if (nb_joints > 180)
    std::cerr << "Warning: This is a lot of joints, model may be unsuited for "
                 "GPU based skinning.\n//TODO investigate dual quaternion "
                 "based skinning\n";

  // TODO put the GLSL code ouside of here, load them from files
  // Main vertex shader, that perform GPU skinning

  const std::string no_skinning_vert_src((char*)no_skinning_vert,
                                         no_skinning_vert_len);

  const std::string skinning_vert_src = [&] {
    // TODO a real shader template system may be useful
    // Write in shader source code the value of `nb_joints`
    std::string skinning_template_vert_src((char*)skinning_template_vert,
                                           skinning_template_vert_len);
    size_t index = skinning_template_vert_src.find("$nb_joints");
    if (index == std::string::npos) {
      std::cerr
          << "The skinned mesh vertex shader doesn't have the $nb_joints "
             "token in it's source code anywhere. We cannot do skinning on "
             "a shader that cannot receive the joint list.\n";
      exit(EXIT_FAILURE);
    }

    skinning_template_vert_src.replace(index, strlen("$nb_joints"),
                                       std::to_string(nb_joints));
    return skinning_template_vert_src;
  }();

  const std::string unlit_frag_src((char*)unlit_frag, unlit_frag_len);
  const std::string draw_debug_color_src((char*)draw_debug_color_frag,
                                         draw_debug_color_frag_len);
  const std::string uv_frag_src((char*)uv_frag, uv_frag_len);
  const std::string normals_frag_src((char*)normals_frag, normals_frag_len);
  const std::string weights_frag_src((char*)weights_frag, weights_frag_len);
  const std::string pbr_metallic_roughness_frag_src(
      (char*)pbr_metallic_roughness_frag, pbr_metallic_roughness_frag_len);
  const std::string normal_map_frag_src((char*)normal_map_frag,
                                        normal_map_frag_len);
  const std::string perturbed_normal_frag_src((char*)perturbed_normal_frag,
                                              perturbed_normal_frag_len);
  const std::string occlusion_map_frag_src((char*)occlusion_map_frag,
                                           occlusion_map_frag_len);
  const std::string emissive_map_frag_src((char*)emissive_map_frag,
                                          emissive_map_frag_len);
  const std::string base_color_map_frag_src((char*)base_color_map_frag,
                                            base_color_map_frag_len);
  const std::string metallic_roughness_map_frag_src(
      (char*)metallic_roughness_map_frag, metallic_roughness_map_frag_len);
  const std::string world_frag_src((char*)world_fragment_frag,
                                   world_fragment_frag_len);
  const std::string vertex_color_frag_src((char*)vertex_color_frag,
                                          vertex_color_frag_len);
  const std::string& vert_src =
      nb_joints != 0 ? skinning_vert_src : no_skinning_vert_src;

  shaders["unlit"] = shader("unlit", vert_src, unlit_frag_src);
  shaders["debug_color"] =
      shader("debug_color", no_skinning_vert_src, draw_debug_color_src);
  shaders["debug_uv"] = shader(
      "debug_uv", nb_joints != 0 ? skinning_vert_src : no_skinning_vert_src,
      uv_frag_src);
  shaders["debug_normals"] =
      shader("debug_normals", vert_src, normals_frag_src);
  shaders["debug_normal_map"] =
      shader("debug_normal_map", vert_src, normal_map_frag_src);
  shaders["debug_metallic_roughness_map"] =
      shader("debug_metallic_roughness_map", vert_src,
             metallic_roughness_map_frag_src);
  shaders["debug_occlusion_map"] =
      shader("debug_occlusion_map", vert_src, occlusion_map_frag_src);
  shaders["debug_emissive_map"] =
      shader("debug_emissive_map", vert_src, emissive_map_frag_src);
  shaders["debug_base_color_map"] =
      shader("debug_base_color_map", vert_src, base_color_map_frag_src);
  shaders["debug_applied_normal_mapping"] = shader(
      "debug_applied_normal_mapping", vert_src, perturbed_normal_frag_src);
  shaders["debug_vertex_color"] =
      shader("debug_vertex_color", vert_src, vertex_color_frag_src);
  shaders["debug_frag_pos"] =
      shader("debug_frag_pos", vert_src, world_frag_src);
  shaders["pbr_metal_rough"] =
      shader("pbr_metal_rough", vert_src, pbr_metallic_roughness_frag_src);
  if (nb_joints > 0)
    shaders["weights"] = shader("weights", vert_src, weights_frag_src);
}

void update_uniforms(std::map<std::string, shader>& shaders, bool use_ibl,
                     const glm::vec3& camera_position,
                     const glm::vec3& light_color,
                     const glm::vec3& light_direction, const int active_joint,
                     const std::string& shader_to_use, const glm::mat4& model,
                     const glm::mat4& mvp, const glm::mat3& normal,
                     const std::vector<glm::mat4>& joint_matrices) {
  shaders[shader_to_use].use();
  shaders[shader_to_use].set_uniform("camera_position", camera_position);
  shaders[shader_to_use].set_uniform("light_direction", light_direction);
  shaders[shader_to_use].set_uniform("light_color", light_color);
  shaders[shader_to_use].set_uniform("active_joint", active_joint);
  shaders[shader_to_use].set_uniform("joint_matrix", joint_matrices);
  shaders[shader_to_use].set_uniform("mvp", mvp);
  shaders[shader_to_use].set_uniform("model", model);
  shaders[shader_to_use].set_uniform("normal", normal);
  shaders[shader_to_use].set_uniform("debug_color",
                                     glm::vec4(0.5f, 0.5f, 0.f, 1.f));
  shaders[shader_to_use].set_uniform("use_ibl",
                                     int(use_ibl ? GL_TRUE : GL_FALSE));
}

void perform_draw_call(const draw_call_submesh& draw_call_to_perform) {
  glBindVertexArray(draw_call_to_perform.VAO);
  glDrawElements(draw_call_to_perform.draw_mode,
                 GLsizei(draw_call_to_perform.count), GL_UNSIGNED_INT, 0);
}
