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

std::string bin_to_str(unsigned char data[], unsigned int len) {
  std::string output;
  output.resize(len);
  memcpy((char*)output.c_str(), (char*)data, len);
  return output;
}

void load_shaders(const size_t nb_joints,
                  std::map<std::string, shader>& shaders) {
  std::cerr << "Generating GLSL code for shader with " << nb_joints
            << " joints.\n";

  if (nb_joints > 180)
    std::cerr << "Warning: This is a lot of joints, model may be unsuited for "
                 "GPU based skinning.\n//TODO investigate dual quaternion "
                 "based skinning\n";

    // TODO put the GLSL code ouside of here, load them from files
    // Main vertex shader, that perform GPU skinning

#include "bitangent.frag_inc.hh"
#include "draw_debug_color.frag_inc.hh"
#include "no_skinning.vert_inc.hh"
#include "normals.frag_inc.hh"
#include "pbr_metallic_roughness.frag_inc.hh"
#include "skinning_template.vert_inc.hh"
#include "tangents.frag_inc.hh"
#include "unlit.frag_inc.hh"
#include "uv.frag_inc.hh"
#include "weights.frag_inc.hh"

  std::string vertex_shader_no_skinning(
      bin_to_str(no_skinning_vert, no_skinning_vert_len));

  // TODO a real shader template system may be useful
  // Write in shader source code the value of `nb_joints`
  std::string vertex_shader_source_skinning_template(
      bin_to_str(skinning_template_vert, skinning_template_vert_len));
  std::string vertex_shader_source_skinning = [&] {
    size_t index = vertex_shader_source_skinning_template.find("$nb_joints");
    if (index == std::string::npos) {
      std::cerr
          << "The skinned mesh vertex shader doesn't have the $nb_joints "
             "token in it's source code anywhere. We cannot do skinning on "
             "a shader that cannot receive the joint list.\n";
      exit(EXIT_FAILURE);
    }

    vertex_shader_source_skinning_template.replace(index, strlen("$nb_joints"),
                                                   std::to_string(nb_joints));
    return vertex_shader_source_skinning_template;
  }();

  std::string fragment_shader_source_textured_unlit =
      bin_to_str(unlit_frag, unlit_frag_len);

  std::string fragment_shader_source_draw_debug_color =
      bin_to_str(draw_debug_color_frag, draw_debug_color_frag_len);
  std::string fragment_shader_source_uv = bin_to_str(uv_frag, uv_frag_len);
  std::string fragment_shader_source_normals =
      bin_to_str(normals_frag, normals_frag_len);
  std::string fragment_shader_source_tangents =
      bin_to_str(tangents_frag, tangents_frag_len);
  std::string fragment_shader_source_bitangent =
      bin_to_str(bitangent_frag, bitangent_frag_len);
  std::string fragment_shader_weights =
      bin_to_str(bitangent_frag, bitangent_frag_len);
  std::string fragment_shader_pbr_metal_rough =
      bin_to_str(pbr_metallic_roughness_frag, pbr_metallic_roughness_frag_len);

  shaders["unlit"] =
      shader("unlit",
             nb_joints != 0 ? vertex_shader_source_skinning.c_str()
                            : vertex_shader_no_skinning.c_str(),
             fragment_shader_source_textured_unlit.c_str());
  shaders["debug_color"] =
      shader("debug_color", vertex_shader_no_skinning.c_str(),
             fragment_shader_source_draw_debug_color.c_str());
  shaders["debug_uv"] =
      shader("debug_uv",
             nb_joints != 0 ? vertex_shader_source_skinning.c_str()
                            : vertex_shader_no_skinning.c_str(),
             fragment_shader_source_uv.c_str());
  shaders["debug_normals"] =
      shader("debug_normals",
             nb_joints != 0 ? vertex_shader_source_skinning.c_str()
                            : vertex_shader_no_skinning.c_str(),
             fragment_shader_source_normals.c_str());

  shaders["debug_tangent"] =
      shader("debug_tangent",
             nb_joints != 0 ? vertex_shader_source_skinning.c_str()
                            : vertex_shader_no_skinning.c_str(),
             fragment_shader_source_tangents.c_str());

  shaders["debug_bitangent"] =
      shader("debug_bitangent",
             nb_joints != 0 ? vertex_shader_source_skinning.c_str()
                            : vertex_shader_no_skinning.c_str(),
             fragment_shader_source_bitangent.c_str());

  shaders["no_skinning_tex"] =
      shader("no_skinning_tex", vertex_shader_no_skinning.c_str(),
             fragment_shader_source_textured_unlit.c_str());

  shaders["pbr_metal_rough"] =
      shader("pbr_metal_rough",
             nb_joints != 0 ? vertex_shader_source_skinning.c_str()
                            : vertex_shader_no_skinning.c_str(),
             fragment_shader_pbr_metal_rough.c_str());

  if (nb_joints > 0)
    shaders["weights"] =
        shader("weights",
               nb_joints != 0 ? vertex_shader_source_skinning.c_str()
                              : vertex_shader_no_skinning.c_str(),
               fragment_shader_weights.c_str());
}

void update_uniforms(std::map<std::string, shader>& shaders,
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
}

void perform_draw_call(const draw_call_submesh& draw_call_to_perform) {
  // glBindTexture(GL_TEXTURE_2D, draw_call_to_perform.main_texture);
  glBindVertexArray(draw_call_to_perform.VAO);
  glDrawElements(draw_call_to_perform.draw_mode,
                 GLsizei(draw_call_to_perform.count), GL_UNSIGNED_INT, 0);
}
