#pragma once

#include <algorithm>
#include <map>
#include <string>

#include "glad/glad.h"
#include "material.hh"
#include "shader.hh"
/// Debug output
void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar* message, void* userParam);

/// Draw a point as the origin of the current space (model/view/projection
/// matrix set to shader)
void draw_space_origin_point(float point_size, GLuint shader,
                             const glm::vec4& color);
/// Draw the X, Y and Z unit vectors of the current space (model/view/projection
/// matrix set to shader)
void draw_space_base(GLuint shader, const float line_width,
                     const float axis_scale);

/// Load all the shaders. Skinning shader needs t
void load_shaders(const size_t nb_joints,
                  std::map<std::string, shader>& shaders);

/// Update all shader's uniforms
void update_uniforms(std::map<std::string, shader>& shaders,
                     const glm::vec3& camera_position,
                     const glm::vec3& light_color,
                     const glm::vec3& light_direction, const int active_joint,
                     const std::string& shader_to_use, const glm::mat4& model,
                     const glm::mat4& mvp, const glm::mat3& normal,
                     const std::vector<glm::mat4>& joint_matrices);

/// Info needed to actually submit drawcall for a submesh
struct draw_call_submesh {
  GLenum draw_mode;
  size_t count;
  GLuint VAO, main_texture;
};

/// Perform the specified drawcall
void perform_draw_call(const draw_call_submesh& draw_call_to_perform);
