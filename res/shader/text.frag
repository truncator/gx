#version 330

uniform sampler2D u_Texture;

in vec2 v_UV;
in vec3 v_Color;

out vec4 frag_color;

void main()
{
    float alpha = texture(u_Texture, v_UV).r;
    frag_color = vec4(v_Color, alpha);
}
