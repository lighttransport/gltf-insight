#version 130

#define ALPHA_OPAQUE 0
#define ALPHA_MASK 1
#define ALPHA_BLEND 2

out vec4 output_color;

in vec2 interpolated_uv;
in vec2 interpolated_normal;
in vec2 interpolated_tangent;

//texture maps
uniform sampler2D normal_texture;
uniform sampler2D occlusion_texture;
uniform sampler2D emissive_texture;
uniform sampler2D base_color_texture;
uniform sampler2D metallic_roughness_texture;

//TODO BRDF lookup table here
uniform sampler2D brdf_lut;

uniform vec4 base_color_factor;
uniform vec4 metallic_factor;
uniform vec4 roughness_factor;
uniform vec3 emissive_factor;
uniform int alpha_mode;
uniform float alpha_cutoff;

//To hold the data during computation
struct pbr_info
{
	float n_dot_l;
	float n_dot_v;
	float n_dot_h;
	float l_dot_h;
	float v_dot_h;
	vec3 reflect_0;
	vec3 reflect_90;
	vec3 diffuse_color;
	vec3 specular_color;
};

const float PI =  3.141592653589793;

void main()
{
	//sample the base_color texture. //TODO SRGB color space.
	vec4 base_color = base_color_factor * texture(base_color_texture, interpolated_uv);

	if(alpha_mode == ALPHA_MASK)
	{
		if(base_color.a < alpha_cutoff)
			discard;
	}

	/*
	if(alpha_mode == ALPHA_BLEND)
	{
	}
	*/

}