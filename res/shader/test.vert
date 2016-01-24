#version 330

layout(std140) uniform u_Camera
{
    mat4 u_ViewProjection;
};

uniform mat4 u_Model;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;

out vec3 v_Position;
out vec3 v_Normal;
out vec3 v_Color;

void main()
{
    v_Position = vec3(u_Model * vec4(a_Position, 1.0));
    v_Normal = normalize(mat3(u_Model) * a_Normal);
    v_Color = a_Color;

    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
