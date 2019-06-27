#version 130

in vec2 interpolated_uv;
in vec3 interpolated_normal;
in vec4 interpolated_colors;
out vec4 output_color;

uniform sampler2D base_color_texture;
uniform sampler2D emissive_texture;

uniform vec3 emissive_factor;
uniform vec4 base_color_factor;
uniform int alpha_mode;
uniform float alpha_cutoff;

#define ALPHA_OPAQUE 0
#define ALPHA_MASK 1
#define ALPHA_BLEND 2


void main()
{
  vec4 color = interpolated_colors * base_color_factor * texture(base_color_texture, interpolated_uv);

  if(alpha_mode == ALPHA_MASK || alpha_mode == ALPHA_BLEND)
  {
	if(color.a < alpha_cutoff)
	{
		discard;
	}
  }

  vec4 emissive = vec4(emissive_factor, 1) * texture(emissive_texture, interpolated_uv);

  color = color + emissive;
  output_color = color;
}