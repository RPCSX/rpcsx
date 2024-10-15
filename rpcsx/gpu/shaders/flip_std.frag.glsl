#version 450

layout(location = 0) in vec2 coord;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform texture2D tex;
layout(binding = 1) uniform sampler samp;

void main()
{
  color = vec4(texture(sampler2D(tex, samp), coord.xy).xyz, 1).rgba;
}
