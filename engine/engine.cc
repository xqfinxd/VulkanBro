#include "engine.h"

#include <iostream>
#include <set>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <map>

#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vulkan/vulkan.h>

#include "stb_image.h"

namespace marble {

#ifdef NDEBUG
const bool kEnableValidationLayers = false;
#else
const bool kEnableValidationLayers = true;
#endif

extern SDL_Window* gWindow;

constexpr VkSampleCountFlagBits kSampleRate = VK_SAMPLE_COUNT_1_BIT;

std::vector<const char*> GetRequiredExtensions() {
	uint32_t extensionCount = 0;
	auto res = SDL_Vulkan_GetInstanceExtensions(gWindow, &extensionCount, nullptr);
	assert(SDL_TRUE == res);
	std::vector<const char*> extensions(extensionCount);
	res = SDL_Vulkan_GetInstanceExtensions(gWindow, &extensionCount, extensions.data());
	assert(SDL_TRUE == res);
	if (kEnableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT,
	VkDebugUtilsMessageTypeFlagsEXT,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void*) {
	std::cerr << callback_data->pMessage << std::endl;
	return VK_FALSE;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
	create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = (
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		);
	create_info.messageType = (
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
		);
	create_info.pfnUserCallback = DebugCallback;
}

struct VkSwapchainImage {
	VkImage image;
	VkImageView view;
};

struct VkImageDescriptor {
	VkFormat format;
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
};

struct VkBufferDescriptor {
	VkBuffer buffer;
	VkDeviceMemory memory;
	size_t size;
};

class Engine {
public:
	VkInstance instance{};
	VkDebugUtilsMessengerEXT debug_messenger{};
	VkPhysicalDevice gpu{ VK_NULL_HANDLE };
	uint32_t queue_family_count{};
	std::vector<VkQueueFamilyProperties> queue_family_properties{};
	VkPhysicalDeviceProperties gpu_properties{};
	VkPhysicalDeviceMemoryProperties memory_properties{};
	VkSurfaceKHR surface{};
	uint32_t graphics_queue_family_index{ UINT32_MAX };
	uint32_t present_queue_family_index{ UINT32_MAX };
	VkFormat format{ VK_FORMAT_UNDEFINED };
	VkDevice device{};
	VkCommandPool command_pool{};
	VkQueue graphics_queue{};
	VkQueue present_queue{};
	uint32_t swapchain_image_count{};
	VkSwapchainKHR swapchain{};
	std::vector<VkSwapchainImage> swapchain_images{};
	VkImageDescriptor depth_image{};
	uint32_t current_buffer{ 0 };
	VkSemaphore image_acquired_semaphore;
	VkSemaphore draw_complete_semaphore;
	VkFence fence;

	Engine();
	~Engine();

	bool GetMemoryType(uint32_t type_bits, VkFlags mask, uint32_t &type_index) {
		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
			if ((type_bits & 1) == 1) {
				if ((memory_properties.memoryTypes[i].propertyFlags & mask) == mask) {
					type_index = i;
					return true;
				}
			}
			type_bits >>= 1;
		}
		return false;
	}

	bool NextImage() {
		auto res = vkAcquireNextImageKHR(device, swapchain, 2000000, image_acquired_semaphore, VK_NULL_HANDLE, &current_buffer);
		if (res != VK_SUCCESS) {
			return false;
		}
		return true;
	}

	void SetImageLayout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect_mask, VkImageLayout old_image_layout,
		VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
		VkImageMemoryBarrier image_memory_barrier = {};
		image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		image_memory_barrier.pNext = NULL;
		image_memory_barrier.srcAccessMask = 0;
		image_memory_barrier.dstAccessMask = 0;
		image_memory_barrier.oldLayout = old_image_layout;
		image_memory_barrier.newLayout = new_image_layout;
		image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier.image = image;
		image_memory_barrier.subresourceRange.aspectMask = aspect_mask;
		image_memory_barrier.subresourceRange.baseMipLevel = 0;
		image_memory_barrier.subresourceRange.levelCount = 1;
		image_memory_barrier.subresourceRange.baseArrayLayer = 0;
		image_memory_barrier.subresourceRange.layerCount = 1;

		switch (old_image_layout) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;

		default:
			break;
		}

		switch (new_image_layout) {
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			image_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		default:
			break;
		}
		vkCmdPipelineBarrier(cmd, src_stages, dest_stages, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
	}

	void CreateShaderModule(VkShaderModule& shader_module, uint32_t* data, size_t size) {
		VkShaderModuleCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = size;
		info.pCode = data;
		vkCreateShaderModule(device, &info, nullptr, &shader_module);
	}
	void DestroyShaderModule(VkShaderModule& shader_module) {
		vkDestroyShaderModule(device, shader_module, nullptr);
	}

private:
	void CreateInstance() {
		const std::vector<const char*> validation_layers = {
			"VK_LAYER_KHRONOS_validation",
		};

		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Hello Triangle";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;

		auto extensions = GetRequiredExtensions();
		create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		create_info.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
		if (kEnableValidationLayers) {
			create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
			create_info.ppEnabledLayerNames = validation_layers.data();

			PopulateDebugMessengerCreateInfo(debug_create_info);
			create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
		} else {
			create_info.enabledLayerCount = 0;

			create_info.pNext = nullptr;
		}

		auto res = vkCreateInstance(&create_info, nullptr, &instance);
		assert(VK_SUCCESS == res);
	}
	void DestroyInstance() {
		vkDestroyInstance(instance, nullptr);
	}

