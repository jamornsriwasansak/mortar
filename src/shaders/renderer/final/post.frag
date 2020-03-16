#version 450
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D final;

layout(location = 0) in vec2 uv;

void main()
{
    float gamma = 1.0f / 2.2f;
    vec4 final_tex_color = texture(final, uv).rgba;
    if (any(isnan(final_tex_color)))
    {
		fragColor = vec4(0.0f, 1.0, 0.0f, 1.0f);
    }
    else
    {
		fragColor = pow(final_tex_color, vec4(gamma));
    }
}
