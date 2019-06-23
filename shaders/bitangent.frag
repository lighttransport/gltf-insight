#version 130

in vec3 interpolated_bitangent;
out vec4 output_color;

void main()
{
  output_color = vec4(normalize(interpolated_bitangent), 1);
} 