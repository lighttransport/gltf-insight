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

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif
#include <cstring>
#include <iostream>

#include "gl_util.hh"

GLuint utility_buffers::point_vbo = 0;
GLuint utility_buffers::line_vbo = 0;
GLuint utility_buffers::point_vao = 0;
GLuint utility_buffers::line_vao = 0;
GLuint utility_buffers::point_ebo = 0;
GLuint utility_buffers::line_ebo = 0;

void utility_buffers::init_static_buffers() {
  float buffer[6] = {0.f};
  unsigned int ebo[2] = {0, 1};

  // Generate objects
  glGenVertexArrays(1, &point_vao);
  glGenVertexArrays(1, &line_vao);
  glGenBuffers(1, &point_vbo);
  glGenBuffers(1, &line_vbo);
  glGenBuffers(1, &point_ebo);
  glGenBuffers(1, &line_ebo);

  // Setup point VAO
  glBindVertexArray(point_vao);
  glBindBuffer(GL_ARRAY_BUFFER, point_vbo);
  glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float), buffer, GL_STREAM_DRAW);
  glVertexAttribPointer(VBO_layout_position, 3, GL_FLOAT, GL_FALSE,
                        3 * sizeof(float), nullptr);
  glEnableVertexAttribArray(VBO_layout_position);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, point_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 1 * sizeof(unsigned int), ebo,
               GL_STATIC_DRAW);

  // Setup Line VAO
  glBindVertexArray(line_vao);
  glBindBuffer(GL_ARRAY_BUFFER, line_vbo);
  glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), buffer, GL_STREAM_DRAW);
  glVertexAttribPointer(VBO_layout_position, 3, GL_FLOAT, GL_FALSE,
                        3 * sizeof(float), nullptr);
  glEnableVertexAttribArray(VBO_layout_position);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, point_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2 * sizeof(unsigned int), ebo,
               GL_STATIC_DRAW);
  glBindVertexArray(0);
}

void glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity,
                   GLsizei length, const GLchar* message, void* userParam) {
  (void)length;
  (void)userParam;

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

#ifndef __EMSCRIPTEN__  // TODO add a symbol for GLES 3.0

  // strange observation: This function doesn't exist in GLES 3.0...
  // set the size
  glPointSize(point_size);

#endif

  glUseProgram(shader);
  glUniform4f(glGetUniformLocation(shader, "debug_color"), color.r, color.g,
              color.b, color.a);

  glPointSize(point_size);

  static constexpr std::array<float, 3> vertex = {{0, 0, 0}};
  glBindBuffer(GL_ARRAY_BUFFER, utility_buffers::point_vbo);
  glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(float), vertex.data(),
               GL_STREAM_DRAW);
  glBindVertexArray(utility_buffers::point_vao);
  glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, nullptr);
}

void draw_line(GLuint shader, const glm::vec3 origin, const glm::vec3 end,
               const glm::vec4 draw_color, const float line_width) {
  glUseProgram(shader);
  glLineWidth(line_width);
  glUniform4f(glGetUniformLocation(shader, "debug_color"), draw_color.r,
              draw_color.g, draw_color.b, draw_color.a);

  static std::array<float, 6> buffer;
  buffer[0] = origin.x;
  buffer[1] = origin.y;
  buffer[2] = origin.z;
  buffer[3] = end.x;
  buffer[4] = end.y;
  buffer[5] = end.z;

  glBindBuffer(GL_ARRAY_BUFFER, utility_buffers::line_vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float), buffer.data(),
               GL_STREAM_DRAW);
  glBindVertexArray(utility_buffers::line_vao);
  glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, nullptr);
}

void draw_space_base(GLuint shader, const float line_width,
                     const float axis_scale) {
  draw_line(shader, glm::vec3(0.f), glm::vec3(axis_scale, 0, 0),
            glm::vec4(1, 0, 0, 1), line_width);
  draw_line(shader, glm::vec3(0.f), glm::vec3(0, axis_scale, 0),
            glm::vec4(0, 1, 0, 1), line_width);
  draw_line(shader, glm::vec3(0.f), glm::vec3(0, 0, axis_scale),
            glm::vec4(0, 0, 1, 1), line_width);
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

  const std::string no_skinning_vert_src(
      reinterpret_cast<char*>(no_skinning_vert), no_skinning_vert_len);

  const std::string skinning_vert_src = [&] {
    // TODO a real shader template system may be useful
    // Write in shader source code the value of `nb_joints`
    std::string skinning_template_vert_src(
        reinterpret_cast<char*>(skinning_template_vert),
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

  const std::string unlit_frag_src(reinterpret_cast<char*>(unlit_frag),
                                   unlit_frag_len);
  const std::string draw_debug_color_src(
      reinterpret_cast<char*>(draw_debug_color_frag),
      draw_debug_color_frag_len);
  const std::string uv_frag_src(reinterpret_cast<char*>(uv_frag), uv_frag_len);
  const std::string normals_frag_src(reinterpret_cast<char*>(normals_frag),
                                     normals_frag_len);
  const std::string weights_frag_src(reinterpret_cast<char*>(weights_frag),
                                     weights_frag_len);
  const std::string pbr_metallic_roughness_frag_src(
      reinterpret_cast<char*>(pbr_metallic_roughness_frag),
      pbr_metallic_roughness_frag_len);
  const std::string normal_map_frag_src(
      reinterpret_cast<char*>(normal_map_frag), normal_map_frag_len);
  const std::string perturbed_normal_frag_src(
      reinterpret_cast<char*>(perturbed_normal_frag),
      perturbed_normal_frag_len);
  const std::string occlusion_map_frag_src(
      reinterpret_cast<char*>(occlusion_map_frag), occlusion_map_frag_len);
  const std::string emissive_map_frag_src(
      reinterpret_cast<char*>(emissive_map_frag), emissive_map_frag_len);
  const std::string base_color_map_frag_src(
      reinterpret_cast<char*>(base_color_map_frag), base_color_map_frag_len);
  const std::string metallic_roughness_map_frag_src(
      reinterpret_cast<char*>(metallic_roughness_map_frag),
      metallic_roughness_map_frag_len);
  const std::string world_frag_src(reinterpret_cast<char*>(world_fragment_frag),
                                   world_fragment_frag_len);
  const std::string vertex_color_frag_src(
      reinterpret_cast<char*>(vertex_color_frag), vertex_color_frag_len);
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
  shaders["weights"] = shader("weights", vert_src, weights_frag_src);
}

void update_uniforms(std::map<std::string, shader>& shaders, bool use_ibl,
                     const glm::vec3& camera_position,
                     const glm::vec3& light_color,
                     const glm::vec3& light_direction, const int active_joint,
                     const std::string& shader_to_use, const glm::mat4& model,
                     const glm::mat4& mvp, const glm::mat3& normal,
                     const std::vector<glm::mat4>& joint_matrices,
                     const glm::vec3& active_vertex) {
  shaders[shader_to_use].use();
  shaders[shader_to_use].set_uniform("active_vertex", active_vertex);
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

void perform_draw_call(
    const draw_call_submesh_descriptor& draw_call_to_perform) {
  glBindVertexArray(draw_call_to_perform.VAO);
  glDrawElements(draw_call_to_perform.draw_mode,
                 GLsizei(draw_call_to_perform.count), GL_UNSIGNED_INT, nullptr);
}