	void SetupDebugMessenger() {
		if (!kEnableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT create_info;
		PopulateDebugMessengerCreateInfo(create_info);

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, &create_info, nullptr, &debug_messenger);
		}
	}
	void UninstallDebugMessenger() {
		if (!kEnableValidationLayers) return;
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debug_messenger, nullptr);
		}
	}

	void InitGpuInfo() {
		uint32_t gpu_count = 0;
		VkResult res = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
		assert(gpu_count);
		std::vector<VkPhysicalDevice> gpus(gpu_count);
		res = vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());
		assert(VK_SUCCESS == res);

		for (const auto& gpu_element : gpus) {
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(gpu_element, &properties);
			if (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == properties.deviceType) {
				gpu = gpu_element;
				break;
			}
		}

		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, nullptr);

		queue_family_properties.resize(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, queue_family_properties.data());
		assert(queue_family_count);

		vkGetPhysicalDeviceMemoryProperties(gpu, &memory_properties);
		vkGetPhysicalDeviceProperties(gpu, &gpu_properties);
	}

	void CreateSurface() {
		auto res = SDL_Vulkan_CreateSurface(gWindow, instance, &surface);
		assert(SDL_TRUE == res);
		std::vector<VkBool32> supports_present = std::vector<VkBool32>(queue_family_count);
		for (uint32_t i = 0; i < queue_family_count; i++) {
			vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supports_present[i]);
		}

		graphics_queue_family_index = UINT32_MAX;
		present_queue_family_index = UINT32_MAX;
		for (uint32_t i = 0; i < queue_family_count; ++i) {
			if ((queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				if (graphics_queue_family_index == UINT32_MAX) graphics_queue_family_index = i;

				if (supports_present[i] == VK_TRUE) {
					graphics_queue_family_index = i;
					present_queue_family_index = i;
					break;
				}
			}
		}

		if (present_queue_family_index == UINT32_MAX) {
			for (uint32_t i = 0; i < queue_family_count; ++i)
				if (supports_present[i] == VK_TRUE) {
					present_queue_family_index = i;
					break;
				}
		}

		assert(graphics_queue_family_index != UINT32_MAX && present_queue_family_index != UINT32_MAX);

		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, NULL);
		std::vector<VkSurfaceFormatKHR> surf_formats = std::vector<VkSurfaceFormatKHR>(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, surf_formats.data());
		assert(format_count);

		format = surf_formats[0].format;
		for (size_t i = 0; i < format_count; ++i) {
			if (surf_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
				format = VK_FORMAT_B8G8R8A8_UNORM;
				break;
			}
		}
	}
	void DestroySurface() {
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}

	void CreateDevice() {
		VkDeviceQueueCreateInfo queue_info = {};

		float queue_priorities[1] = { 0.0 };
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = NULL;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = queue_priorities;
		queue_info.queueFamilyIndex = graphics_queue_family_index;

		std::vector<const char*> device_extensions{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};

		VkDeviceCreateInfo device_info = {};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = NULL;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queue_info;
		device_info.enabledExtensionCount = (uint32_t)device_extensions.size();
		device_info.ppEnabledExtensionNames = device_extensions.data();
		device_info.pEnabledFeatures = NULL;

		auto res = vkCreateDevice(gpu, &device_info, NULL, &device);
		assert(VK_SUCCESS == res);
	}
	void DestroyDevice() {
		vkDestroyDevice(device, nullptr);
	}

	void InitQueue() {
		vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);
		if (graphics_queue_family_index == present_queue_family_index) {
			present_queue = graphics_queue;
		} else {
			vkGetDeviceQueue(device, present_queue_family_index, 0, &present_queue);
		}
	}

	void CreateSwapchain() {
		VkResult res;
		VkSurfaceCapabilitiesKHR surf_capabilities;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surf_capabilities);

		uint32_t present_mode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &present_mode_count, NULL);
		std::vector<VkPresentModeKHR> present_modes(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &present_mode_count, present_modes.data());
		assert(present_mode_count);

		VkExtent2D swapchain_extent;
		if (surf_capabilities.currentExtent.width == 0xFFFFFFFF) {
			int width = 0, height = 0;
			SDL_Vulkan_GetDrawableSize(gWindow, &width, &height);
			swapchain_extent.width = width;
			swapchain_extent.height = height;
			if (swapchain_extent.width < surf_capabilities.minImageExtent.width) {
				swapchain_extent.width = surf_capabilities.minImageExtent.width;
			} else if (swapchain_extent.width > surf_capabilities.maxImageExtent.width) {
				swapchain_extent.width = surf_capabilities.maxImageExtent.width;
			}

			if (swapchain_extent.height < surf_capabilities.minImageExtent.height) {
				swapchain_extent.height = surf_capabilities.minImageExtent.height;
			} else if (swapchain_extent.height > surf_capabilities.maxImageExtent.height) {
				swapchain_extent.height = surf_capabilities.maxImageExtent.height;
			}
		} else {
			swapchain_extent = surf_capabilities.currentExtent;
		}

		VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

		uint32_t min_swapchain_image_count = surf_capabilities.minImageCount;

		VkSurfaceTransformFlagBitsKHR pretransform;
		if (surf_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			pretransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		} else {
			pretransform = surf_capabilities.currentTransform;
		}

		VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		VkCompositeAlphaFlagBitsKHR composite_alpha_flags[4] = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (uint32_t i = 0; i < sizeof(composite_alpha_flags) / sizeof(composite_alpha_flags[0]); i++) {
			if (surf_capabilities.supportedCompositeAlpha & composite_alpha_flags[i]) {
				composite_alpha = composite_alpha_flags[i];
				break;
			}
		}

		VkSwapchainCreateInfoKHR swapchain_ci = {};
		swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_ci.pNext = NULL;
		swapchain_ci.surface = surface;
		swapchain_ci.minImageCount = min_swapchain_image_count;
		swapchain_ci.imageFormat = format;
		swapchain_ci.imageExtent.width = swapchain_extent.width;
		swapchain_ci.imageExtent.height = swapchain_extent.height;
		swapchain_ci.preTransform = pretransform;
		swapchain_ci.compositeAlpha = composite_alpha;
		swapchain_ci.imageArrayLayers = 1;
		swapchain_ci.presentMode = swapchain_present_mode;
		swapchain_ci.oldSwapchain = VK_NULL_HANDLE;
		swapchain_ci.clipped = true;
		swapchain_ci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_ci.queueFamilyIndexCount = 0;
		swapchain_ci.pQueueFamilyIndices = NULL;
		uint32_t queue_family_indices[2] = { (uint32_t)graphics_queue_family_index, (uint32_t)present_queue_family_index };
		if (graphics_queue_family_index != present_queue_family_index) {
			swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchain_ci.queueFamilyIndexCount = 2;
			swapchain_ci.pQueueFamilyIndices = queue_family_indices;
		}

		res = vkCreateSwapchainKHR(device, &swapchain_ci, NULL, &swapchain);
		assert(VK_SUCCESS == res);

		vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, NULL);

		std::vector<VkImage> temp_images(swapchain_image_count);
		res = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, temp_images.data());
		assert(VK_SUCCESS == res);

		for (uint32_t i = 0; i < swapchain_image_count; i++) {
			VkSwapchainImage image_desc;
			image_desc.image = temp_images[i];

			VkImageViewCreateInfo color_image_view = {};
			color_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			color_image_view.pNext = NULL;
			color_image_view.format = format;
			color_image_view.components.r = VK_COMPONENT_SWIZZLE_R;
			color_image_view.components.g = VK_COMPONENT_SWIZZLE_G;
			color_image_view.components.b = VK_COMPONENT_SWIZZLE_B;
			color_image_view.components.a = VK_COMPONENT_SWIZZLE_A;
			color_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			color_image_view.subresourceRange.baseMipLevel = 0;
			color_image_view.subresourceRange.levelCount = 1;
			color_image_view.subresourceRange.baseArrayLayer = 0;
			color_image_view.subresourceRange.layerCount = 1;
			color_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			color_image_view.flags = 0;
			color_image_view.image = temp_images[i];
			vkCreateImageView(device, &color_image_view, NULL, &image_desc.view);

			swapchain_images.push_back(image_desc);
		}
		current_buffer = 0;
	}
	void DestroySwapchain() {
		for (const auto& d : swapchain_images) {
			vkDestroyImageView(device, d.view, nullptr);
		}
		swapchain_images.clear();
		vkDestroySwapchainKHR(device, swapchain, nullptr);
	}

	void CreateDepthImage() {
		VkResult res;
		VkImageCreateInfo image_info = {};
		VkFormatProperties props;

		if (depth_image.format == VK_FORMAT_UNDEFINED) depth_image.format = VK_FORMAT_D16_UNORM;

		const VkFormat depth_format = depth_image.format;
		vkGetPhysicalDeviceFormatProperties(gpu, depth_format, &props);
		if (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			image_info.tiling = VK_IMAGE_TILING_LINEAR;
		} else if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		} else {
			assert(0);
		}

		int width = 0, height = 0;
		SDL_Vulkan_GetDrawableSize(gWindow, &width, &height);

		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.pNext = nullptr;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = depth_format;
		image_info.extent.width = width;
		image_info.extent.height = height;
		image_info.extent.depth = 1;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = kSampleRate;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_info.queueFamilyIndexCount = 0;
		image_info.pQueueFamilyIndices = nullptr;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		image_info.flags = 0;

		VkMemoryAllocateInfo mem_alloc = {};
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_alloc.pNext = nullptr;
		mem_alloc.allocationSize = 0;
		mem_alloc.memoryTypeIndex = 0;

		VkImageViewCreateInfo view_info = {};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.pNext = nullptr;
		view_info.image = VK_NULL_HANDLE;
		view_info.format = depth_format;
		view_info.components.r = VK_COMPONENT_SWIZZLE_R;
		view_info.components.g = VK_COMPONENT_SWIZZLE_G;
		view_info.components.b = VK_COMPONENT_SWIZZLE_B;
		view_info.components.a = VK_COMPONENT_SWIZZLE_A;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.flags = 0;

		if (depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
			depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
			depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
			view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		VkMemoryRequirements mem_reqs;

		res = vkCreateImage(device, &image_info, nullptr, &depth_image.image);
		assert(VK_SUCCESS == res);

		vkGetImageMemoryRequirements(device, depth_image.image, &mem_reqs);

		mem_alloc.allocationSize = mem_reqs.size;
		auto pass = GetMemoryType(mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mem_alloc.memoryTypeIndex);
		assert(pass);

		res = vkAllocateMemory(device, &mem_alloc, nullptr, &depth_image.memory);
		assert(VK_SUCCESS == res);

		res = vkBindImageMemory(device, depth_image.image, depth_image.memory, 0);
		assert(VK_SUCCESS == res);

		view_info.image = depth_image.image;
		res = vkCreateImageView(device, &view_info, NULL, &depth_image.view);
		assert(VK_SUCCESS == res);
	}
	void DestroyDepthImage() {
		vkDestroyImage(device, depth_image.image, nullptr);
		vkDestroyImageView(device, depth_image.view, nullptr);
		vkFreeMemory(device, depth_image.memory, nullptr);
	}

	void CreateFenceAndSemaphore() {
		VkSemaphoreCreateInfo semaphore_info;
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphore_info.pNext = nullptr;
		semaphore_info.flags = 0;

		auto res = vkCreateSemaphore(device, &semaphore_info, nullptr, &image_acquired_semaphore);
		assert(res == VK_SUCCESS);

		res = vkCreateSemaphore(device, &semaphore_info, nullptr, &draw_complete_semaphore);
		assert(res == VK_SUCCESS);

		VkFenceCreateInfo fence_info;
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.pNext = nullptr;
		fence_info.flags = 0;
		vkCreateFence(device, &fence_info, nullptr, &fence);
	}
	void DestroyFenceAndSemaphore() {
		vkDestroyFence(device, fence, nullptr);
		vkDestroySemaphore(device, draw_complete_semaphore, nullptr);
		vkDestroySemaphore(device, image_acquired_semaphore, nullptr);
	}
};

