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
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // default parameters
    const size_t num_flights = 2;
    const int2   target_resolution(1920, 1080);

    // create main window
    Window main_window("Mortar", target_resolution);

    // create device and swapchain
    Rhi::Entry          entry(main_window, is_debug);
    Rhi::PhysicalDevice physical_device = entry.get_graphics_devices()[0];
    Rhi::Device         device("main_device", physical_device);
    Rhi::Swapchain      swapchain("main_swapchain", device, main_window);

    // misc
    GuiEventCoordinator gui_event_coordinator;
    ShaderBinaryManager shader_binary_manager("shadercache");

    // create renderer
    MainLoop main_loop(device, main_window, num_flights, swapchain, shader_binary_manager, gui_event_coordinator);
    main_loop.run();

    return 0;
}