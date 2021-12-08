#pragma once

#include "glfwhandler.h"
#include "vmath.h"

struct CameraProperties
{
    float4x4 m_view;
    float4x4 m_proj;
    float4x4 m_vp;
};

struct FpsCamera
{
    float3  m_origin;
    float   m_fov_y;
    float3  m_direction;
    float   m_aspect_ratio_w_div_h;
    float3  m_up;
    float   m_move_speed   = 0.2f;
    float   m_up_speed     = 0.2f;
    float   m_rotate_speed = 4.0f;
    double2 m_prev_cursor_pos;
    bool    m_is_moved;

    FpsCamera() {}

    FpsCamera(const float3 & origin, const float3 & lookat, const float3 & up, const float fov_y, const float aspect_ratio)
    : m_origin(origin),
      m_fov_y(fov_y),
      m_direction(normalize(lookat - origin)),
      m_aspect_ratio_w_div_h(aspect_ratio),
      m_up(up),
      m_prev_cursor_pos(double2(0, 0)),
      m_is_moved(false)
    {
    }

    void
    update(const Window & window, const int2 & resolution, const float frame_time, const bool allow_input)
    {
        // handle movement
        float forward = 0.0f;
        float right   = 0.0f;
        float up      = 0.0f;
        float boost   = 0.0f;

        if (allow_input)
        {
            forward += glfwGetKey(window.m_glfw_window, GLFW_KEY_W) == GLFW_PRESS ? 1.0f : 0.0f;
            forward += glfwGetKey(window.m_glfw_window, GLFW_KEY_S) == GLFW_PRESS ? -1.0f : 0.0f;
            right += glfwGetKey(window.m_glfw_window, GLFW_KEY_D) == GLFW_PRESS ? 1.0f : 0.0f;
            right += glfwGetKey(window.m_glfw_window, GLFW_KEY_A) == GLFW_PRESS ? -1.0f : 0.0f;
            up += glfwGetKey(window.m_glfw_window, GLFW_KEY_SPACE) == GLFW_PRESS ? 1.0f : 0.0f;
            up += glfwGetKey(window.m_glfw_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ? -1.0f : 0.0f;
            boost = glfwGetKey(window.m_glfw_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 10.0f : 1.0f;
        }

        move_forward(forward * m_move_speed * frame_time * boost);
        move_right(right * m_move_speed * frame_time * boost);
        flyup(up * m_up_speed * frame_time * boost);

        // handle direction
        double2 cursor_pos;
        double2 cursor_move(0.0, 0.0);
        glfwGetCursorPos(window.m_glfw_window, &cursor_pos.x, &cursor_pos.y);
        const float2 resolutionf(resolution);
        if (allow_input && glfwGetMouseButton(window.m_glfw_window, GLFW_MOUSE_BUTTON_1))
        {
            cursor_move = cursor_pos - m_prev_cursor_pos;
            rotate(m_rotate_speed * float2(cursor_move) / resolutionf);
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
        m_aspect_ratio_w_div_h    = resolutionf.x / resolutionf.y;
    }

    void
    move_forward(const float amount)
    {
        m_origin += amount * m_direction;
    }

    void
    move_right(const float amount)
    {
        float3 side_axis = cross(m_direction, m_up);
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
        m_direction        = float3(rotate_x * vec4(m_direction, 1.0));

        // rotate y
        float3    side_axis = cross(m_direction, m_up);
        glm::mat4 rotate_y  = glm::rotate(glm::identity<mat4>(), -amount.y, side_axis);
        m_direction         = float3(rotate_y * vec4(m_direction, 1.0));
    }

    template <bool IsRightHanded = true>
    CameraProperties
    get_camera_props()
    {
        CameraProperties result;
        if constexpr (IsRightHanded)
        {
            result.m_view = glm::lookAtRH(m_origin, m_origin + m_direction, m_up);
            result.m_proj = glm::perspectiveRH(m_fov_y, m_aspect_ratio_w_div_h, 0.01f, 100.0f);
        }
        else
        {
            result.m_view = glm::lookAtLH(m_origin, m_origin + m_direction, m_up);
            result.m_proj = glm::perspectiveLH(m_fov_y, m_aspect_ratio_w_div_h, 0.01f, 100.0f);
        }
        // result.m_proj[1][1] *= -1.0f;
        result.m_vp = result.m_proj * result.m_view;
        return result;
    }
};