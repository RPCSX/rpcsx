#version 450

layout (triangles, invocations = 1) in;
layout (triangle_strip, max_vertices = 4) out;

layout (location=0) in vec4 inp[3];
layout (location=0) out vec4 outp;

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

  outp = inp[0];
  gl_Position = topLeft;
  EmitVertex();

  outp = inp[2];
  gl_Position = bottomLeft;
  EmitVertex();

  outp = vec4(inp[1].x, inp[0].y, inp[0].z, inp[0].w);
  gl_Position = topRight;
  EmitVertex();

  outp = vec4(inp[1].x, inp[2].y, inp[0].z, inp[0].w);
  gl_Position = bottomRight;
  EmitVertex();

  EndPrimitive();
}
