#version 330

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec4 input_colors;
layout (location = 4) in vec4 input_joints;
layout (location = 5) in vec4 input_weights;

uniform mat4 model;
uniform mat4 mvp;
uniform mat3 normal;

out vec3 interpolated_normal;
out vec3 fragment_world_position;
out vec4 interpolated_colors;

out vec2 interpolated_uv;
out vec4 interpolated_weights;

void main()
{
  gl_Position = mvp * vec4(input_position, 1.0);

  interpolated_normal = normal * input_normal;
  fragment_world_position = vec3(model * vec4(input_position, 1.0));
  
  interpolated_uv = input_uv;
  interpolated_weights = input_weights;
  interpolated_colors = input_colors;
}