#pragma once

struct RenderParams
{
    int2                          m_resolution = { 0, 0 };
    AssetPool *                   m_asset_pool = nullptr;
    Scene *                       m_scene      = nullptr;
    FpsCamera *                   m_fps_camera = nullptr;
    std::vector<StandardObject> * m_static_objects;
    bool                          m_is_static_mesh_dirty = true;
    bool                          m_is_shaders_dirty     = false;
    bool                          m_should_imgui_drawn   = true;
};
