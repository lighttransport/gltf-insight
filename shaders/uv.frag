#version 330

out vec4 output_color;
in vec2 interpolated_uv;

void main()
{
  output_color = vec4(interpolated_uv, 0, 1);
}