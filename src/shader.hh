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
