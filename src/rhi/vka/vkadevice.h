#pragma once

//#define USE_VULKAN_AFTER_CRASH
#ifdef USE_VULKAN_AFTER_CRASH
#include "VulkanAfterCrash.h"
#endif

#include "vkacommon.h"
#include "vkaentry.h"

#include "common/uniquehandle.h"

namespace VKA_NAME
{
struct Device
{
    struct FeaturesAvailable
    {
        bool m_support_raytracing   = false;
        bool m_support_mesh_shader  = false;
        bool m_support_debug_marker = false;
    };

    // device
    vk::PhysicalDeviceProperties m_vk_device_properties;
    vk::PhysicalDevice           m_vk_pdevice; // physical device
    vk::UniqueDevice             m_vk_ldevice; // logical device
    vk::Instance                 m_vk_instance;
    vk::SurfaceKHR               m_vk_surface;

    // device queue and its command pool
    vk::Queue                m_vk_graphics_queue;
    vk::Queue                m_vk_present_queue;
    vk::Queue                m_vk_compute_queue;
    vk::Queue                m_vk_transfer_queue;
    vk::UniqueCommandPool    m_vk_graphics_command_pool;
    vk::UniqueDescriptorPool m_vk_descriptor_pool;
    struct QueueFamilyIndices
    {
        uint32_t m_graphics = std::numeric_limits<uint32_t>::max();
        uint32_t m_compute  = std::numeric_limits<uint32_t>::max();
        uint32_t m_transfer = std::numeric_limits<uint32_t>::max();
        uint32_t m_present  = std::numeric_limits<uint32_t>::max();
    };
    QueueFamilyIndices m_family_indices;
    FeaturesAvailable  m_features;

    struct VmaAllocatorDeleter
    {
        void
        operator()(VmaAllocator vma_allocator)
        {
            vmaDestroyAllocator(vma_allocator);
        }
    };
    UniquePtrHandle<VmaAllocator, VmaAllocatorDeleter> m_vma_allocator;
#ifdef USE_VULKAN_AFTER_CRASH
    struct VkAfterCrashDeviceDeleter
    {
        void
        operator()(VkAfterCrash_Device device)
        {
            VkAfterCrash_DestroyDevice(device);
        }
    };
    struct VkAfterCrashBufferDeleter
    {
        void
        operator()(VkAfterCrash_Buffer buffer)
        {
            VkAfterCrash_DestroyBuffer(buffer);
        }
    };
    UniquePtrHandle<VkAfterCrash_Device, VkAfterCrashDeviceDeleter> m_vkac_device;
    UniquePtrHandle<VkAfterCrash_Buffer, VkAfterCrashBufferDeleter> m_vkac_buffer;
#endif
    uint32_t * m_vkac_crash_data = nullptr;

    Device() {}

    Device(const PhysicalDevice & physical_device) : m_debug(physical_device.m_enable_debug)
    {
        m_vk_instance = physical_device.m_vk_instance;
        m_vk_pdevice  = physical_device.m_vk_pdevice;
        m_vk_surface  = physical_device.m_vk_surface;
        m_features    = query_features();

        if (m_debug)
        {
            m_features.m_support_debug_marker = false;
        }

        // find queues
        m_family_indices = explore_queue_family(m_vk_pdevice, physical_device.m_vk_surface);
        const auto queue_family_indices = { m_family_indices.m_graphics,
                                            m_family_indices.m_present,
                                            m_family_indices.m_compute,
                                            m_family_indices.m_transfer };

        // create logical device
        m_vk_ldevice = create_device(m_vk_pdevice,
                                     physical_device.m_device_layers,
                                     physical_device.m_device_extensions,
                                     m_features,
                                     queue_family_indices);

        // init default dispatcher
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_vk_ldevice.get());

        // get queues from the logical device
        m_vk_graphics_queue = m_vk_ldevice->getQueue(m_family_indices.m_graphics, 0);
        m_vk_present_queue  = m_vk_ldevice->getQueue(m_family_indices.m_present, 0);
        m_vk_compute_queue  = m_vk_ldevice->getQueue(m_family_indices.m_compute, 0);
        m_vk_transfer_queue = m_vk_ldevice->getQueue(m_family_indices.m_transfer, 0);
        Logger::Info(__FUNCTION__, " get all necessary queues");

