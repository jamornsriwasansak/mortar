#pragma once

struct SceneResource;
struct FpsCamera;

struct RenderParams
{
    int2 m_resolution                = { 0, 0 };
    SceneResource * m_scene_resource = nullptr;
    FpsCamera * m_fps_camera         = nullptr;
    bool m_is_static_mesh_dirty      = true;
    bool m_is_shaders_dirty          = false;
    bool m_should_imgui_drawn        = true;
};
