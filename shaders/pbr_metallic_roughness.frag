#version 130

#define ALPHA_OPAQUE 0
#define ALPHA_MASK 1
#define ALPHA_BLEND 2

out vec4 output_color;

in vec2 interpolated_uv;
in vec3 interpolated_normal;
in vec3 interpolated_tangent;
in vec3 interpolated_bitangent;
in vec3 frag_world_position;
in mat3 tbn;

//texture maps
uniform sampler2D normal_texture;
uniform sampler2D occlusion_texture;
uniform sampler2D emissive_texture;
uniform sampler2D base_color_texture;
uniform sampler2D metallic_roughness_texture;

//TODO BRDF lookup table here
uniform sampler2D brdf_lut;

uniform vec4 base_color_factor;
uniform float metallic_factor;
uniform float roughness_factor;
uniform vec3 emissive_factor;
uniform int alpha_mode;
uniform float alpha_cutoff;

uniform vec3 camera_position;
uniform vec3 light_direction;
uniform vec3 light_color;

//To hold the data during computation
struct pbr_info
{
	float n_dot_l;
	float n_dot_v;
	float n_dot_h;
	float l_dot_h;
	float v_dot_h;
	float preceptual_roughness;
	float metalnes;
	vec3 reflect_0;
	vec3 reflect_90;
	float alpha_roughness;
	vec3 diffuse_color;
	vec3 specular_color;
};

const float PI =  3.141592653589793;
const float min_roughness = 0.04;


vec3 calculate_normal_vector()
{
  vec3 sampled_normal = normalize((texture(normal_texture, interpolated_uv).rgb * 2.0f) - 1.0f);
  return (normalize(tbn * sampled_normal));
}

vec3 diffuse(pbr_info pbr_inputs)
{
	return pbr_inputs.diffuse_color /*/ PI*/;
}

vec3 specular_reflection(pbr_info pbr_inputs)
{
 return pbr_inputs.reflect_0 + (pbr_inputs.reflect_90 - pbr_inputs.reflect_0) 
 * pow(clamp( 1.0 - pbr_inputs.v_dot_h, 0.0, 1.0), 5.0 );
}

float geometric_occlusion(pbr_info pbr_inputs)
{
	//copy input parameters;
	float n_dot_l = pbr_inputs.n_dot_l;
	float n_dot_v = pbr_inputs.n_dot_v;
	float r = pbr_inputs.alpha_roughness;

	//calculate attenuation
	float att_l = 2.0 * n_dot_l / (n_dot_l + sqrt(r*r * (1.0 - r*r) * (n_dot_l * n_dot_l)));
	float att_v = 2.0 * n_dot_v / (n_dot_v + sqrt(r*r * (1.0 - r*r) * (n_dot_v * n_dot_v)));
	return att_l * att_v;
}

float microfaced_distribution(pbr_info pbr_inputs)
{
	float r_sq = pbr_inputs.alpha_roughness * pbr_inputs.alpha_roughness;
	float f = (pbr_inputs.n_dot_h * r_sq - pbr_inputs.n_dot_h) * pbr_inputs.n_dot_h +1.0f;
	return r_sq / (PI * f * f);
}

void main()
{
	//sample the base_color texture. //TODO SRGB color space.
	vec4 base_color = base_color_factor * texture(base_color_texture, interpolated_uv);

	if(alpha_mode == ALPHA_MASK)
	{
		if(base_color.a < alpha_cutoff)
			discard;
	}

	//Get material phycisal properties : how much metallic it is, and the surface
	//roughness
	float metallic;
	float perceptual_roughness;
	vec4 physics_sample = texture(metallic_roughness_texture, interpolated_uv);
	metallic = metallic_factor * physics_sample.b;
	perceptual_roughness = clamp(roughness_factor * physics_sample.g, min_roughness, 1.0);
	float alpha_roughness = perceptual_roughness * perceptual_roughness;


	vec3 f0 = vec3(0.04); //frenel factor
	vec3 diffuse_color = base_color.rgb * (vec3(1.0) - f0); 
	diffuse_color *= 1.0 - metallic;
	vec3 specular_color = mix(f0, base_color.rgb, metallic);

	//surface reflectance
	float reflectance = max(max(specular_color.r, specular_color.g), specular_color.b);

	float reflectance90 = clamp(reflectance * 25, 0.0, 1.0);
	vec3 specular_env_r0 = specular_color.rgb;
	vec3 specular_env_r90 =  vec3(1.0f, 1.0f, 1.0f) * reflectance90;

	//vec3 n = calculate_normal_vector(); //TODO fix my tangent space for normal mapping!
	vec3 n = normalize(interpolated_normal);
	vec3 v = normalize(camera_position - frag_world_position);
	vec3 l = normalize(-light_direction);
	vec3 h = normalize(l+v);
	vec3 reflection = -normalize(reflect(v, n));
	reflection.y *= -1.0f;
	
	float n_dot_l = clamp(dot(n, l), 0.001, 1.0);
	float n_dot_v = clamp(abs(dot(n, v)), 0.001, 1.0);
	float n_dot_h = clamp(dot(n, h), 0.0, 1.0);
	float l_dot_h = clamp(dot(l, h), 0.0, 1.0);
	float v_dot_h = clamp(dot(v, h), 0.0, 1.0);

	pbr_info pbr_inputs = pbr_info
	(
		n_dot_l,
		n_dot_v,
		n_dot_h,
		l_dot_h,
		v_dot_h,
		perceptual_roughness,
		metallic,
		specular_env_r0,
		specular_env_r90,
		alpha_roughness,
		diffuse_color,
		specular_color
	);

	vec3  F = specular_reflection(pbr_inputs);
	float G = geometric_occlusion(pbr_inputs);
	float D = microfaced_distribution(pbr_inputs);

	vec3 diffuse_contribution = (1.0 - F) * diffuse(pbr_inputs);
	vec3 sepcular_contribution = F * G * D / (4.0f * n_dot_l * n_dot_v);
	vec3 color = n_dot_l * light_color * (diffuse_contribution + sepcular_contribution);

	//TODO make these tweakable?
	//Occlusion remove light is small features of the geometry
	const float occlusion_strength = 1.0f;
	float ao = texture(occlusion_texture, interpolated_uv).r;
	color = mix(color, color * ao, occlusion_strength);
	
	//Emissive makes part of the object actually "emit" light
	const float emissive_strength = 1.0f;
	vec3 emissive = (vec4(emissive_factor, emissive_strength) 
		* texture(emissive_texture, interpolated_uv)).rgb;
	color += emissive;

	output_color = vec4(color, base_color.a);
}