        // create command pool for one time submit
        m_vk_graphics_command_pool = create_command_pool(m_vk_ldevice.get(), m_family_indices.m_graphics);

        // allocate vma
        VmaAllocatorCreateInfo vma_allocator_ci = {};
        vma_allocator_ci.physicalDevice         = m_vk_pdevice;
        vma_allocator_ci.device                 = *m_vk_ldevice;
        vma_allocator_ci.instance               = physical_device.m_vk_instance;
        vma_allocator_ci.vulkanApiVersion       = physical_device.m_vk_api_made_version;
        vma_allocator_ci.flags = VmaAllocatorCreateFlagBits::VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        vmaCreateAllocator(&vma_allocator_ci, &m_vma_allocator.get());
        Logger::Info(__FUNCTION__, " create vma allocator");

#ifdef USE_VULKAN_AFTER_CRASH
        VkAfterCrash_DeviceCreateInfo vkac_device_ci;
        vkac_device_ci.vkPhysicalDevice = m_vk_pdevice;
        vkac_device_ci.vkDevice         = m_vk_ldevice.get();
        VKCK(VkAfterCrash_CreateDevice(&vkac_device_ci, &m_vkac_device.get()));

        VkAfterCrash_BufferCreateInfo vkac_buffer_ci;
        vkac_buffer_ci.markerCount = 1000;
        VKCK(VkAfterCrash_CreateBuffer(m_vkac_device.get(), &vkac_buffer_ci, &m_vkac_buffer.get(), &m_vkac_crash_data));
#endif
    }

