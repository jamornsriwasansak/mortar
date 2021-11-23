#include "mainloop.h"

int
main()
{
#ifdef NDEBUG
    constexpr bool is_debug = false;
#else
    constexpr bool is_debug = true;
#endif

    // default parameters
    const size_t num_flights = 2;
    const int2   target_resolution(1920, 1080);

    // create main window
    Window main_window("Mortar", target_resolution);

    // create device
    Rhi::Entry          entry(main_window, is_debug);
    Rhi::PhysicalDevice physical_device = entry.get_graphics_devices()[0];
    Rhi::Device         device("main_device", physical_device);

    // create renderer
    MainLoop main_loop(device, main_window, num_flights);
    main_loop.init();
    main_loop.run();
    return 0;
}
