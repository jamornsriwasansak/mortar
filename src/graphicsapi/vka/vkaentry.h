#pragma once
#include "vkacommon.h"

namespace Vka
{
struct PhysicalDevice
{
    vk::SurfaceKHR            m_vk_surface;
    vk::PhysicalDevice        m_vk_pdevice;
    vk::Instance              m_vk_instance;
    uint32_t                  m_vk_api_made_version;
    bool                      m_enable_debug      = false;
    std::vector<const char *> m_device_extensions = {
        // required by swapchain
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    std::vector<const char *> m_device_layers = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    PhysicalDevice() {}

    PhysicalDevice(const vk::PhysicalDevice & physical_device,
                   const vk::Instance         instance,
                   const vk::SurfaceKHR       surface,
                   const uint32_t             vk_api_made_version,
                   bool                       enable_debug)
    : m_vk_pdevice(physical_device),
      m_vk_instance(instance),
      m_vk_surface(surface),
      m_vk_api_made_version(vk_api_made_version),
      m_enable_debug(enable_debug)
    {
    }
};

struct Entry
{
    uint32_t                  m_vk_api_version;
    uint32_t                  m_vk_api_made_version;
    bool                      m_debug;
    vk::UniqueInstance        m_vk_instance;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    vk::SurfaceKHR            m_vk_surface;

    // TODO:: move this to somewhere else
    static constexpr uint32_t DefaultApiVersion = 120;

    Entry() {}

