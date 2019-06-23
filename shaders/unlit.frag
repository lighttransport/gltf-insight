#version 130

in vec2 interpolated_uv;
in vec3 interpolated_normal;
out vec4 output_color;

uniform sampler2D base_color_texture;

void main()
{
  vec4 sampled_color = texture(base_color_texture, interpolated_uv);

  if(sampled_color.a != 1.0) discard;
  
  output_color = sampled_color;
}