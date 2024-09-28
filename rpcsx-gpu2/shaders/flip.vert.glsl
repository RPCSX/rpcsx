#version 450

layout(location = 0) out vec2 coord;

void main()
{
  float x = float(((gl_VertexIndex + 2) / 3) & 1) * 2 - 1; 
  float y = float(((gl_VertexIndex + 1) / 3) & 1) * 2 - 1; 

  gl_Position = vec4(x, y, 0, 1);

  coord.x = x < 0 ? 0 : 1;
  coord.y = y < 0 ? 0 : 1;
}
