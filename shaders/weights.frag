
in float selected;
in vec4 interpolated_weights;

out vec4 output_color;

void main()
{
  output_color = interpolated_weights;
  if(selected > 0.999f)
  {
  output_color = mix(output_color, vec4(1.0f, 0.5f, 0.0f, 1.0f), 0.5f);
  }
}