    Entry(const Window & window, const bool debug) : m_debug(debug)
    {
        std::vector<const char *> glfw_extensions = GlfwHandler::Inst().query_glfw_extensions();
        std::vector<const char *> instance_extensions;
        std::vector<const char *> instance_layers;

        // request validation layer
        if (debug)
        {
            instance_layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        // request debug marker
        if (debug)
        {
            instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        instance_extensions.insert(instance_extensions.end(),
                                   glfw_extensions.begin(),
                                   glfw_extensions.end());

        // dynamic vulkan loader
        vk::DynamicLoader dl;
        vkGetInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        // make sure validation layer and debug extension are available
        check_instance_extensions_support(instance_extensions);
        check_instance_layers_support(instance_layers);

        // vk version
        m_vk_api_version      = DefaultApiVersion;
        m_vk_api_made_version = VK_MAKE_VERSION(digit<uint32_t>(m_vk_api_version, 2),
                                                digit<uint32_t>(m_vk_api_version, 1),
                                                digit<uint32_t>(m_vk_api_version, 0));

        assert(m_vk_api_version > 0);
        assert(m_vk_api_made_version > 0);

        m_vk_instance = create_instance("vka_engine_name", "vka_app_name", instance_extensions, instance_layers);
        if (!m_vk_instance)
        {
            Logger::Error<true>(__FUNCTION__, " ", "cannot create vk instance");
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_vk_instance.get());

        // create surface
        VkSurfaceKHR vksurface;
        VkResult err = glfwCreateWindowSurface(m_vk_instance.get(), window.m_glfw_window, nullptr, &vksurface);
        if (err)
        {
            Logger::Critical<true>(__FUNCTION__, " creating surface failed ", vk::to_string(vk::Result(err)));
        }
        Logger::Info(__FUNCTION__, " created surface");
        m_vk_surface = vk::SurfaceKHR(vksurface);
    }

    std::vector<PhysicalDevice>
    get_graphics_devices()
    {
        std::vector<PhysicalDevice> results;

        Logger::Info(__FUNCTION__ " search for suitable physical device");
        const std::vector<vk::PhysicalDevice> devices = m_vk_instance->enumeratePhysicalDevices();

        for (const auto & device : devices)
        {
            Logger::Info(__FUNCTION__ " found " + std::string(device.getProperties().deviceName.data()));

            // check if device pass api version
            uint32_t api_version = device.getProperties().apiVersion;
            if (m_vk_api_made_version <= api_version)
            {
                results.emplace_back(device, *m_vk_instance, m_vk_surface, m_vk_api_made_version, m_debug);
            }
        }

        // sort gpu by ranks
        std::map<vk::PhysicalDeviceType, int> ranks = { { vk::PhysicalDeviceType::eDiscreteGpu, 1000 },
                                                        { vk::PhysicalDeviceType::eIntegratedGpu, 100 },
                                                        { vk::PhysicalDeviceType::eVirtualGpu, 10 },
                                                        { vk::PhysicalDeviceType::eCpu, 1 } };
        auto get_gpu_rank                           = [&](const vk::PhysicalDeviceType type) {
            return (ranks.count(type) == 0) ? 0 : ranks[type];
        };
        std::sort(results.begin(), results.end(), [&](const PhysicalDevice & lhs, const PhysicalDevice & rhs) {
            const auto l_prop = lhs.m_vk_pdevice.getProperties();
            const auto r_prop = rhs.m_vk_pdevice.getProperties();
            if (get_gpu_rank(l_prop.deviceType) > get_gpu_rank(r_prop.deviceType)) return true;
            if (get_gpu_rank(l_prop.deviceType) < get_gpu_rank(r_prop.deviceType)) return false;
            if (l_prop.apiVersion > r_prop.apiVersion) return true;
            if (l_prop.apiVersion < r_prop.apiVersion) return false;
            if (l_prop.deviceID >= r_prop.deviceID) return true;
            return false;
        });

        // return result
        return results;
    }

private:
    void
    check_instance_layers_support(const std::vector<const char *> layers)
    {
        // at first no layer is found
        std::vector<std::string> not_found_layer_names(layers.begin(), layers.end());

        // query all layer properties
        const auto layer_props = vk::enumerateInstanceLayerProperties();

        // slowly remove a layer name from the not_found_layer_names if found
        for (const std::string & required_layer_name : layers)
            for (const vk::LayerProperties & layer_prop : layer_props)
                if (std::strcmp(layer_prop.layerName.data(), required_layer_name.data()) == 0)
                {
                    const auto remove_elements =
                        std::remove(not_found_layer_names.begin(), not_found_layer_names.end(), required_layer_name);
                    not_found_layer_names.erase(remove_elements, not_found_layer_names.end());
                }

        // if not found, print the names of layers and throw error
        if (not_found_layer_names.size() > 0)
        {
            for (const std::string & not_found_layer_name : not_found_layer_names)
            {
                Logger::Critical<true>(__FUNCTION__ " couldn't find required instance layer " + not_found_layer_name);
            }
        }
    }

    void
    check_instance_extensions_support(const std::vector<const char *> extensions)
    {
        // at first no extension is found
        std::vector<std::string> not_found_extension_names(extensions.begin(), extensions.end());

        // query all extension props
        const auto extension_props = vk::enumerateInstanceExtensionProperties();

        // slowly remove a layer name from the not_found_extension_names if found
        for (const std::string & required_extension_name : extensions)
            for (const vk::ExtensionProperties & extension_prop : extension_props)
                if (strcmp(extension_prop.extensionName.data(), required_extension_name.data()) == 0)
                {
                    const auto remove_elements = std::remove(not_found_extension_names.begin(),
                                                             not_found_extension_names.end(),
                                                             required_extension_name);
                    not_found_extension_names.erase(remove_elements, not_found_extension_names.end());
                }

        // if not found, print the names of extensions and throw error
        if (not_found_extension_names.size() > 0)
        {
            for (const std::string & not_found_extension_name : not_found_extension_names)
            {
                Logger::Critical<true>(__FUNCTION__ " couldn't find required extension layer : " +
                                       not_found_extension_name);
            }
        }
    }

    vk::UniqueInstance
    create_instance(const std::string &               engine_name,
                    const std::string &               application_name,
                    const std::vector<const char *> & instance_extensions,
                    const std::vector<const char *> & instance_layers)
    {
        Logger::Info(__FUNCTION__ " creating instance");

        // application information
        vk::ApplicationInfo app_info = {};
        {
            app_info.setApiVersion(m_vk_api_made_version);
            app_info.setApplicationVersion(m_vk_api_made_version);
            app_info.setEngineVersion(m_vk_api_made_version);
            app_info.setPApplicationName(application_name.c_str());
            app_info.setPEngineName(engine_name.c_str());
        }

        // information for creating vk instance
        vk::InstanceCreateInfo instance_ci = {};
        {
            instance_ci.setPApplicationInfo(&app_info);
            instance_ci.setEnabledExtensionCount(static_cast<uint32_t>(instance_extensions.size()));
            instance_ci.setPpEnabledExtensionNames(instance_extensions.data());
            instance_ci.setEnabledLayerCount(static_cast<uint32_t>(instance_layers.size()));
            instance_ci.setPpEnabledLayerNames(instance_layers.data());
        }

        // create instance
        vk::UniqueInstance instance = vk::createInstanceUnique(instance_ci, nullptr);
        Logger::Info(__FUNCTION__ " created instance");

        return instance;
    }
};
} // namespace Vka