Engine::Engine() {
	CreateInstance();
	SetupDebugMessenger();
	InitGpuInfo();
	CreateSurface();
	CreateDevice();
	InitQueue();
	CreateSwapchain();
	CreateDepthImage();
	CreateFenceAndSemaphore();
}

Engine::~Engine() {
	DestroyFenceAndSemaphore();
	DestroyDepthImage();
	DestroySwapchain();
	DestroyDevice();
	DestroySurface();
	UninstallDebugMessenger();
	DestroyInstance();
}

std::vector<char> ReadFile(const char * filename) {
	std::ifstream fs{};
	fs.open(filename, std::ios::binary);
	fs.seekg(0, fs.end);
	uint32_t length = (uint32_t)fs.tellg();
	fs.seekg(0, fs.beg);
	std::vector<char> data(length);
	fs.read(data.data(), length);
	fs.close();
	return data;
}

Engine* gEngine = nullptr;

void InitEngine() {
	gEngine = new Engine();
}

void QuitEngine() {
	delete gEngine;
	gEngine = nullptr;
}

struct Program {
	Program() {
		assert(gEngine);
	}

	VkCommandPool cmd_pool;
	std::vector<VkCommandBuffer> cmds;
	VkRenderPass renderpass;
	std::vector<VkFramebuffer> framebuffers;
	VkDescriptorPool descriptor_pool;

