#version 140
#extension GL_ARB_explicit_attrib_location : require

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec4 input_colors;
layout (location = 3) in vec3 input_tangent;
layout (location = 4) in vec4 input_joints;
layout (location = 5) in vec4 input_weights;

uniform mat4 model;
uniform mat4 mvp;
uniform mat3 normal;
uniform int active_joint;

//TODO this array of matrices can represent too much uniform data for some rigging schemes.
//Should replace this with another skinning method (dual quaternion skinning?) to prevent that.
uniform mat4 joint_matrix[$nb_joints];

out vec3 interpolated_normal;
out vec3 fragment_world_position;
out vec2 interpolated_uv;
out vec4 interpolated_weights;
out vec4 interpolated_colors;

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

/*
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
*/

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
  fragment_world_position = vec3(model * vec4(input_position, 1.0));

  interpolated_uv = input_uv;
  interpolated_weights = weight_color();
  interpolated_colors = input_colors;
}

