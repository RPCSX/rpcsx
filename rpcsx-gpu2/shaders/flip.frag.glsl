#version 450

layout(location = 0) in vec2 coord;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 1) uniform sampler samp[];
layout(set = 0, binding = 3) uniform texture2D tex[];

void main()
{
  color = vec4(texture(sampler2D(tex[0], samp[0]), coord.xy).xyz, 1);
}
