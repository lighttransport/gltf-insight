#pragma once

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "glad/include/glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/matrix.hpp"

class shader {
  GLuint program_;
  std::string shader_name_;

 public:
  shader(shader&& other) throw();
  shader& operator=(shader&& other) throw();
  shader(const shader&) = delete;
  shader& operator=(const shader&) = delete;
  shader();
  shader(const char* shader_name, const char* vertex_shader_source_code,
         const char* fragment_shader_source_code);
  ~shader();

  void use() const;
  GLuint get_program() const;
  const char* get_name() const;

  void set_uniform(const char* name, const glm::vec4& v) const;
  void set_uniform(const char* name, const glm::mat4& m) const;
  void set_uniform(const char* name, const glm::mat3& m) const;
  void set_uniform(const char* name,
                   const std::vector<glm::mat4>& matrices) const;
  void set_uniform(const char* name, size_t number_of_matrices,
                   float* data) const;
};
