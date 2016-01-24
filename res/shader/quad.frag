#version 330

in vec3 v_Color;

out vec4 frag_color;

void main()
{
    frag_color = vec4(v_Color, 1.0);
}