	VkBufferDescriptor uniform_buffer;
	VkImageDescriptor sampled_image;
	VkSampler sampler;
	
	std::vector<VkBufferDescriptor> vertex_buffers;
	VkBufferDescriptor index_buffer;

	std::vector<VkVertexInputBindingDescription> vertex_bindings;
	std::vector<VkVertexInputAttributeDescription> vertex_attributes;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet descriptor_set;
	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;
	VkPipelineCache cache;

	void CreateCmdPool() {
		VkCommandPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.queueFamilyIndex = gEngine->graphics_queue_family_index;

		auto res = vkCreateCommandPool(gEngine->device, &pool_info, nullptr, &cmd_pool);
		assert(VK_SUCCESS == res);
	}

	VkCommandBuffer GenCmd(bool secondary, bool join) {
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = cmd_pool;
		alloc_info.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		VkCommandBuffer cmd;
		auto res = vkAllocateCommandBuffers(gEngine->device, &alloc_info, &cmd);
		if (res != VK_SUCCESS) {
			return VK_NULL_HANDLE;
		}
		if (join) {
			cmds.push_back(cmd);
		}
		return cmd;
	}

	void CreateRenderpass() {
		VkAttachmentDescription attachments[2];
		attachments[0].format = gEngine->format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachments[0].flags = 0;

		attachments[1].format = gEngine->depth_image.format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].flags = 0;

		VkAttachmentReference color_reference = {};
		color_reference.attachment = 0;
		color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_reference = {};
		depth_reference.attachment = 1;
		depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = NULL;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_reference;
		subpass.pResolveAttachments = NULL;
		subpass.pDepthStencilAttachment = &depth_reference;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = NULL;

		VkSubpassDependency subpass_dependency = {};
		subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpass_dependency.dstSubpass = 0;
		subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependency.srcAccessMask = 0;
		subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpass_dependency.dependencyFlags = 0;

		VkRenderPassCreateInfo rp_info = {};
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rp_info.pNext = NULL;
		rp_info.attachmentCount = 2;
		rp_info.pAttachments = attachments;
		rp_info.subpassCount = 1;
		rp_info.pSubpasses = &subpass;
		rp_info.dependencyCount = 1;
		rp_info.pDependencies = &subpass_dependency;

