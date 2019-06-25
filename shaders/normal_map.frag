#version 130

in vec2 interpolated_uv;
uniform sampler2D normal_texture;
out vec4 output_color;

void main()
{
  vec4 sampled_pixel = texture(normal_texture, interpolated_uv);
  output_color = vec4(sampled_pixel.rgb, 1.0);
}