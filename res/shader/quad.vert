#version 330

layout(std140) uniform u_Camera
{
    mat4 u_ViewProjection;
};

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec3 a_Color;

out vec2 v_UV;
out vec3 v_Color;

void main()
{
    v_UV = a_UV;
    v_Color = a_Color;
    gl_Position = u_ViewProjection * vec4(a_Position, 0.0, 1.0);
}