    FeaturesAvailable
    query_features() const
    {
        // less compare for std set of extension_names
        struct cstrless_compare
        {
            bool
            operator()(const char * a, const char * b) const
            {
                return std::strcmp(a, b) < 0;
            }
        };
        std::set<const char *, cstrless_compare> extension_names;

        // push all props into extension names
        auto extension_props = m_vk_pdevice.enumerateDeviceExtensionProperties();
        for (auto & extension_prop : extension_props)
        {
            extension_names.insert(extension_prop.extensionName);
        }

        FeaturesAvailable result;

        // check if ray tracing is available
        if (extension_names.count(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 1 &&
            extension_names.count(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME) == 1 &&
            extension_names.count(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 1 &&
            extension_names.count(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 1 &&
            extension_names.count(VK_KHR_MAINTENANCE3_EXTENSION_NAME) == 1 &&
            extension_names.count(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 1)
        {
            result.m_support_raytracing = true;
        }

        if (extension_names.count(VK_NV_MESH_SHADER_EXTENSION_NAME) == 1)
        {
            result.m_support_mesh_shader = true;
        }

        if (extension_names.count(VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 1)
        {
            result.m_support_debug_marker = true;
        }

        return result;
    }

    inline bool
    enable_debug() const
    {
#ifdef NDEBUG
        return false;
#else
        return m_debug;
#endif
    }

    template <typename HppType, typename CType>
    void
    name_vkhpp_object(const HppType & vk_object, const std::string & name = "") const
    {
        CType base_vk_object = vk_object;
        if (!name.empty())
        {
            vk::DebugMarkerObjectNameInfoEXT name_info = {};
            name_info.setObjectType(vk_object.debugReportObjectType);
            name_info.setObject(reinterpret_cast<uint64_t>(base_vk_object));
            name_info.setPObjectName(name.c_str());
            m_vk_ldevice->debugMarkerSetObjectNameEXT(name_info);
        }
    }

    void
    one_time_command_submit(const std::function<void(vk::CommandBuffer)> & func) const
    {
        // create a temp command buffer
        vk::CommandBufferAllocateInfo alloc_info = {};
        {
            alloc_info.setLevel(vk::CommandBufferLevel::ePrimary);
            alloc_info.setCommandPool(*m_vk_graphics_command_pool);
            alloc_info.setCommandBufferCount(1);
        }
        std::vector<vk::UniqueCommandBuffer> command_buffers =
            m_vk_ldevice->allocateCommandBuffersUnique(alloc_info);
        vk::CommandBuffer & command_buffer = command_buffers[0].get();

        // tell driver that we gonna submit but only once
        vk::CommandBufferBeginInfo command_buffer_bi;
        {
            command_buffer_bi.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        }

        // record command
        command_buffer.begin(command_buffer_bi);
        func(command_buffer);
        command_buffer.end();

        // create submission
        vk::SubmitInfo submit_info = {};
        {
            submit_info.setCommandBufferCount(1);
            submit_info.setPCommandBuffers(&command_buffer);
        }

        // submit
        m_vk_graphics_queue.submit({ submit_info }, nullptr);
        m_vk_graphics_queue.waitIdle();
    }

private:
    bool m_debug = false;

    vk::UniqueDevice
    create_device(const vk::PhysicalDevice &      physical_device,
                  const std::vector<const char *> layers,
                  const std::vector<const char *> extensions,
                  const FeaturesAvailable &       features,
                  const std::vector<uint32_t>     family_indices_)
    {
        Logger::Info(__FUNCTION__ " creating logical device");
        // make sure all indices in the provided family_indices is unique
        std::set<uint32_t> family_indices_set_tmp(family_indices_.begin(), family_indices_.end());
        std::vector<uint32_t> family_indices(family_indices_set_tmp.begin(), family_indices_set_tmp.end());

        // create info for all queues
        const float                            queue_priority = 1.0f;
        std::vector<vk::DeviceQueueCreateInfo> queue_cis;
        for (const uint32_t family_index : family_indices)
        {
            vk::DeviceQueueCreateInfo queue_ci;
            queue_ci.setQueueFamilyIndex(family_index);
            queue_ci.setQueueCount(1);
            queue_ci.setPQueuePriorities(&queue_priority);
            queue_cis.push_back(queue_ci);
        }

        // push necessary extensions
        std::vector<const char *> all_extensions(extensions.begin(), extensions.end());
        if (features.m_support_raytracing)
        {
            all_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            all_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            all_extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            all_extensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
            all_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        }
        if (features.m_support_mesh_shader)
        {
            all_extensions.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
        }
        if (m_debug)
        {
            all_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        }

        // specify device features
        vk::PhysicalDeviceFeatures device_features = {};
        device_features.setSamplerAnisotropy(VK_TRUE);
        device_features.setShaderInt64(VK_TRUE);

        vk::PhysicalDeviceAccelerationStructureFeaturesKHR feature_raytracing_accel = {};
        feature_raytracing_accel.setAccelerationStructure(VK_TRUE);

        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR feature_raytracing_pipeline = {};
        feature_raytracing_pipeline.setRayTracingPipeline(VK_TRUE);
        feature_raytracing_pipeline.setRayTracingPipelineTraceRaysIndirect(VK_TRUE);
        feature_raytracing_pipeline.setRayTraversalPrimitiveCulling(VK_TRUE);
        feature_raytracing_pipeline.setPNext(&feature_raytracing_accel);

        vk::PhysicalDeviceVulkan12Features device_vulkan12_features = {};
        device_vulkan12_features.setBufferDeviceAddress(VK_TRUE);
        device_vulkan12_features.setStorageBuffer8BitAccess(VK_TRUE);
        device_vulkan12_features.setShaderStorageBufferArrayNonUniformIndexing(VK_TRUE);
        device_vulkan12_features.setRuntimeDescriptorArray(VK_TRUE);
        device_vulkan12_features.setPNext(&feature_raytracing_pipeline);
        device_vulkan12_features.setUniformAndStorageBuffer8BitAccess(VK_TRUE);

        vk::PhysicalDeviceFeatures2 device_features2 = {};
        device_features2.setFeatures(device_features);
        device_features2.setPNext(&device_vulkan12_features);

        vk::DeviceCreateInfo device_ci = {};
        device_ci.setQueueCreateInfos(queue_cis);
        device_ci.setPEnabledExtensionNames(all_extensions);
        device_ci.setPEnabledLayerNames(layers);
        device_ci.setPNext(&device_features2);

        vk::UniqueDevice device;
        try
        {
            device = physical_device.createDeviceUnique(device_ci);
            Logger::Info(__FUNCTION__ " created logical device");
        }
        catch (std::exception & e)
        {
            Logger::Critical<true>(__FUNCTION__ " could not create device " + std::string(e.what()));
        }
        return device;
    }

    vk::UniqueCommandPool
    create_command_pool(const vk::Device & device, const uint32_t graphics_queue_family_index)
    {
        vk::CommandPoolCreateInfo pool_ci = {};
        {
            pool_ci.setQueueFamilyIndex(graphics_queue_family_index);
        }
        return device.createCommandPoolUnique(pool_ci);
    }

    QueueFamilyIndices
    explore_queue_family(const vk::PhysicalDevice &          physical_device,
                         const std::optional<vk::SurfaceKHR> surface = std::nullopt)
    {
        QueueFamilyIndices result;

        const auto queue_family_properties = physical_device.getQueueFamilyProperties();

        // show queue info
        for (size_t i_queue_family = 0; i_queue_family < queue_family_properties.size(); i_queue_family++)
        {
            std::string queue_family_info = "";
            if (surface.has_value())
            {
                if (physical_device.getSurfaceSupportKHR(static_cast<uint32_t>(i_queue_family), *surface))
                {
                    queue_family_info += "present, ";
                }
            }
            if (queue_family_properties[i_queue_family].queueFlags & vk::QueueFlagBits::eGraphics)
            {
                queue_family_info += "graphics, ";
            }
            if (queue_family_properties[i_queue_family].queueFlags & vk::QueueFlagBits::eCompute)
            {
                queue_family_info += "compute, ";
            }
            if (queue_family_properties[i_queue_family].queueFlags & vk::QueueFlagBits::eTransfer)
            {
                queue_family_info += "transfer, ";
            }
            Logger::Info(__FUNCTION__ ", queue family info, queue count : " +
                         std::to_string(queue_family_properties[i_queue_family].queueCount) +
                         ", queue family info : " + queue_family_info);
        }

        // find dedicated compute queue
        std::set<size_t> selected_queue_family_ids;
        const uint32_t   not_found = std::numeric_limits<uint32_t>::max();

        auto select_queue_family = [&](const vk::QueueFlags queue_flag,
                                       const bool           need_present,
                                       const bool           check_if_selected) -> uint32_t {
            for (size_t i_queue_family = 0; i_queue_family < queue_family_properties.size(); i_queue_family++)
            {
                if (check_if_selected)
                {
                    if (selected_queue_family_ids.find(i_queue_family) != selected_queue_family_ids.end())
                    {
                        continue;
                    }
                }

                bool has_all_queue_flags   = false;
                bool has_present_if_needed = !need_present;
                if (queue_family_properties[i_queue_family].queueFlags & queue_flag)
                {
                    has_all_queue_flags = true;
                }

                if (surface.has_value())
                {
                    if (physical_device.getSurfaceSupportKHR(static_cast<uint32_t>(i_queue_family), *surface) && need_present)
                    {
                        has_present_if_needed = true;
                    }
                }

                if (has_all_queue_flags && has_present_if_needed)
                {
                    return static_cast<uint32_t>(i_queue_family);
                }
            }
            return not_found;
        };

        // first round: try to select the queue that dedicate to specific task
        // graphics -> compute -> transfer
        const bool need_present = surface.has_value();
        result.m_graphics = select_queue_family(vk::QueueFlagBits::eGraphics, need_present, true);
        if (result.m_graphics == not_found)
        {
            Logger::Critical<true>(
                __FUNCTION__ " could not find a graphics queue that shares a present queue on this hardware");
            return result;
        }
        result.m_present = result.m_graphics;
        selected_queue_family_ids.insert(result.m_graphics);

        // compute
        result.m_compute = select_queue_family(vk::QueueFlagBits::eCompute, false, true);
        if (result.m_compute == not_found)
        {
            Logger::Warn(
                __FUNCTION__ " could not find a dedicated compute queue. Try to find a non-dedicated compute queue");
            result.m_compute = select_queue_family(vk::QueueFlagBits::eCompute, false, false);
        }
        if (result.m_compute == not_found)
        {
            Logger::Critical<true>(__FUNCTION__ " could not find a compute queue.");
            return result;
        }
        selected_queue_family_ids.insert(result.m_compute);

        // transfer
        result.m_transfer = select_queue_family(vk::QueueFlagBits::eTransfer, false, true);
        if (result.m_transfer == not_found)
        {
            Logger::Warn(
                __FUNCTION__ " could not find a dedicated transfer queue. Try to find a non-dedicated transfer queue");
            result.m_transfer = select_queue_family(vk::QueueFlagBits::eTransfer, false, false);
        }
        if (result.m_transfer == not_found)
        {
            Logger::Critical<true>(__FUNCTION__ " could not find a transfer queue.");
            return result;
        }
        selected_queue_family_ids.insert(result.m_transfer);

        return result;
    }
};
} // namespace VKA_NAME