		auto res = vkCreateRenderPass(gEngine->device, &rp_info, NULL, &renderpass);
		assert(res == VK_SUCCESS);
	}

	void CreateFramebuffers() {
		VkImageView attachments[2];
		attachments[1] = gEngine->depth_image.view;

		int width = 0, height = 0;
		SDL_Vulkan_GetDrawableSize(gWindow, &width, &height);

		VkFramebufferCreateInfo fb_info = {};
		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_info.pNext = nullptr;
		fb_info.renderPass = renderpass;
		fb_info.attachmentCount = 2;
		fb_info.pAttachments = attachments;
		fb_info.width = width;
		fb_info.height = height;
		fb_info.layers = 1;

		VkResult res;
		framebuffers.resize(gEngine->swapchain_image_count);
		for (uint32_t i = 0; i < gEngine->swapchain_image_count; i++) {
			attachments[0] = gEngine->swapchain_images[i].view;
			res = vkCreateFramebuffer(gEngine->device, &fb_info, NULL, &framebuffers[i]);
			assert(res == VK_SUCCESS);
		}
	}
	
	void CreateDescriptorPool() {
		VkDescriptorPoolSize type_count[2];
		type_count[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		type_count[0].descriptorCount = 1;
		type_count[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		type_count[1].descriptorCount = 1;

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.pNext = nullptr;
		pool_info.maxSets = 1;
		pool_info.poolSizeCount = 2;
		pool_info.pPoolSizes = type_count;

		auto res = vkCreateDescriptorPool(gEngine->device, &pool_info, nullptr, &descriptor_pool);
		assert(res == VK_SUCCESS);
	}

	void CreateUniformBuffer() {
		VkBufferCreateInfo buf_info = {};
		buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_info.pNext = nullptr;
		buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		buf_info.size = sizeof(glm::mat4);
		buf_info.queueFamilyIndexCount = 0;
		buf_info.pQueueFamilyIndices = nullptr;
		buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buf_info.flags = 0;
		auto res = vkCreateBuffer(gEngine->device, &buf_info, nullptr, &uniform_buffer.buffer);
		assert(res == VK_SUCCESS);

		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(gEngine->device, uniform_buffer.buffer, &mem_reqs);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.memoryTypeIndex = 0;

		alloc_info.allocationSize = mem_reqs.size;
		bool pass = gEngine->GetMemoryType(mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			alloc_info.memoryTypeIndex);
		assert(pass && "No mappable, coherent memory");

		res = vkAllocateMemory(gEngine->device, &alloc_info, nullptr, &uniform_buffer.memory);
		assert(res == VK_SUCCESS);

		res = vkBindBufferMemory(gEngine->device, uniform_buffer.buffer, uniform_buffer.memory, 0);
		assert(res == VK_SUCCESS);

		uniform_buffer.size = sizeof(glm::mat4);
	}

	void CreateImage(const char* image) {
		int width = 0, height = 0, channel = 0;
		unsigned char* image_data = stbi_load(image, &width, &height, &channel, 4);
		sampled_image.format = VK_FORMAT_R8G8B8A8_UNORM;

		VkImageCreateInfo image_create_info = {};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.pNext = nullptr;
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_create_info.extent.width = width;
		image_create_info.extent.height = height;
		image_create_info.extent.depth = 1;
		image_create_info.mipLevels = 1;
		image_create_info.arrayLayers = 1;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		image_create_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		image_create_info.queueFamilyIndexCount = 0;
		image_create_info.pQueueFamilyIndices = nullptr;
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.flags = 0;

		VkMemoryAllocateInfo mem_alloc = {};
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_alloc.pNext = nullptr;
		mem_alloc.allocationSize = 0;
		mem_alloc.memoryTypeIndex = 0;

		VkMemoryRequirements mem_reqs;

		auto res = vkCreateImage(gEngine->device, &image_create_info, nullptr, &sampled_image.image);
		assert(res == VK_SUCCESS);

		vkGetImageMemoryRequirements(gEngine->device, sampled_image.image, &mem_reqs);

		mem_alloc.allocationSize = mem_reqs.size;

		VkFlags requirements = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		bool pass = gEngine->GetMemoryType(mem_reqs.memoryTypeBits, requirements, mem_alloc.memoryTypeIndex);
		assert(pass);

		res = vkAllocateMemory(gEngine->device, &mem_alloc, nullptr, &(sampled_image.memory));
		assert(res == VK_SUCCESS);

		res = vkBindImageMemory(gEngine->device, sampled_image.image, sampled_image.memory, 0);
		assert(res == VK_SUCCESS);

		VkImageSubresource subres = {};
		subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subres.mipLevel = 0;
		subres.arrayLayer = 0;

		VkSubresourceLayout layout = {};
		void *data;
		vkGetImageSubresourceLayout(gEngine->device, sampled_image.image, &subres, &layout);

		res = vkMapMemory(gEngine->device, sampled_image.memory, 0, mem_reqs.size, 0, &data);
		assert(res == VK_SUCCESS);

		memcpy(data, image_data, width * height * channel);

		vkUnmapMemory(gEngine->device, sampled_image.memory);

		VkImageViewCreateInfo view_info = {};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.pNext = nullptr;
		view_info.image = VK_NULL_HANDLE;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		view_info.components.r = VK_COMPONENT_SWIZZLE_R;
		view_info.components.g = VK_COMPONENT_SWIZZLE_G;
		view_info.components.b = VK_COMPONENT_SWIZZLE_B;
		view_info.components.a = VK_COMPONENT_SWIZZLE_A;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;

		view_info.image = sampled_image.image;
		res = vkCreateImageView(gEngine->device, &view_info, nullptr, &sampled_image.view);
		assert(res == VK_SUCCESS);

		auto cmd = GenCmd(false, false);

		VkCommandBufferBeginInfo cmd_begin_info = {};
		cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_begin_info.pNext = NULL;
		cmd_begin_info.flags = 0;
		cmd_begin_info.pInheritanceInfo = NULL;

		vkBeginCommandBuffer(cmd, &cmd_begin_info);
		gEngine->SetImageLayout(cmd, sampled_image.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_PREINITIALIZED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		vkEndCommandBuffer(cmd);
		const VkCommandBuffer cmd_bufs[] = { cmd };
		VkFenceCreateInfo fenceInfo;
		VkFence cmdFence;
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.pNext = nullptr;
		fenceInfo.flags = 0;
		vkCreateFence(gEngine->device, &fenceInfo, NULL, &cmdFence);

		VkSubmitInfo submit_info[1] = {};
		submit_info[0].pNext = nullptr;
		submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info[0].waitSemaphoreCount = 0;
		submit_info[0].pWaitSemaphores = nullptr;
		submit_info[0].pWaitDstStageMask = nullptr;
		submit_info[0].commandBufferCount = 1;
		submit_info[0].pCommandBuffers = cmd_bufs;
		submit_info[0].signalSemaphoreCount = 0;
		submit_info[0].pSignalSemaphores = nullptr;

		/* Queue the command buffer for execution */
		res = vkQueueSubmit(gEngine->graphics_queue, 1, submit_info, cmdFence);
		assert(res == VK_SUCCESS);

		do {
			res = vkWaitForFences(gEngine->device, 1, &cmdFence, VK_TRUE, 2000000);
		} while (res == VK_TIMEOUT);
		assert(res == VK_SUCCESS);

		vkDestroyFence(gEngine->device, cmdFence, nullptr);
		vkFreeCommandBuffers(gEngine->device, cmd_pool, 1, &cmd);
	}

	void CreateSampler() {
		VkSamplerCreateInfo sampler_info = {};
		sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_info.magFilter = VK_FILTER_NEAREST;
		sampler_info.minFilter = VK_FILTER_NEAREST;
		sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.mipLodBias = 0.0;
		sampler_info.anisotropyEnable = VK_FALSE;
		sampler_info.maxAnisotropy = 1;
		sampler_info.compareOp = VK_COMPARE_OP_NEVER;
		sampler_info.minLod = 0.0;
		sampler_info.maxLod = 0.0;
		sampler_info.compareEnable = VK_FALSE;
		sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		/* create sampler */
		auto res = vkCreateSampler(gEngine->device, &sampler_info, NULL, &sampler);
		assert(res == VK_SUCCESS);
	}

	void AddVertexBuffer(float* data, size_t size) {
		VkBufferDescriptor v_buffer;
		v_buffer.size = size;
		VkBufferCreateInfo buf_info = {};
		buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_info.pNext = nullptr;
		buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		buf_info.size = size;
		buf_info.queueFamilyIndexCount = 0;
		buf_info.pQueueFamilyIndices = nullptr;
		buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buf_info.flags = 0;
		auto res = vkCreateBuffer(gEngine->device, &buf_info, nullptr, &v_buffer.buffer);
		assert(res == VK_SUCCESS);

		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(gEngine->device, v_buffer.buffer, &mem_reqs);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.memoryTypeIndex = 0;

		alloc_info.allocationSize = mem_reqs.size;
		bool pass = gEngine->GetMemoryType(mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			alloc_info.memoryTypeIndex);
		assert(pass && "No mappable, coherent memory");

		res = vkAllocateMemory(gEngine->device, &alloc_info, nullptr, &v_buffer.memory);
		assert(res == VK_SUCCESS);

		res = vkBindBufferMemory(gEngine->device, v_buffer.buffer, v_buffer.memory, 0);
		assert(res == VK_SUCCESS);

		void* dest_data = nullptr;
		res = vkMapMemory(gEngine->device, v_buffer.memory, 0, mem_reqs.size, 0, &dest_data);
		assert(res == VK_SUCCESS);
		memcpy(dest_data, data, size);
		vkUnmapMemory(gEngine->device, v_buffer.memory);
		vertex_buffers.push_back(v_buffer);
	}

	void CreateIndexBuffer(uint32_t* data, size_t size) {
		index_buffer.size = size;
		VkBufferCreateInfo buf_info = {};
		buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_info.pNext = nullptr;
		buf_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		buf_info.size = size;
		buf_info.queueFamilyIndexCount = 0;
		buf_info.pQueueFamilyIndices = nullptr;
		buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buf_info.flags = 0;
		auto res = vkCreateBuffer(gEngine->device, &buf_info, nullptr, &index_buffer.buffer);
		assert(res == VK_SUCCESS);

		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(gEngine->device, index_buffer.buffer, &mem_reqs);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.memoryTypeIndex = 0;

		alloc_info.allocationSize = mem_reqs.size;
		bool pass = gEngine->GetMemoryType(mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			alloc_info.memoryTypeIndex);
		assert(pass && "No mappable, coherent memory");

		res = vkAllocateMemory(gEngine->device, &alloc_info, nullptr, &index_buffer.memory);
		assert(res == VK_SUCCESS);

		res = vkBindBufferMemory(gEngine->device, index_buffer.buffer, index_buffer.memory, 0);
		assert(res == VK_SUCCESS);

		void* dest_data = nullptr;
		res = vkMapMemory(gEngine->device, index_buffer.memory, 0, mem_reqs.size, 0, &dest_data);
		assert(res == VK_SUCCESS);
		memcpy(dest_data, data, size);
		vkUnmapMemory(gEngine->device, index_buffer.memory);
	}

	void VertexBinding(uint32_t binding, uint32_t stride) {
		VkVertexInputBindingDescription binding_desc{};
		binding_desc.binding = binding;
		binding_desc.stride = stride;
		binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		vertex_bindings.push_back(binding_desc);
	}

	void VertexAttribute(uint32_t binding, uint32_t location, uint32_t fmt, uint32_t offset) {
		VkVertexInputAttributeDescription attr_desc{};
		attr_desc.binding = binding;
		attr_desc.location = location;
		attr_desc.format = (VkFormat)fmt;
		attr_desc.offset = offset;
		vertex_attributes.push_back(attr_desc);
	}

	void CreateLayout() {
		VkDescriptorSetLayoutBinding layout_bindings[2];
		layout_bindings[0].binding = 0;
		layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layout_bindings[0].descriptorCount = 1;
		layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layout_bindings[0].pImmutableSamplers = nullptr;

		layout_bindings[1].binding = 1;
		layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layout_bindings[1].descriptorCount = 1;
		layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings[1].pImmutableSamplers = nullptr;
		
		VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
		descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_layout.pNext = nullptr;
		descriptor_layout.flags = 0;
		descriptor_layout.bindingCount = 2;
		descriptor_layout.pBindings = layout_bindings;

		auto res = vkCreateDescriptorSetLayout(gEngine->device, &descriptor_layout, nullptr, &descriptor_set_layout);
		assert(res == VK_SUCCESS);

		VkPipelineLayoutCreateInfo pipeline_layout_info = {};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.pNext = NULL;
		pipeline_layout_info.pushConstantRangeCount = 0;
		pipeline_layout_info.pPushConstantRanges = NULL;
		pipeline_layout_info.setLayoutCount = 1;
		pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

		res = vkCreatePipelineLayout(gEngine->device, &pipeline_layout_info, NULL, &pipeline_layout);
		assert(res == VK_SUCCESS);
	}

	void CreateDescriptorSet() {
		VkDescriptorSetAllocateInfo alloc_info[1];
		alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info[0].pNext = nullptr;
		alloc_info[0].descriptorPool = descriptor_pool;
		alloc_info[0].descriptorSetCount = 1;
		alloc_info[0].pSetLayouts = &descriptor_set_layout;

		auto res = vkAllocateDescriptorSets(gEngine->device, alloc_info, &descriptor_set);
		assert(res == VK_SUCCESS);

		VkWriteDescriptorSet writes[2];

		VkDescriptorBufferInfo buffer_info;
		buffer_info.buffer = uniform_buffer.buffer;
		buffer_info.offset = 0;
		buffer_info.range = uniform_buffer.size;

		VkDescriptorImageInfo image_info;
		image_info.sampler = sampler;
		image_info.imageView = sampled_image.view;
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		writes[0] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].pNext = nullptr;
		writes[0].dstSet = descriptor_set;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &buffer_info;
		writes[0].dstArrayElement = 0;
		writes[0].dstBinding = 0;

		writes[1] = {};
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptor_set;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &image_info;
		writes[1].dstArrayElement = 0;

		vkUpdateDescriptorSets(gEngine->device, 2, writes, 0, nullptr);
	}

	void CreatePipeline() {
		VkDynamicState dynamicStateEnables[2];
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pNext = NULL;
		dynamicState.pDynamicStates = dynamicStateEnables;
		dynamicState.dynamicStateCount = 0;

		VkPipelineVertexInputStateCreateInfo vi;
		memset(&vi, 0, sizeof(vi));
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi.pNext = NULL;
		vi.flags = 0;
		vi.vertexBindingDescriptionCount = vertex_bindings.size();
		vi.pVertexBindingDescriptions = vertex_bindings.data();
		vi.vertexAttributeDescriptionCount = vertex_attributes.size();
		vi.pVertexAttributeDescriptions = vertex_attributes.data();
		VkPipelineInputAssemblyStateCreateInfo ia;
		ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia.pNext = NULL;
		ia.flags = 0;
		ia.primitiveRestartEnable = VK_FALSE;
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineRasterizationStateCreateInfo rs;
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.pNext = NULL;
		rs.flags = 0;
		rs.polygonMode = VK_POLYGON_MODE_FILL;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rs.depthClampEnable = VK_FALSE;
		rs.rasterizerDiscardEnable = VK_FALSE;
		rs.depthBiasEnable = VK_FALSE;
		rs.depthBiasConstantFactor = 0;
		rs.depthBiasClamp = 0;
		rs.depthBiasSlopeFactor = 0;
		rs.lineWidth = 1.0f;

		VkPipelineColorBlendStateCreateInfo cb;
		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cb.flags = 0;
		cb.pNext = NULL;
		VkPipelineColorBlendAttachmentState att_state[1];
		att_state[0].colorWriteMask = 0xf;
		att_state[0].blendEnable = VK_FALSE;
		att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
		att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
		att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		cb.attachmentCount = 1;
		cb.pAttachments = att_state;
		cb.logicOpEnable = VK_FALSE;
		cb.logicOp = VK_LOGIC_OP_NO_OP;
		cb.blendConstants[0] = 1.0f;
		cb.blendConstants[1] = 1.0f;
		cb.blendConstants[2] = 1.0f;
		cb.blendConstants[3] = 1.0f;

		VkPipelineViewportStateCreateInfo vp = {};
		vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp.pNext = NULL;
		vp.flags = 0;
		VkViewport viewports;
		viewports.minDepth = 0.0f;
		viewports.maxDepth = 1.0f;
		viewports.x = 0;
		viewports.y = 0;
		int width = 0, height = 0;
		SDL_Vulkan_GetDrawableSize(gWindow, &width, &height);
		viewports.width = width;
		viewports.height = height;
		VkRect2D scissor;
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vp.viewportCount = 1;
		vp.scissorCount = 1;
		vp.pScissors = &scissor;
		vp.pViewports = &viewports;

		VkPipelineDepthStencilStateCreateInfo ds;
		ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds.pNext = NULL;
		ds.flags = 0;
		ds.depthTestEnable = VK_TRUE;
		ds.depthWriteEnable = VK_TRUE;
		ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		ds.depthBoundsTestEnable = VK_FALSE;
		ds.stencilTestEnable = VK_FALSE;
		ds.back.failOp = VK_STENCIL_OP_KEEP;
		ds.back.passOp = VK_STENCIL_OP_KEEP;
		ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
		ds.back.compareMask = 0;
		ds.back.reference = 0;
		ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
		ds.back.writeMask = 0;
		ds.minDepthBounds = 0;
		ds.maxDepthBounds = 0;
		ds.stencilTestEnable = VK_FALSE;
		ds.front = ds.back;

		VkPipelineMultisampleStateCreateInfo ms;
		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.pNext = NULL;
		ms.flags = 0;
		ms.pSampleMask = NULL;
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		ms.sampleShadingEnable = VK_FALSE;
		ms.alphaToCoverageEnable = VK_FALSE;
		ms.alphaToOneEnable = VK_FALSE;
		ms.minSampleShading = 0.0;

		VkShaderModule shaders[2];
		auto v_code = ReadFile("resources/textured.vert.spv");
		auto f_code = ReadFile("resources/textured.frag.spv");
		gEngine->CreateShaderModule(shaders[0], (uint32_t*)v_code.data(), v_code.size());
		gEngine->CreateShaderModule(shaders[1], (uint32_t*)f_code.data(), f_code.size());
		VkPipelineShaderStageCreateInfo shader_stages[2];
		shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shader_stages[0].module = shaders[0];
		shader_stages[0].pName = "main";
		shader_stages[0].flags = 0;
		shader_stages[0].pNext = nullptr;
		shader_stages[0].pSpecializationInfo = nullptr;

		shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_stages[1].module = shaders[1];
		shader_stages[1].pName = "main";
		shader_stages[1].flags = 0;
		shader_stages[1].pNext = nullptr;
		shader_stages[1].pSpecializationInfo = nullptr;

		VkPipelineCacheCreateInfo cache_info;
		cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		cache_info.pNext = NULL;
		cache_info.initialDataSize = 0;
		cache_info.pInitialData = NULL;
		cache_info.flags = 0;
		auto res = vkCreatePipelineCache(gEngine->device, &cache_info, NULL, &cache);
		assert(res == VK_SUCCESS);

		VkGraphicsPipelineCreateInfo pipeline_info;
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.pNext = NULL;
		pipeline_info.layout = pipeline_layout;
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_info.basePipelineIndex = 0;
		pipeline_info.flags = 0;
		pipeline_info.pVertexInputState = &vi;
		pipeline_info.pInputAssemblyState = &ia;
		pipeline_info.pRasterizationState = &rs;
		pipeline_info.pColorBlendState = &cb;
		pipeline_info.pTessellationState = nullptr;
		pipeline_info.pMultisampleState = &ms;
		pipeline_info.pDynamicState = &dynamicState;
		pipeline_info.pViewportState = &vp;
		pipeline_info.pDepthStencilState = &ds;
		pipeline_info.pStages = shader_stages;
		pipeline_info.stageCount = 2;
		pipeline_info.renderPass = renderpass;
		pipeline_info.subpass = 0;

		res = vkCreateGraphicsPipelines(gEngine->device, cache, 1, &pipeline_info, NULL, &pipeline);
		assert(res == VK_SUCCESS);

		vkDestroyShaderModule(gEngine->device, shaders[0], nullptr);
		vkDestroyShaderModule(gEngine->device, shaders[1], nullptr);
	}

	void Build() {
		CreateCmdPool();
		CreateRenderpass();
		CreateFramebuffers();
		CreateImage("resources/wall.jpg");
		CreateSampler();
		CreateUniformBuffer();

		float vertices[] = {
	   -1.0f, -1.0f, -1.0f, 1.0f,
	   -1.0f,  1.0f,  1.0f, 1.0f,
	   -1.0f, -1.0f,  1.0f, 1.0f,
	   -1.0f,  1.0f, -1.0f, 1.0f,
		};
		float uvs[] = {
		0.0f,  0.0f,
		0.0f,  1.0f,
	    1.0f,  1.0f,
	    1.0f,  0.0f
		};
		uint32_t indices[] = {
			0, 1, 2,
			1, 0, 3 
		};
		AddVertexBuffer(vertices, sizeof(vertices));
		AddVertexBuffer(uvs, sizeof(uvs));
		VertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
		VertexAttribute(1, 1, VK_FORMAT_R32G32_SFLOAT, 0);
		VertexBinding(0, sizeof(float) * 4);
		VertexBinding(1, sizeof(float) * 2);
		CreateIndexBuffer(indices, sizeof(indices));
		CreateLayout();
		CreateDescriptorPool();
		CreateDescriptorSet();
		CreatePipeline();

		int width = 0, height = 0;
		SDL_Vulkan_GetDrawableSize(gWindow, &width, &height);
		
		for (uint32_t i = 0; i < gEngine->swapchain_image_count; i++) {
			auto cmd = GenCmd(false, true);

			VkCommandBufferBeginInfo cmd_begin_info{};
			cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			vkBeginCommandBuffer(cmd, &cmd_begin_info);

			VkClearValue clear_values[2];
			clear_values[0].color.float32[0] = 0.2f;
			clear_values[0].color.float32[1] = 0.2f;
			clear_values[0].color.float32[2] = 0.2f;
			clear_values[0].color.float32[3] = 0.2f;
			clear_values[1].depthStencil.depth = 1.0f;
			clear_values[1].depthStencil.stencil = 0;

			VkRenderPassBeginInfo rp_begin;
			rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			rp_begin.pNext = nullptr;
			rp_begin.renderPass = renderpass;
			rp_begin.framebuffer = framebuffers[i];
			rp_begin.renderArea.offset.x = 0;
			rp_begin.renderArea.offset.y = 0;
			rp_begin.renderArea.extent.width = width;
			rp_begin.renderArea.extent.height = height;
			rp_begin.clearValueCount = 2;
			rp_begin.pClearValues = clear_values;

			vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			std::vector<VkBuffer> _buffers{};
			for (auto& buf : vertex_buffers) {
				_buffers.push_back(buf.buffer);
			}
			std::vector<VkDeviceSize> _offsets(vertex_buffers.size(), 0);
			vkCmdBindVertexBuffers(cmd, 0, _buffers.size(), _buffers.data(), _offsets.data());
			vkCmdBindIndexBuffer(cmd, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

			vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

			vkCmdEndRenderPass(cmd);

			vkEndCommandBuffer(cmd);
		}
	}

	void Draw() {
		assert(gEngine);
		if (!gEngine->NextImage()) {
			return;
		}
		VkResult res;

		VkPipelineStageFlags pipe_stage_flags;
		VkSubmitInfo submit_info;
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = nullptr;
		submit_info.pWaitDstStageMask = &pipe_stage_flags;
		pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &gEngine->image_acquired_semaphore;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmds[gEngine->current_buffer];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &gEngine->draw_complete_semaphore;
		res = vkQueueSubmit(gEngine->graphics_queue, 1, &submit_info, gEngine->fence);
		assert(res == VK_SUCCESS);

		vkWaitForFences(gEngine->device, 1, &gEngine->fence, VK_TRUE, UINT64_MAX);
		vkResetFences(gEngine->device, 1, &gEngine->fence);
		
		VkPresentInfoKHR present;
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.pNext = nullptr;
		present.swapchainCount = 1;
		present.pSwapchains = &gEngine->swapchain;
		present.pImageIndices = &gEngine->current_buffer;
		present.waitSemaphoreCount = 1;
		present.pWaitSemaphores = &gEngine->draw_complete_semaphore;
		present.pResults = nullptr;

		res = vkQueuePresentKHR(gEngine->present_queue, &present);
		assert(res == VK_SUCCESS);
	}

	void Update(float x, float y, float z) {
		int width = 0, height = 0;
		SDL_Vulkan_GetDrawableSize(gWindow, &width, &height);

		float fov = glm::radians(45.0f);
		if (width > height && width != 0 && height != 0) {
			fov *= static_cast<float>(height) / static_cast<float>(width);
		}
		auto projection = glm::perspective(fov, static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
		auto view = glm::lookAt(glm::vec3(x, y, z),  // Camera is at (-5,3,-10), in World Space
			glm::vec3(0, 0, 0),     // and looks at the origin
			glm::vec3(0, -1, 0)     // Head is up (set to 0,-1,0 to look upside-down)
		);
		auto model = glm::mat4(1.0f);
		// Vulkan clip space has inverted Y and half Z.
		auto clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);

		auto MVP = clip * projection * view * model;
		void* dest_data = nullptr;
		vkMapMemory(gEngine->device, uniform_buffer.memory, 0, sizeof(MVP), 0, &dest_data);
		memcpy(dest_data, &MVP, sizeof(MVP));
		vkUnmapMemory(gEngine->device, uniform_buffer.memory);
	}

	void Clear() {
		assert(gEngine);
		auto& device = gEngine->device;
		vkDeviceWaitIdle(device);
		vkDestroyRenderPass(device, renderpass, nullptr);
		for (auto& fb : framebuffers) {
			vkDestroyFramebuffer(device, fb, nullptr);
		}
		vkFreeCommandBuffers(device, cmd_pool, cmds.size(), cmds.data());
		vkDestroyCommandPool(device, cmd_pool, nullptr);
		vkDestroyPipelineCache(device, cache, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
		// vkFreeDescriptorSets(device, descriptor_pool, 1, &descriptor_set);
		vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
		vkDestroyBuffer(device, index_buffer.buffer, nullptr);
		vkFreeMemory(device, index_buffer.memory, nullptr);
		vkDestroyBuffer(device, uniform_buffer.buffer, nullptr);
		vkFreeMemory(device, uniform_buffer.memory, nullptr);
		for (auto& buffer : vertex_buffers) {
			vkDestroyBuffer(device, buffer.buffer, nullptr);
			vkFreeMemory(device, buffer.memory, nullptr);
		}
		vkDestroySampler(device, sampler, nullptr);
		vkDestroyImage(device, sampled_image.image, nullptr);
		vkDestroyImageView(device, sampled_image.view, nullptr);
		vkFreeMemory(device, sampled_image.memory, nullptr);
	}
};

Program * CreateProgram() {
	Program* program = new Program();
	return program;
}

void DestoryProgram(Program* program) {
	program->Clear();
	delete program;
}

void Build(Program* program) {
	program->Build();
}

void Draw(Program* program) {
	program->Draw();
}

void Update(Program* program, float x, float y, float z) {
	program->Update(x, y, z);
}

}