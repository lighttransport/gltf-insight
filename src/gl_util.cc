#include "gl_util.hh"

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

void draw_space_origin_point(float point_size) {
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
  // TODO put the GLSL code ouside of here, load them from files
  // Main vertex shader, that perform GPU skinning
  std::string vertex_shader_source_str = R"glsl(
#version 330

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec4 input_joints;
layout (location = 4) in vec4 input_weights;

uniform mat4 mvp;
uniform mat3 normal;

uniform mat4 joint_matrix[$nb_joints];

out vec3 interpolated_normal;
out vec2 interpolated_uv;
out vec4 interpolated_weights;

void main()
{
  //compute skinning matrix
  mat4 skin_matrix =
    input_weights.x * joint_matrix[int(input_joints.x)]
  + input_weights.y * joint_matrix[int(input_joints.y)]
  + input_weights.z * joint_matrix[int(input_joints.z)]
  + input_weights.w * joint_matrix[int(input_joints.w)];

  gl_Position = mvp * skin_matrix * vec4(input_position, 1.0);

  interpolated_normal = normal * input_normal;
  interpolated_uv = input_uv;
  interpolated_weights = input_weights;
}
)glsl";

  const char* vertex_shader_no_skinning = R"glsl(
#version 330

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec4 input_joints;
layout (location = 4) in vec4 input_weights;

uniform mat4 mvp;
uniform mat3 normal;

out vec3 interpolated_normal;
out vec2 interpolated_uv;

void main()
{
  gl_Position = mvp * vec4(input_position, 1.0);

  interpolated_normal = normal * input_normal;
  interpolated_uv = input_uv;
}
)glsl";

  // Write in shader source code the value of `nb_joints`
  {
    size_t index = vertex_shader_source_str.find("$nb_joints");
    if (index == std::string::npos) {
      std::cerr
          << "The skinned mesh vertex shader doesn't have the $nb_joints "
             "token in it's source code anywhere. We cannot do skinning on "
             "a shader that cannot receive the joint list.\n";
      exit(EXIT_FAILURE);
    }

    vertex_shader_source_str.replace(index, strlen("$nb_joints"),
                                     std::to_string(nb_joints));
  }
  const char* vertex_shader_source = vertex_shader_source_str.c_str();

  const char* fragment_shader_source_textured = R"glsl(
#version 330

in vec2 interpolated_uv;
in vec3 interpolated_normal;
out vec4 output_color;
uniform sampler2D diffuse_texture;

void main()
{
  output_color = texture(diffuse_texture, interpolated_uv);
}
)glsl";

  const char* fragment_shader_source_draw_debug_color = R"glsl(
#version 330

out vec4 output_color;
uniform vec4 debug_color;

void main()
{
  output_color = debug_color;
}
)glsl";

  const char* fragment_shader_source_uv = R"glsl(
#version 330

out vec4 output_color;
in vec2 interpolated_uv;

void main()
{
  output_color = vec4(interpolated_uv, 0, 1);
}
)glsl";

  const char* fragment_shader_source_normals = R"glsl(
#version 330

in vec3 interpolated_normal;
out vec4 output_color;

void main()
{
  output_color = vec4(interpolated_normal, 1);
}

)glsl";

  const char* fragment_shader_weights = R"glsl(
#version 330

in vec4 interpolated_weights;
out vec4 output_color;

void main()
{
  output_color = interpolated_weights;
}

)glsl";

  shaders["textured"] =
      shader("textured", vertex_shader_source, fragment_shader_source_textured);
  shaders["debug_color"] = shader("debug_color", vertex_shader_no_skinning,
                                  fragment_shader_source_draw_debug_color);
  shaders["debug_uv"] =
      shader("debug_uv", vertex_shader_source, fragment_shader_source_uv);
  shaders["debug_normals"] = shader("debug_normals", vertex_shader_source,
                                    fragment_shader_source_normals);
  shaders["no_skinning_tex"] =
      shader("no_skinning_tex", vertex_shader_no_skinning,
             fragment_shader_source_textured);
  shaders["weights"] =
      shader("weights", vertex_shader_source, fragment_shader_weights);
}

void update_uniforms(std::map<std::string, shader>& shaders,
                     const std::string& shader_to_use, const glm::mat4& mvp,
                     const glm::mat3& normal,
                     const std::vector<glm::mat4>& joint_matrices) {
  shaders[shader_to_use].use();
  shaders[shader_to_use].set_uniform("joint_matrix", joint_matrices);
  shaders[shader_to_use].set_uniform("mvp", mvp);
  shaders[shader_to_use].set_uniform("normal", normal);
  shaders[shader_to_use].set_uniform("debug_color",
                                     glm::vec4(0.5f, 0.5f, 0.f, 1.f));
}

void perform_draw_call(const draw_call_submesh& draw_call_to_perform) {
  glBindTexture(GL_TEXTURE_2D, draw_call_to_perform.main_texture);
  glBindVertexArray(draw_call_to_perform.VAO);
  glDrawElements(draw_call_to_perform.draw_mode,
                 GLsizei(draw_call_to_perform.count), GL_UNSIGNED_INT, 0);
}
