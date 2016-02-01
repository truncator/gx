#version 330

layout(std140) uniform u_Camera
{
    mat4 u_ViewProjection;
};

uniform vec2 u_Start;
uniform vec2 u_End;

void main()
{
    vec2 positions[2] = vec2[2](u_Start, u_End);
    gl_Position = u_ViewProjection * vec4(positions[gl_VertexID], 0.0, 1.0);
}
