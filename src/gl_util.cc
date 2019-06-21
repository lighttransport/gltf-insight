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
  std::cerr << "Generating GLSL code for shader with " << nb_joints
            << " joints.\n";

  if (nb_joints > 180)
    std::cerr << "Warning: This is a lot of joints, model may be unsuited for "
                 "GPU based skinning.\n//TODO investigate dual quaternion "
                 "based skinning\n";

  // TODO put the GLSL code ouside of here, load them from files
  // Main vertex shader, that perform GPU skinning
  std::string vertex_shader_source_str = R"glsl(
#version 130
#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_gpu_shader5 : enable

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec3 input_tangent;
layout (location = 4) in vec4 input_joints;
layout (location = 5) in vec4 input_weights;

uniform mat4 mvp;
uniform mat3 normal;
uniform int active_joint;

//TODO this array of matrices can represent too much uniform data for some rigging schemes.
//Should replace this with another skinning method (dual quaternion skinning?) to prevent that.
uniform mat4 joint_matrix[$nb_joints];

out vec3 interpolated_normal;
out vec3 interpolated_tangent;
out vec3 interpolated_bitangent;

out vec2 interpolated_uv;
out vec4 interpolated_weights;

vec3 float_to_rgb(float value)
{
 vec3 color = vec3(0,0,0);

if(value < 0.00001) return color;

 value *= 2;

 if(value < 0.5)
 {
    color.b = 1.0 - (value + 0.5);
    color.g = value + 0.5;
 }
 else
 {
    value = (value * 2.0) - 1.0;
    color.g = 1.0 - (value + 0.5);
    color.r = value + 0.5;
 }

  return color;
}

vec4 weight_color()
{
  vec3 color = vec3(0,0,0);

  if(input_joints.x == active_joint)
    color += float_to_rgb(input_weights.x);

  if(input_joints.y == active_joint)
    color += float_to_rgb(input_weights.y);

  if(input_joints.z == active_joint)
    color += float_to_rgb(input_weights.z);

  if(input_joints.w == active_joint)
    color += float_to_rgb(input_weights.w);

  return vec4(color, 1);
}

#ifndef GL_ARB_gpu_shader5

mat4 inverse(mat4 m)
{
  float
      a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3],
      a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3],
      a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3],
      a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3],

      b00 = a00 * a11 - a01 * a10,
      b01 = a00 * a12 - a02 * a10,
      b02 = a00 * a13 - a03 * a10,
      b03 = a01 * a12 - a02 * a11,
      b04 = a01 * a13 - a03 * a11,
      b05 = a02 * a13 - a03 * a12,
      b06 = a20 * a31 - a21 * a30,
      b07 = a20 * a32 - a22 * a30,
      b08 = a20 * a33 - a23 * a30,
      b09 = a21 * a32 - a22 * a31,
      b10 = a21 * a33 - a23 * a31,
      b11 = a22 * a33 - a23 * a32,

      det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

  return mat4(
      a11 * b11 - a12 * b10 + a13 * b09,
      a02 * b10 - a01 * b11 - a03 * b09,
      a31 * b05 - a32 * b04 + a33 * b03,
      a22 * b04 - a21 * b05 - a23 * b03,
      a12 * b08 - a10 * b11 - a13 * b07,
      a00 * b11 - a02 * b08 + a03 * b07,
      a32 * b02 - a30 * b05 - a33 * b01,
      a20 * b05 - a22 * b02 + a23 * b01,
      a10 * b10 - a11 * b08 + a13 * b06,
      a01 * b08 - a00 * b10 - a03 * b06,
      a30 * b04 - a31 * b02 + a33 * b00,
      a21 * b02 - a20 * b04 - a23 * b00,
      a11 * b07 - a10 * b09 - a12 * b06,
      a00 * b09 - a01 * b07 + a02 * b06,
      a31 * b01 - a30 * b03 - a32 * b00,
      a20 * b03 - a21 * b01 + a22 * b00) / det;
}
#endif

void main()
{
  //compute skinning matrix
  mat4 skin_matrix =
    input_weights.x * joint_matrix[int(input_joints.x)]
  + input_weights.y * joint_matrix[int(input_joints.y)]
  + input_weights.z * joint_matrix[int(input_joints.z)]
  + input_weights.w * joint_matrix[int(input_joints.w)];

  mat4 normal_skin_matrix = transpose(inverse(skin_matrix));
  gl_Position = mvp * skin_matrix * vec4(input_position, 1.0);

  interpolated_normal = normal * vec3(normal_skin_matrix * vec4(input_normal, 1.0));
  interpolated_tangent = normal *  vec3(normal_skin_matrix * vec4(input_tangent, 1.0));
  interpolated_bitangent = cross(interpolated_normal, interpolated_tangent);
  
  interpolated_uv = input_uv;
  interpolated_weights = weight_color();
}
)glsl";

  const char* vertex_shader_no_skinning = R"glsl(
