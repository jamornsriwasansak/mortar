#pragma once

#include "common/mortar.h"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

const std::vector<const char *> InstanceExtensions =
{
	// required by raytracing
	 VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
};

const std::vector<const char *> DeviceExtensions =
{
	// required by swapchain
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,

	// required by raytracing
	VK_NV_RAY_TRACING_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	VK_KHR_MAINTENANCE3_EXTENSION_NAME,

	// required for sharing SSBO scene
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

struct Core
{
	uint32_t									m_vk_api_version;
	uint32_t									m_vk_api_made_version;
	GLFWwindow *								m_glfw_window;
	uvec2										m_resolution;
	vk::UniqueInstance							m_vk_instance;
	vk::PhysicalDevice							m_vk_physical_device;
	vk::UniqueDevice							m_vk_device;
	vk::UniqueSurfaceKHR						m_vk_surface;
	uint32_t									m_vk_graphics_queue_family_index;
	uint32_t									m_vk_present_queue_family_index;
	vk::Queue									m_vk_graphics_queue;
	vk::Queue									m_vk_present_queue;
	vk::UniqueSwapchainKHR						m_vk_swapchain;
	vk::UniqueCommandPool						m_vk_command_pool;
	vk::Format									m_vk_swapchain_surface_format;
	std::vector<vk::Image>						m_vk_swapchain_images;
	std::vector<vk::UniqueImageView>			m_vk_swapchain_image_views;
	vk::UniqueDescriptorPool					m_vk_descriptor_pool;
	vk::PhysicalDeviceRayTracingPropertiesNV	m_vk_rt_properties;

	static
	Core &
	Inst()
	{
		static Core singleton;
		return singleton;
	}

	void
	one_time_command_submit(const std::function<void(vk::CommandBuffer &)> & func)
	{
		// create a temp command buffer
		vk::CommandBufferAllocateInfo alloc_info = {};
		{
			alloc_info.setLevel(vk::CommandBufferLevel::ePrimary);
			alloc_info.setCommandPool(*m_vk_command_pool);
			alloc_info.setCommandBufferCount(1);
		}
		std::vector<vk::UniqueCommandBuffer> command_buffers = m_vk_device->allocateCommandBuffersUnique(alloc_info);
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

	struct LoopCallbackParameters
	{
		std::array<vk::Semaphore, 1>	m_wait_semaphores;
		std::array<vk::Semaphore, 1>	m_signal_semaphores;
		vk::Fence						m_inflight_fence;
		int								m_image_index = -1;
	};

	void
	loop(const std::function<void(const LoopCallbackParameters & cb_params)> & callback)
	{
		const int MaxFrameInFlight = 3;
		std::vector<vk::UniqueSemaphore> image_available_semaphores(MaxFrameInFlight);
		std::vector<vk::UniqueSemaphore> signal_semaphores(MaxFrameInFlight);
		std::vector<vk::UniqueFence> inflight_fences(MaxFrameInFlight);
		for (size_t i = 0; i < MaxFrameInFlight; i++)
		{
			vk::SemaphoreCreateInfo semaphore_ci = {};
			image_available_semaphores[i] = m_vk_device->createSemaphoreUnique(semaphore_ci);
			signal_semaphores[i] = m_vk_device->createSemaphoreUnique(semaphore_ci);

			vk::FenceCreateInfo fence_ci = {};
			fence_ci.setFlags(vk::FenceCreateFlagBits::eSignaled);
			inflight_fences[i] = m_vk_device->createFenceUnique(fence_ci);
		}

		int i_inflight_frame = 0;
		while (!glfwWindowShouldClose(m_glfw_window))
		{
			// wait for previous inflight frame to finish
			const std::array<vk::Fence, 1> fences = { *inflight_fences[i_inflight_frame] };
			m_vk_device->waitForFences(fences,
									   VK_TRUE,
									   std::numeric_limits<uint64_t>::max());
			m_vk_device->resetFences(fences);

			// glfw poll events
			glfwPollEvents();

			// if esc is press, request closing the windows
			if (glfwGetKey(m_glfw_window, GLFW_KEY_ESCAPE))
			{
				glfwSetWindowShouldClose(m_glfw_window, GLFW_TRUE);
			}

			const vk::Semaphore & image_available_semaphore = *image_available_semaphores[i_inflight_frame];
			const vk::Semaphore & signal_semaphore = *signal_semaphores[i_inflight_frame];

			const uint32_t image_index = m_vk_device->acquireNextImageKHR(*m_vk_swapchain,
																		  std::numeric_limits<uint64_t>::max(),
																		  image_available_semaphore,
																		  nullptr).value;

			// loop callback
			LoopCallbackParameters cb_params;
			cb_params.m_wait_semaphores = { image_available_semaphore };
			cb_params.m_signal_semaphores = { signal_semaphore };
			cb_params.m_image_index = image_index;
			cb_params.m_inflight_fence = *inflight_fences[i_inflight_frame];
			callback(cb_params);
			
			// create present info
			vk::PresentInfoKHR present_info = {};
			vk::SwapchainKHR swapchains[] = { *m_vk_swapchain };
			{
				present_info.setWaitSemaphoreCount(1);
				present_info.setPWaitSemaphores(&signal_semaphore);
				present_info.setSwapchainCount(1);
				present_info.setPSwapchains(swapchains);
				present_info.setPImageIndices(&image_index);
				present_info.setPResults(nullptr);
			}

			// present
			m_vk_present_queue.presentKHR(present_info);

			// update inflight frame
			i_inflight_frame = (i_inflight_frame + 1) % MaxFrameInFlight;
		}
		m_vk_device->waitIdle();
	}

	std::vector<vk::UniqueCommandBuffer>
	create_command_buffers()
	{
		vk::CommandBufferAllocateInfo command_ai = {};
		{
			command_ai.setCommandPool(*m_vk_command_pool);
			command_ai.setLevel(vk::CommandBufferLevel::ePrimary);
			command_ai.setCommandBufferCount(uint32_t(m_vk_swapchain_images.size()));
		}
		return m_vk_device->allocateCommandBuffersUnique(command_ai);
	}

private:

	Core():
		m_resolution(ImageExtent)
	{
		// get list of extensions and layers for instance
		std::vector<const char *> instance_extensions;
		std::vector<const char *> instance_layers;
		{
			instance_extensions.insert(instance_extensions.end(),
									   InstanceExtensions.begin(),
									   InstanceExtensions.end());
			if (UseValidationLayer)
			{
				instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				instance_layers.push_back("VK_LAYER_KHRONOS_validation");
			}
		}

		// get list of extensions and layers for logical device
		std::vector<const char *> device_extensions;
		std::vector<const char *> device_layers;
		{
			device_extensions.insert(device_extensions.end(),
									 DeviceExtensions.begin(),
									 DeviceExtensions.end());
			if (UseValidationLayer)
			{
				device_layers.push_back("VK_LAYER_KHRONOS_validation");
			}
		}

		// init
		init(ImageExtent,
			 instance_extensions,
			 instance_layers,
			 device_extensions,
			 device_layers);
	}

	Core(const Core &) = delete;

	~Core()
	{
		glfwDestroyWindow(m_glfw_window);
		glfwTerminate();
	}

	void
	init(const uvec2 resolution,
		 std::vector<const char *> instance_extensions,
		 std::vector<const char *> instance_layers,
		 std::vector<const char *> device_extensions,
		 std::vector<const char *> device_layers)
	{
		// vk version
		m_vk_api_version = VkVersion;
		m_vk_api_made_version = VK_MAKE_VERSION(digit(m_vk_api_version, 2),
												digit(m_vk_api_version, 1),
												digit(m_vk_api_version, 0));

		// init glfw3 and its windows
		m_glfw_window = init_glfw3(resolution);

		// so that we don't have to manually load every extensions' functions by ourselves
		// https://github.com/KhronosGroup/Vulkan-Hpp#extensions--per-device-function-pointers
		vk::DynamicLoader dynamic_loader;
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

		// get list of extension and layers for instance
		std::vector<const char *> glfw_extensions = query_glfw_extensions();
		instance_extensions.insert(instance_extensions.end(),
								   glfw_extensions.begin(),
								   glfw_extensions.end());

		// make sure validation layer and debug extension are available
		check_instance_extensions_support(instance_extensions);
		check_instance_layers_support(instance_layers);

		// create instance
		m_vk_instance = create_instance(instance_extensions,
										instance_layers);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_vk_instance);

		// create surface
		m_vk_surface = create_surface(*m_vk_instance,
									  m_glfw_window);

		// get suitable physical device
		m_vk_physical_device = get_suitable_physical_device(*m_vk_instance);

		// get properties for rt
		auto properties = m_vk_physical_device.getProperties2<vk::PhysicalDeviceProperties2,
															  vk::PhysicalDeviceRayTracingPropertiesNV>();
		m_vk_rt_properties = properties.get<vk::PhysicalDeviceRayTracingPropertiesNV>();

		// find graphics queue
		auto families_result = find_graphics_and_present_queue_families(m_vk_physical_device,
																		*m_vk_surface);
		THROW_ASSERT(families_result.m_graphics_queue_family_index.has_value(), "no graphics queue");
		THROW_ASSERT(families_result.m_present_queue_family_index.has_value(), "no present queue");
		m_vk_graphics_queue_family_index = families_result.m_graphics_queue_family_index.value();
		m_vk_present_queue_family_index = families_result.m_present_queue_family_index.value();

		// create logical device from physical device
		const auto queue_family_indices = { m_vk_graphics_queue_family_index,
											m_vk_present_queue_family_index };
		m_vk_device = create_device(m_vk_physical_device,
									device_layers,
									device_extensions,
									queue_family_indices);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_vk_device);

		// get graphics queue from the logical device
		m_vk_graphics_queue = m_vk_device->getQueue(m_vk_graphics_queue_family_index, 0);

		// get present queue from the logical device
		m_vk_present_queue = m_vk_device->getQueue(m_vk_present_queue_family_index, 0);

		// create swapchain
		auto swapchain_result = create_swapchain(m_vk_physical_device,
												 *m_vk_device,
												 *m_vk_surface,
												 m_vk_graphics_queue_family_index,
												 m_vk_present_queue_family_index);
		m_vk_swapchain = std::move(swapchain_result.m_swapchain);
		m_vk_swapchain_surface_format = swapchain_result.m_swapchain_surface_format;
		m_vk_swapchain_images = m_vk_device->getSwapchainImagesKHR(*m_vk_swapchain);
		m_vk_swapchain_image_views = create_swapchain_image_views(*m_vk_device,
																  m_vk_swapchain_images,
																  m_vk_swapchain_surface_format,
																  Extent);

		// create descriptor pool
		m_vk_descriptor_pool = create_descriptor_pool(*m_vk_device,
													  m_vk_swapchain_images.size());

		// create command pool and buffers
		m_vk_command_pool = create_command_pool(*m_vk_device,
												m_vk_graphics_queue_family_index);
	}

	GLFWwindow *
	init_glfw3(const uvec2 & resolution)
	{
		// init glfw3
		THROW_ASSERT(glfwInit(), "could not init glfw");
		THROW_ASSERT(glfwVulkanSupported(), "does not support vulkan");

		// setup glfw hints
		glfwDefaultWindowHints();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

		// create glfw windows and focus on the window
		GLFWwindow * glfw_window = glfwCreateWindow(resolution.x, resolution.y, "mortar", nullptr, nullptr);
		THROW_ASSERT(glfw_window != nullptr, "cannot create glfw window");
		glfwMakeContextCurrent(glfw_window);
		return glfw_window;
	}
	
	std::vector<const char *>
	query_glfw_extensions()
	{
		// query extensions and number of extensions
		uint32_t num_glfw_extension = 0;
		const char ** glfw_extensions = glfwGetRequiredInstanceExtensions(&num_glfw_extension);

		// turn it into vector
		std::vector<const char *> result(num_glfw_extension);
		for (size_t i_ext = 0; i_ext < num_glfw_extension; i_ext++)
		{
			result[i_ext] = glfw_extensions[i_ext];
		}

		// add debug extension if required
		if (UseValidationLayer)
		{
			result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		return result;
	}

	void
	check_instance_layers_support(const std::vector<const char *> layers)
	{
		// at first no layer is found
		std::vector<std::string> not_found_layer_names(layers.begin(),
													   layers.end());

		// query all layer properties
		const auto layer_props = vk::enumerateInstanceLayerProperties();

		// slowly remove a layer name from the not_found_layer_names if found
		for (const std::string & required_layer_name : layers)
			for (const vk::LayerProperties & layer_prop : layer_props)
				if (layer_prop.layerName == required_layer_name)
				{
					const auto remove_elements = std::remove(not_found_layer_names.begin(),
															 not_found_layer_names.end(),
															 required_layer_name);
					not_found_layer_names.erase(remove_elements,
												not_found_layer_names.end());
				}

		// if not found, print the names of layers and throw error
		if (not_found_layer_names.size() > 0)
		{
			for (const std::string & not_found_layer_name : not_found_layer_names)
			{
				std::cout << "couldn't find :" + not_found_layer_name << std::endl;
			}
			THROW("couldn't find some required validation layers");
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
				if (extension_prop.extensionName == required_extension_name)
				{
					const auto remove_elements = std::remove(not_found_extension_names.begin(),
															 not_found_extension_names.end(),
															 required_extension_name);
					not_found_extension_names.erase(remove_elements,
													not_found_extension_names.end());
				}

		// if not found, print the names of extensions and throw error
		if (not_found_extension_names.size() > 0)
		{
			for (const std::string & not_found_extension_name : not_found_extension_names)
			{
				std::cout << "couldn't find :" + not_found_extension_name << std::endl;
			}
			THROW("couldn't find some required extensions layers");
		}
	}

	vk::UniqueInstance
	create_instance(const std::vector<const char *> & instance_extensions,
					const std::vector<const char *> & instance_layers)
	{
		// application information
		vk::ApplicationInfo app_info = {};
		{
			app_info.setApiVersion(m_vk_api_made_version);
			app_info.setApplicationVersion(0);
			app_info.setEngineVersion(0);
			app_info.setPApplicationName("");
			app_info.setPEngineName("");
		}

		// information for creating vk instance
		vk::InstanceCreateInfo instance_ci = {};
		{
			instance_ci.setPApplicationInfo(&app_info);
			instance_ci.setEnabledExtensionCount(uint32_t(instance_extensions.size()));
			instance_ci.setPpEnabledExtensionNames(instance_extensions.data());
			instance_ci.setEnabledLayerCount(uint32_t(instance_layers.size()));
			instance_ci.setPpEnabledLayerNames(instance_layers.data());
		}

		// create instance
		return vk::createInstanceUnique(instance_ci, nullptr);
	}

	vk::UniqueSurfaceKHR
	create_surface(const vk::Instance & vk_instance,
				   GLFWwindow * window)
	{
		// trick from https://github.com/KhronosGroup/Vulkan-Hpp/issues/242#issuecomment-415520944
		VkSurfaceKHR tmp_surface;
		VkResult err = glfwCreateWindowSurface(vk_instance, window, nullptr, &tmp_surface);
		// probably vulkan hpp bug. still dunno how to fix it
		vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderDynamic> destroyer(vk_instance,
																			 nullptr,
																			 VULKAN_HPP_DEFAULT_DISPATCHER);
		return vk::UniqueSurfaceKHR(tmp_surface,
									destroyer);
	}

	vk::PhysicalDevice
	get_suitable_physical_device(const vk::Instance & vk_instance)
	{
		const auto devices = vk_instance.enumeratePhysicalDevices();

		// try finding discrete gpu first
		for (const auto & device : devices)
			if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
				return device;

		// if could not find then we try with integrated gpu
		for (const auto & device : devices)
			if (device.getProperties().deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
				return device;

		return nullptr;
	}

	vk::UniqueDevice
	create_device(const vk::PhysicalDevice & physical_device,
				  const std::vector<const char *> layers,
				  const std::vector<const char *> extensions,
				  const std::vector<uint32_t> family_indices_)
	{
		// make sure all indices in the provided family_indices is unique
		std::set<uint32_t> family_indices_set_tmp(family_indices_.begin(), family_indices_.end());
		std::vector<uint32_t> family_indices(family_indices_set_tmp.begin(), family_indices_set_tmp.end());

		// create info for all queues
		const float queue_priority = 1.0f;
		std::vector<vk::DeviceQueueCreateInfo> queue_cis;
		for (const uint32_t family_index : family_indices)
		{
			vk::DeviceQueueCreateInfo queue_ci;
			queue_ci.setQueueFamilyIndex(family_index);
			queue_ci.setQueueCount(1);
			queue_ci.setPQueuePriorities(&queue_priority);
			queue_cis.push_back(queue_ci);
		}

		// specify device features
		vk::PhysicalDeviceFeatures device_features = {};
		device_features.setSamplerAnisotropy(true);
		device_features.setShaderInt64(true);

		vk::PhysicalDeviceDescriptorIndexingFeatures device_indexing_features = {};
		device_indexing_features.setShaderStorageBufferArrayNonUniformIndexing(true);
		device_indexing_features.setRuntimeDescriptorArray(true);

		vk::PhysicalDevice8BitStorageFeatures features8 = {};
		features8.setStorageBuffer8BitAccess(true);
		features8.setPNext(&device_indexing_features);

		vk::PhysicalDeviceFeatures2 device_features2 = {};
		device_features2.setFeatures(device_features);
		device_features2.setPNext(&features8);

		// setup create info for logical device
		vk::DeviceCreateInfo device_ci = {};
		{
			device_ci.setPQueueCreateInfos(queue_cis.data());
			device_ci.setQueueCreateInfoCount(uint32_t(queue_cis.size()));
			device_ci.setEnabledExtensionCount(uint32_t(extensions.size()));
			device_ci.setPpEnabledExtensionNames(extensions.data());
			device_ci.setEnabledLayerCount(uint32_t(layers.size()));
			device_ci.setPpEnabledLayerNames(layers.data());
			device_ci.setPNext(&device_features2);
		}
		vk::UniqueDevice device = physical_device.createDeviceUnique(device_ci);

		return std::move(device);
	}

	struct QueueFamiliesReturnType
	{
		std::optional<uint32_t> m_graphics_queue_family_index;
		std::optional<uint32_t> m_present_queue_family_index;
	};
	QueueFamiliesReturnType
	find_graphics_and_present_queue_families(const vk::PhysicalDevice & physical_device,
											 const vk::SurfaceKHR & surface)
	{
		std::optional<uint32_t> graphics_queue_family_index;
		std::optional<uint32_t> present_queue_family_index;

		const auto queue_family_properties = physical_device.getQueueFamilyProperties();
		for (size_t iq = 0; iq < queue_family_properties.size(); iq++)
		{
			if (queue_family_properties[iq].queueFlags & vk::QueueFlagBits::eGraphics)
			{
				graphics_queue_family_index = uint32_t(iq);
			}
			if (physical_device.getSurfaceSupportKHR(uint32_t(iq),
													 surface))
			{
				present_queue_family_index = uint32_t(iq);
			}
		}
		return { graphics_queue_family_index, present_queue_family_index };
	}

	struct SwapchainReturnType
	{
		vk::UniqueSwapchainKHR m_swapchain;
		vk::Format m_swapchain_surface_format;
	};
	SwapchainReturnType
	create_swapchain(const vk::PhysicalDevice & physical_device,
					 const vk::Device & device,
					 const vk::SurfaceKHR & surface,
					 const uint32_t graphics_queue_family_index,
					 const uint32_t present_queue_family_index)
	{
		vk::Format swapchain_surface_format;

		// check surface formats
		const auto surface_formats = physical_device.getSurfaceFormatsKHR(surface);
		bool found_wanted_surface_format = false;
		for (const vk::SurfaceFormatKHR & surface_format : surface_formats)
		{
			if (surface_format.format == RgbaUnormFormat &&
				surface_format.colorSpace == ColorSpace)
			{
				found_wanted_surface_format = true;
				swapchain_surface_format = surface_format.format;
				break;
			}
		}
		THROW_ASSERT(found_wanted_surface_format, "could not find specified surface format or colorspace");

		// check present modes
		const auto present_modes = physical_device.getSurfacePresentModesKHR(surface);
		bool found_wanted_present_mode = false;
		for (const vk::PresentModeKHR & present_mode : present_modes)
		{
			if (present_mode == PresentMode)
			{
				found_wanted_present_mode = true;
				break;
			}
		}
		THROW_ASSERT(found_wanted_present_mode, "could not find specified present mode");

		// based on surface capabilities find out minimum number of images + 1, we can put in the swapchain
		const auto capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
		const uint32_t num_min_images = capabilities.minImageCount;
		const uint32_t num_max_images = capabilities.maxImageCount == 0 ?
			std::numeric_limits<uint32_t>::max() :
			capabilities.maxImageCount;
		const uint32_t num_images = std::min(num_min_images + 1, num_max_images);

		// create family indices
		const uint32_t queue_family_indices[] = { graphics_queue_family_index, present_queue_family_index };

		vk::SwapchainCreateInfoKHR swapchain_ci = {};
		{
			swapchain_ci.setSurface(surface);
			swapchain_ci.setMinImageCount(num_images);
			swapchain_ci.setImageFormat(swapchain_surface_format);
			swapchain_ci.setImageColorSpace(ColorSpace);
			swapchain_ci.setImageExtent(vk::Extent2D(Width, Height));
			swapchain_ci.setImageArrayLayers(1);
			swapchain_ci.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
			if (graphics_queue_family_index == present_queue_family_index)
			{
				swapchain_ci.setImageSharingMode(vk::SharingMode::eExclusive);
			}
			else
			{
				swapchain_ci.setImageSharingMode(vk::SharingMode::eConcurrent);
				swapchain_ci.setQueueFamilyIndexCount(2);
				swapchain_ci.setPQueueFamilyIndices(queue_family_indices);
			}
			swapchain_ci.setPreTransform(capabilities.currentTransform);
			swapchain_ci.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
			swapchain_ci.setPresentMode(PresentMode);
			swapchain_ci.setClipped(true);
			swapchain_ci.setOldSwapchain(nullptr);
		}

		return { device.createSwapchainKHRUnique(swapchain_ci), swapchain_surface_format };
	}

	std::vector<vk::UniqueImageView>
	create_swapchain_image_views(const vk::Device & device,
								 const std::vector<vk::Image> & images,
								 const vk::Format & format,
								 const vk::Extent2D & extent)
	{
		std::vector<vk::UniqueImageView> image_views;
		for (const vk::Image & image : images)
		{
			vk::ImageViewCreateInfo image_view_ci;
			{
				image_view_ci.setImage(image);
				image_view_ci.setViewType(vk::ImageViewType::e2D);
				image_view_ci.setFormat(format);
				image_view_ci.components.setR(vk::ComponentSwizzle::eIdentity);
				image_view_ci.components.setG(vk::ComponentSwizzle::eIdentity);
				image_view_ci.components.setB(vk::ComponentSwizzle::eIdentity);
				image_view_ci.components.setA(vk::ComponentSwizzle::eIdentity);
				image_view_ci.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				image_view_ci.subresourceRange.setBaseMipLevel(0);
				image_view_ci.subresourceRange.setLevelCount(1);
				image_view_ci.subresourceRange.setBaseArrayLayer(0);
				image_view_ci.subresourceRange.setLayerCount(1);
			}
			image_views.push_back(device.createImageViewUnique(image_view_ci));
		}
		return image_views;
	}

	vk::UniqueDescriptorPool
	create_descriptor_pool(const vk::Device & device,
						   const size_t num_swapchain_images)
	{
		int num_descriptors = 100;
		int num_sets = 100;

		std::array<vk::DescriptorPoolSize, 2> pool_sizes;
		{
			pool_sizes[0].setType(vk::DescriptorType::eUniformBuffer);
			pool_sizes[0].setDescriptorCount(uint32_t(num_descriptors));
			
			pool_sizes[1].setType(vk::DescriptorType::eCombinedImageSampler);
			pool_sizes[1].setDescriptorCount(uint32_t(num_descriptors));
		}

		vk::DescriptorPoolCreateInfo pool_ci = {};
		{
			pool_ci.setPoolSizeCount(uint32_t(pool_sizes.size()));
			pool_ci.setPPoolSizes(pool_sizes.data());
			pool_ci.setMaxSets(uint32_t(num_swapchain_images) * num_sets);
		}

		vk::UniqueDescriptorPool result = device.createDescriptorPoolUnique(pool_ci);

		return result;
	}

	vk::UniqueCommandPool
	create_command_pool(const vk::Device & device,
						const uint32_t graphics_queue_family_index)
	{
		vk::CommandPoolCreateInfo pool_ci = {};
		{
			pool_ci.setQueueFamilyIndex(graphics_queue_family_index);
			pool_ci.setFlags(vk::CommandPoolCreateFlagBits::eProtected);
		}
		return device.createCommandPoolUnique(pool_ci);
	}
};