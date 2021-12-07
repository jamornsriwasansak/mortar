#include "mainloop.h"

int
main()
{
#ifdef NDEBUG
    constexpr bool is_debug = false;
#else
    constexpr bool is_debug = true;
#endif

    // setup dear imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO & io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.Fonts->AddFontFromFileTTF("resources/fonts/Roboto/Roboto-Medium.ttf", 15);

    // default parameters
    const size_t num_flights = 2;
    const int2   target_resolution(1920, 1080);

    // create main window
    Window main_window("Mortar", target_resolution);

    // create device
    Rhi::Entry          entry(main_window, is_debug);
    Rhi::PhysicalDevice physical_device = entry.get_graphics_devices()[0];
    Rhi::Device         device("main_device", physical_device);
    Rhi::Swapchain      swapchain("main_swapchain", device, main_window);

    // create renderer
    ShaderBinaryManager shader_binary_manager("shadercache");
    MainLoop main_loop(device, main_window, num_flights, shader_binary_manager, swapchain);
    main_loop.run();

    return 0;
}