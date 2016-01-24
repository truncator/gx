#version 330

layout(std140) uniform u_Camera
{
    mat4 u_ViewProjection;
};

uniform vec3 u_Start;
uniform vec3 u_End;

void main()
{
    vec3 positions[2] = vec3[2](u_Start, u_End);
    gl_Position = u_ViewProjection * vec4(positions[gl_VertexID], 1.0);
}
