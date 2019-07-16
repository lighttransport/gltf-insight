//#version 330

layout (location = 0) in vec3 input_position;
layout (location = 1) in vec3 input_normal;
layout (location = 2) in vec2 input_uv;
layout (location = 3) in vec4 input_colors;
layout (location = 4) in vec4 input_joints;
layout (location = 5) in vec4 input_weights;

uniform mat4 model;
uniform mat4 mvp;
uniform mat3 normal;
uniform int active_joint;
uniform vec3 active_vertex;

//TODO this array of matrices can represent too much uniform data for some rigging schemes.
//Should replace this with another skinning method (dual quaternion skinning?) to prevent that.
uniform mat4 joint_matrix[$nb_joints];
//uniform mat4 joint_matrix[4];

out vec3 interpolated_normal;
out vec3 fragment_world_position;
out vec4 interpolated_colors;

out vec2 interpolated_uv;
out vec4 interpolated_weights;

out float selected;

vec3 float_to_rgb(float value)
{
 vec3 color = vec3(0.0f, 0.0f, 0.0f);

if(value < 0.00001f) return color;

 value *= 2.0f;

 if(value < 0.5f)
 {
    color.b = 1.0f - (value + 0.5f);
    color.g = value + 0.5f;
 }
 else
 {
    value = (value * 2.0f) - 1.0f;
    color.g = 1.0f - (value + 0.5f);
    color.r = value + 0.5f;
 }

  return color;
}

vec4 weight_color()
{
  vec3 color = vec3(0.0f, 0.0f, 0.0f);

  if(input_joints.x == float(active_joint))
    color += float_to_rgb(input_weights.x);

  if(input_joints.y == float(active_joint))
    color += float_to_rgb(input_weights.y);

  if(input_joints.z == float(active_joint))
    color += float_to_rgb(input_weights.z);

  if(input_joints.w == float(active_joint))
    color += float_to_rgb(input_weights.w);

  return vec4(color, 1.0f);
}

void main()
{
  //compute skinning matrix
  mat4 skin_matrix =
    input_weights.x * joint_matrix[int(input_joints.x)]
  + input_weights.y * joint_matrix[int(input_joints.y)]
  + input_weights.z * joint_matrix[int(input_joints.z)]
  + input_weights.w * joint_matrix[int(input_joints.w)];

  mat3 normal_skin_matrix = mat3(transpose(inverse(skin_matrix)));
  gl_Position = mvp * skin_matrix * vec4(input_position, 1.0f);
  vec3 skinned_normal = normal_skin_matrix * input_normal;

  interpolated_normal = normal * normalize(skinned_normal);
  fragment_world_position = vec3(model * vec4(input_position, 1.0f));

  interpolated_uv = input_uv;
  interpolated_weights = weight_color();
  interpolated_colors = input_colors;

 if(gl_VertexID == int(active_vertex.x)
  || gl_VertexID == int(active_vertex.y)
  || gl_VertexID == int(active_vertex.z))
  {
	selected = 1.f;
  }
  else
  {
   selected = 0.f;
  }
}

