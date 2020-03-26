#pragma once

#include "common/mortar.h"
#include "app_shader_shared/shared.glsl.h"

struct CameraProperties
{
	mat4 m_view;
	//
	mat4 m_proj;
	//
	mat4 m_inv_view;
	//
	mat4 m_inv_proj;
	//
	int m_is_moved;
	float padding[3];
};

struct FpsCamera
{
	vec3	m_origin;
	float	m_fov_y;
	vec3	m_direction;
	float	m_aspect_ratio;
	vec3	m_up;
	float	m_move_speed = 0.2f;
	float	m_up_speed = 0.2f;
	float	m_rotate_speed = 4.0f;
	dvec2	m_prev_cursor_pos;
	bool	m_is_moved;

	FpsCamera(const vec3 & origin,
			  const vec3 & lookat,
			  const vec3 & up,
			  const float fov_y,
			  const float aspect_ratio):
		m_origin(origin),
		m_direction(normalize(lookat - origin)),
		m_up(up),
		m_fov_y(fov_y),
		m_aspect_ratio(aspect_ratio),
		m_prev_cursor_pos(dvec2(0, 0)),
		m_is_moved(false)
	{
	}

	void
	update(const float frame_time)
	{
		// handle movement
		float forward = 0.0f;
		float right = 0.0f;
		float up = 0.0f;

		auto & core = Core::Inst();

		forward += glfwGetKey(core.m_glfw_window, GLFW_KEY_W) == GLFW_PRESS ? 1.0f : 0.0f;
		forward += glfwGetKey(core.m_glfw_window, GLFW_KEY_S) == GLFW_PRESS ? -1.0f : 0.0f;
		right += glfwGetKey(core.m_glfw_window, GLFW_KEY_D) == GLFW_PRESS ? 1.0f : 0.0f;
		right += glfwGetKey(core.m_glfw_window, GLFW_KEY_A) == GLFW_PRESS ? -1.0f : 0.0f;
		up += glfwGetKey(core.m_glfw_window, GLFW_KEY_SPACE) == GLFW_PRESS ? 1.0f : 0.0f;
		up += glfwGetKey(core.m_glfw_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ? -1.0f : 0.0f;
		float boost = glfwGetKey(core.m_glfw_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 10.0f : 1.0f;

		move_forward(forward * m_move_speed * frame_time * boost);
		move_right(right * m_move_speed * frame_time * boost);
		flyup(up * m_up_speed * frame_time * boost);

		// handle direction
		dvec2 cursor_pos;
		dvec2 cursor_move(0.0, 0.0);
		glfwGetCursorPos(core.m_glfw_window,
						 &cursor_pos.x,
						 &cursor_pos.y);
		if (glfwGetMouseButton(core.m_glfw_window, GLFW_MOUSE_BUTTON_1))
		{
			cursor_move = cursor_pos - m_prev_cursor_pos;
			rotate(m_rotate_speed  * vec2(cursor_move) / vec2(Extent.width, Extent.height));
		}

		if (forward != 0.0f || right != 0.0f || up != 0.0f || length(cursor_move) != 0.0)
		{
			m_is_moved = true;
		}
		else
		{
			m_is_moved = false;
		}

		m_prev_cursor_pos = cursor_pos;
	}

	void
	move_forward(const float amount)
	{
		m_origin += amount * m_direction;
	}

	void
	move_right(const float amount)
	{
		vec3 side_axis = cross(m_direction, m_up);
		m_origin += side_axis * amount;
	}

	void
	zoom(const float amount)
	{
		m_fov_y = std::pow(m_fov_y, amount + 1.0f);
	}

	void
	flyup(const float amount)
	{
		m_origin += amount * m_up;
	}

	void
	rotate(const vec2 & amount)
	{
		// rotate x
		glm::mat4 rotate_x = glm::rotate(glm::identity<mat4>(), -amount.x, m_up);
		m_direction = vec3(rotate_x * vec4(m_direction, 1.0));

		// rotate y
		vec3 side_axis = cross(m_direction, m_up);
		glm::mat4 rotate_y = glm::rotate(glm::identity<mat4>(), -amount.y, side_axis);
		m_direction = vec3(rotate_y * vec4(m_direction, 1.0));
	}

	CameraProperties
	get_camera_props()
	{
		CameraProperties result;
		result.m_view = glm::lookAt(m_origin, m_origin + m_direction, m_up);
		result.m_proj = glm::perspective(m_fov_y, m_aspect_ratio, 0.01f, 100.0f);
		result.m_proj[1][1] *= -1.0f;
		result.m_inv_view = glm::inverse(result.m_view);
		result.m_inv_proj = glm::inverse(result.m_proj);
		result.m_is_moved = m_is_moved ? 1 : 0;
		return result;
	}
};