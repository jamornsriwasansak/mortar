#version 450 core

layout(location = 0) in vec3 vert_color;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform
UniformBufferObject
{
	mat4 m_model;
	mat4 m_view;
	mat4 m_proj;
} ubo;

layout(set = 0, binding = 2) uniform sampler2D tex_sampler;

void main()
{
	//out_color = vec4(texcoord, 0.0, 1.0);
	//out_color = texture(tex_sampler, texcoord);
	out_color = vec4(1.0, 0.0, 0.0, 1.0);
}