#version 130
#extension GL_ARB_explicit_attrib_location : require

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec3 input_tangent;
layout (location = 4) in vec4 input_joints;
layout (location = 5) in vec4 input_weights;

uniform mat4 mvp;
uniform mat3 normal;

out vec3 interpolated_normal;
out vec3 interpolated_tangent;
out vec3 interpolated_bitangent;

out vec2 interpolated_uv;
out vec4 interpolated_weights;

void main()
{
  gl_Position = mvp * vec4(input_position, 1.0);

  interpolated_normal = normal * input_normal;
  interpolated_tangent = normal * input_tangent;
  interpolated_bitangent = cross(interpolated_normal, interpolated_tangent);
  
  interpolated_uv = input_uv;
  interpolated_weights = input_weights;
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

  const char* fragment_shader_source_textured_unlit = R"glsl(
#version 130

in vec2 interpolated_uv;
in vec3 interpolated_normal;
out vec4 output_color;
uniform sampler2D diffuse_texture;

void main()
{
  vec4 sampled_color = texture(diffuse_texture, interpolated_uv);

  if(sampled_color.a != 1.0) discard;
  
  output_color = sampled_color;
}
)glsl";

  const char* fragment_shader_source_draw_debug_color = R"glsl(
#version 130

out vec4 output_color;
uniform vec4 debug_color;

void main()
{
  output_color = debug_color;
}
)glsl";

  const char* fragment_shader_source_uv = R"glsl(
#version 130

out vec4 output_color;
in vec2 interpolated_uv;

void main()
{
  output_color = vec4(interpolated_uv, 0, 1);
}
)glsl";

  const char* fragment_shader_source_normals = R"glsl(
#version 130

in vec3 interpolated_normal;
out vec4 output_color;

void main()
{
  output_color = vec4(normalize(interpolated_normal), 1);
}

)glsl";

  const char* fragment_shader_source_tangents = R"glsl(
#version 130

in vec3 interpolated_tangent;
out vec4 output_color;

void main()
{
  output_color = vec4(normalize(interpolated_tangent), 1);
}

)glsl";

  const char* fragment_shader_source_bitangent = R"glsl(
#version 130

in vec3 interpolated_bitangent;
out vec4 output_color;

void main()
{
  output_color = vec4(normalize(interpolated_bitangent), 1);
}

)glsl";

  const char* fragment_shader_weights = R"glsl(
#version 130

in vec4 interpolated_weights;
out vec4 output_color;

void main()
{
  output_color = interpolated_weights;
}

)glsl";

  shaders["textured"] =
      shader("textured",
             nb_joints != 0 ? vertex_shader_source : vertex_shader_no_skinning,
             fragment_shader_source_textured_unlit);
  shaders["debug_color"] = shader("debug_color", vertex_shader_no_skinning,
                                  fragment_shader_source_draw_debug_color);
  shaders["debug_uv"] =
      shader("debug_uv",
             nb_joints != 0 ? vertex_shader_source : vertex_shader_no_skinning,
             fragment_shader_source_uv);
  shaders["debug_normals"] =
      shader("debug_normals",
             nb_joints != 0 ? vertex_shader_source : vertex_shader_no_skinning,
             fragment_shader_source_normals);

  shaders["debug_tangent"] =
      shader("debug_tangent",
             nb_joints != 0 ? vertex_shader_source : vertex_shader_no_skinning,
             fragment_shader_source_tangents);

  shaders["debug_bitangent"] =
      shader("debug_bitangent",
             nb_joints != 0 ? vertex_shader_source : vertex_shader_no_skinning,
             fragment_shader_source_bitangent);

  shaders["no_skinning_tex"] =
      shader("no_skinning_tex", vertex_shader_no_skinning,
             fragment_shader_source_textured_unlit);
  shaders["weights"] =
      shader("weights",
             nb_joints != 0 ? vertex_shader_source : vertex_shader_no_skinning,
             fragment_shader_weights);
}

void update_uniforms(std::map<std::string, shader>& shaders,
                     const int active_joint, const std::string& shader_to_use,
                     const glm::mat4& mvp, const glm::mat3& normal,
                     const std::vector<glm::mat4>& joint_matrices) {
  shaders[shader_to_use].use();
  shaders[shader_to_use].set_uniform("active_joint", active_joint);
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
