#version 130

in vec3 interpolated_tangent;
out vec4 output_color;

void main()
{
  output_color = vec4(normalize(interpolated_tangent), 1);
}