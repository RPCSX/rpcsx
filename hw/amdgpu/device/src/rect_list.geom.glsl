#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices = 4) out;

void main(void)
{
  vec4 topLeft = gl_in[0].gl_Position;
  vec4 right = gl_in[1].gl_Position;
  vec4 bottomLeft = gl_in[2].gl_Position;

  vec4 topRight = vec4(
      right.x,
      topLeft.y,
      topLeft.z,
      topLeft.w
  );

  vec4 bottomRight = vec4(
      right.x,
      bottomLeft.y,
      topLeft.z,
      topLeft.w
  );


  gl_Position = topLeft;
  EmitVertex();

  gl_Position = bottomLeft;
  EmitVertex();

  gl_Position = topRight;
  EmitVertex();

  gl_Position = bottomRight;
  EmitVertex();

  EndPrimitive();
}
