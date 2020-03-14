#version 450
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D final;

layout(location = 0) in vec2 uv;

void main()
{
    float gamma = 1. / 2.2;
    fragColor = pow(texture(final, uv).rgba, vec4(gamma));
}
