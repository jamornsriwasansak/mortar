#version 450

layout(set = 0, binding = 0) uniform
UniformBufferObject
{
	mat4 m_model;
	mat4 m_view;
	mat4 m_proj;
} ubo;

layout(set = 0, binding = 1) uniform UniformBufferObject3
{
	mat4 m_model;
	mat4 m_view;
	mat4 m_proj;
} ubo3;

layout(set = 1, binding = 0) uniform
UniformBufferObject2
{
	mat4 m_model;
	mat4 m_view;
	mat4 m_proj;
} ubo2;

layout(location = 0) in vec4 in_position_and_u;
layout(location = 1) in vec4 in_normal_and_v;

layout(location = 0) out vec3 vert_color;
layout(location = 1) out vec2 texcoord;

void main()
{
	gl_Position = ubo3.m_proj * ubo3.m_view * ubo3.m_model * vec4(in_position_and_u.xyz, 1.0f);
	texcoord = vec2(in_position_and_u.w, in_normal_and_v.w);
	vert_color = vec3(1.0f, 0.0f, 0.0f);
}
