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

#include <algorithm>
#include <map>
#include <string>

#ifndef __EMSCRIPTEN__
#include <glad/glad.h>
#else
#include "GLFW/glfw3.h"
#endif

#include "material.hh"
#include "shader.hh"

// These values are used to define shader varying inputs:
static constexpr auto VBO_count = 7;
static constexpr auto VBO_layout_EBO = VBO_count - 1;

static constexpr auto VBO_layout_position = 0;
static constexpr auto VBO_layout_normal = 1;
static constexpr auto VBO_layout_uv = 2;
static constexpr auto VBO_layout_color = 3;
static constexpr auto VBO_layout_joints = 4;
static constexpr auto VBO_layout_weights = 5;

struct utility_buffers {
  static GLuint point_vbo, line_vbo, point_vao, line_vao, point_ebo, line_ebo;
  static void init_static_buffers();
};

/// Debug output
void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar* message, void* userParam);

/// Draw a point in the current space (model/view/projection matrix set to
/// shader)
void draw_point(const glm::vec3& point, float point_size, GLuint shader,
                const glm::vec4& color);

/// Draw a point as the origin of the current space (model/view/projection
/// matrix set to shader)
void draw_space_origin_point(float point_size, GLuint shader,
                             const glm::vec4& color);
/// Draw the X, Y and Z unit vectors of the current space (model/view/projection
/// matrix set to shader)
void draw_space_base(GLuint shader, const float line_width,
                     const float axis_scale);

/// Draw a line from origin to end points in the current space
/// (model/view/projection matrix set to shader)
void draw_line(GLuint shader, const glm::vec3 origin, const glm::vec3 end,
               const glm::vec4 draw_color, const float line_width);

/// Load all the shaders. Skinning shader needs t
void load_shaders(const size_t nb_joints,
                  std::map<std::string, shader>& shaders);

/// Update all shader's uniforms
void update_uniforms(std::map<std::string, shader>& shaders, bool use_ibl,
                     const glm::vec3& camera_position,
                     const glm::vec3& light_color,
                     const glm::vec3& light_direction, const int active_joint,
                     const std::string& shader_to_use, const glm::mat4& model,
                     const glm::mat4& mvp, const glm::mat3& normal,
                     const std::vector<glm::mat4>& joint_matrices,
                     const glm::vec3& active_vertex);

/// Info needed to actually submit drawcall for a submesh
struct draw_call_submesh_descriptor {
  GLenum draw_mode;
  size_t count;
  GLuint VAO;
};

/// Perform the specified drawcall
void perform_draw_call(
    const draw_call_submesh_descriptor& draw_call_to_perform);
