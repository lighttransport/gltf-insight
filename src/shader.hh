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

#include <glad/glad.h>

#include <string>
#include <vector>

#include "glm/glm.hpp"

class shader {
  GLuint program_;
  std::string shader_name_;

 public:
  // default ctor
  shader();
  // delegate ctor
  shader(const char* shader_name, const std::string& vertex_shader_source_code,
         const std::string& fragment_shader_source_code)
      : shader(shader_name, vertex_shader_source_code.c_str(),
               fragment_shader_source_code.c_str()) {}
  // actual ctor
  shader(const char* shader_name, const char* vertex_shader_source_code,
         const char* fragment_shader_source_code);
  ~shader();
  shader(shader&& other);
  shader& operator=(shader&& other);
  shader& operator=(const shader&) = delete;
  shader(const shader&) = delete;

  void use() const;
  GLuint get_program() const;
  const char* get_name() const;

  void set_uniform(const char* name, const float value) const;
  void set_uniform(const char* name, const int value) const;
  void set_uniform(const char* name, const glm::vec4& v) const;
  void set_uniform(const char* name, const glm::vec3& v) const;
  void set_uniform(const char* name, const glm::mat4& m) const;
  void set_uniform(const char* name, const glm::mat3& m) const;
  void set_uniform(const char* name,
                   const std::vector<glm::mat4>& matrices) const;
  void set_uniform(const char* name, size_t number_of_matrices,
                   float* data) const;
};
