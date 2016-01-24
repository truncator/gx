#version 330

in vec3 v_Position;
in vec3 v_Normal;
in vec3 v_Color;

out vec4 frag_color;

void main()
{
    vec3 light_position = vec3(0.0, 0.0, 0.0);
    vec3 light_direction = normalize(v_Position - light_position);
    float light_intensity = 0.2;
    float NdotL = clamp(dot(-light_direction, v_Normal), 0.0, 1.0);

    vec3 final_color = max(v_Color * NdotL * light_intensity, vec3(0.02));

    //final_color = v_Color;
    final_color = (v_Normal + 1.0) / 2.0;
    frag_color = vec4(final_color, 1.0);
}
