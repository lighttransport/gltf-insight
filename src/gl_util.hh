#pragma once

#include <algorithm>
#include <map>
#include <string>

#include "glad/include/glad/glad.h"
#include "shader.hpp"

void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar *message, void *userParam);

void draw_space_origin_point(float point_size);

void draw_space_base(GLuint shader, const float line_width,
                     const float axis_scale);

void load_shaders(const size_t nb_joints,
                  std::map<std::string, shader> &shaders);
