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
  shader(shader&& other) { *this = std::move(other); }

  shader& operator=(shader&& other) {
    program_ = other.program_;
    shader_name_ = std::move(other.shader_name_);
    other.program_ = 0;
    return *this;
  }

  shader(const shader&) = delete;
  shader& operator=(const shader&) = delete;

  ~shader() {
    if (glIsProgram(program_) == GL_TRUE) glDeleteProgram(program_);
  }

  shader() {}
  shader(const char* shader_name, const char* vertex_shader_source_code,
         const char* fragment_shader_source_code)
      : shader_name_(shader_name) {
    std::cout << "Creating " << shader_name << "\n";
    // Create GL objects
    GLint vertex_shader, fragment_shader;
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    program_ = glCreateProgram();

    // Load source code
    glShaderSource(
        vertex_shader, 1,
        static_cast<const GLchar* const*>(&vertex_shader_source_code), nullptr);

    glShaderSource(
        fragment_shader, 1,
        static_cast<const GLchar* const*>(&fragment_shader_source_code),
        nullptr);

    // Compile shader
    GLint success = 0;
    char info_log[512];

    glCompileShader(vertex_shader);
    glCompileShader(fragment_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(vertex_shader, sizeof info_log, nullptr, info_log);
      std::cout << info_log << '\n';
      throw std::runtime_error("Cannot buld vertex shader : " +
                               std::string(info_log));
    }
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(fragment_shader, sizeof info_log, nullptr, info_log);
      std::cout << info_log << '\n';
    }

    // Link shader
    glAttachShader(program_, vertex_shader);
    glAttachShader(program_, fragment_shader);
    glLinkProgram(program_);

    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(program_, sizeof info_log, nullptr, info_log);
      std::cout << info_log << "\n";
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
  }

  void use() const { glUseProgram(program_); }
  const char* get_name() const { return shader_name_.c_str(); }

  bool set_uniform(const char* name, const glm::vec4& v) const {
	  if (name == nullptr) {
		  return false;
	  }
	  
	GLint idx  = glGetUniformLocation(program_, name);

	if (idx < 0) {
		return false;
	}

	glUniform4f(idx, v.x, v.y, v.z, v.w);
}

  bool set_uniform(const char* name, const glm::mat4& m) const {
    if (name == nullptr) {
		return false;
	}
	
	GLint idx = glGetUniformLocation(program_, name);
	
	if (idx < 0) {
		return false;
	}

	glUniformMatrix4fv(idx, 1, GL_FALSE,
		glm::value_ptr(m));
  }

  bool set_uniform(const char* name, const glm::mat3& m) const {
	if (name == nullptr) {
		return false;
	}

	GLint idx = glGetUniformLocation(program_, name);
	  
	if (idx < 0) {
		return false;
	}

	glUniformMatrix3fv(idx, 1, GL_FALSE,
                    glm::value_ptr(m));

  }

  bool set_uniform(const char* name,
                   const std::vector<glm::mat4>& matrices) const {

    if (name == nullptr) {
		return false;
	}

    if (matrices.size() > 0) {
		return false;
	}

	GLint idx = glGetUniformLocation(program_, name);

	if (idx < 0) {
		return false;
	}

	glUniformMatrix4fv(idx, 
		               GLsizei(matrices.size()), GL_FALSE,
                       glm::value_ptr(matrices[0]));

	return true;
  }

  bool set_uniform(const char* name, size_t number_of_matrices,
                   float* data) const {
    if ((name == nullptr) || (data == nullptr) || (number_of_matrices < 1)) {
      return false;
	}

    GLint idx = glGetUniformLocation(program_, name);

	if (idx < 0) {
		return false;
	}

	glUniformMatrix4fv(idx,
		GLsizei(number_of_matrices), GL_FALSE, data);

	return true;
  }

  GLuint get_program() const { return program_; }
};
