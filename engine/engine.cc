#include "engine.h"

#include <iostream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <map>

#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

namespace marble {

#ifdef NDEBUG
const bool kEnableValidationLayers = false;
#else
const bool kEnableValidationLayers = true;
#endif

extern SDL_Window* gWindow;

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

Engine::Engine() {
	
}

Engine::~Engine() {
	
}

void Engine::Init() {
	CreateInstance();
	SetupDebugMessenger();
	InitGpuInfo();
	CreateSurface();
	CreateDevice();
	InitQueue();
	CreateSwapchain();
	CreateDepthImage();
	CreateFenceAndSemaphore();
	CreateCmdPool();
}

void Engine::Quit() {
	DestroyCmdPool();
	DestroyFenceAndSemaphore();
	DestroyDepthImage();
	DestroySwapchain();
	DestroyDevice();
	DestroySurface();
	UninstallDebugMessenger();
	DestroyInstance();
}

bool Engine::GetMemoryType(uint32_t type_bits, VkFlags mask, uint32_t & type_index) {
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

bool Engine::NextImage() {
	auto res = vkAcquireNextImageKHR(device, swapchain, 2000000, image_acquired_semaphore, VK_NULL_HANDLE, &current_buffer);
	if (res != VK_SUCCESS) {
		return false;
	}
	return true;
}

void Engine::SetImageLayout(VkImage image, VkImageAspectFlags aspect_mask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
	auto cmd = BeginOnceCmd();

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

	static std::map<VkImageLayout, VkAccessFlags> old_image_layout_map = {
		{ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT },
		{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT },
		{ VK_IMAGE_LAYOUT_PREINITIALIZED, VK_ACCESS_HOST_WRITE_BIT },
	};

	static std::map<VkImageLayout, VkAccessFlags> new_image_layout_map = {
		{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT },
		{ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT },
		{ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT },
		{ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT },
		{ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT },
	};

	image_memory_barrier.srcAccessMask = old_image_layout_map[old_image_layout];
	image_memory_barrier.dstAccessMask = new_image_layout_map[new_image_layout];

	vkCmdPipelineBarrier(cmd, src_stages, dest_stages, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

	EndOnceCmd(cmd);
}

void Engine::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties, VkBuffer & buffer, VkDeviceMemory & memory) {

	VkBufferCreateInfo buffer_info{};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	auto res = vkCreateBuffer(device, &buffer_info, nullptr, &buffer);
	assert(VK_SUCCESS == res);

	VkMemoryRequirements mem_req;
	vkGetBufferMemoryRequirements(device, buffer, &mem_req);

	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	bool pass = GetMemoryType(mem_req.memoryTypeBits, properties, alloc_info.memoryTypeIndex);
	assert(pass);

	res = vkAllocateMemory(device, &alloc_info, nullptr, &memory);
	assert(VK_SUCCESS == res);

	res = vkBindBufferMemory(device, buffer, memory, 0);
	assert(VK_SUCCESS == res);
}

void Engine::DestroyBuffer(VkBuffer & buffer, VkDeviceMemory & memory) {
	vkDestroyBuffer(device, buffer, nullptr);
	vkFreeMemory(device, memory, nullptr);
}

void Engine::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage & image, VkDeviceMemory & memory) {
	VkResult res;

	VkImageCreateInfo image_info{};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = mipLevels;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	res = vkCreateImage(device, &image_info, nullptr, &image);
	assert(VK_SUCCESS == res);

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(device, image, &mem_req);

	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	auto pass = GetMemoryType(mem_req.memoryTypeBits, properties, alloc_info.memoryTypeIndex);
	assert(pass);

	res = vkAllocateMemory(device, &alloc_info, nullptr, &memory);
	assert(VK_SUCCESS == res);

	res = vkBindImageMemory(device, image, memory, 0);
	assert(VK_SUCCESS == res);
}

void Engine::DestroyImage(VkImage & image, VkDeviceMemory & memory) {
	vkDestroyImage(device, image, nullptr);
	vkFreeMemory(device, memory, nullptr);
}

void Engine::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t miplevels, VkImageView & view) {
	VkImageViewCreateInfo view_info{};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = format;
	view_info.subresourceRange.aspectMask = aspect_flags;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = miplevels;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;

	auto res = vkCreateImageView(device, &view_info, nullptr, &view);
	assert(VK_SUCCESS == res);
}

void Engine::DestroyImageView(VkImageView & view) {
	vkDestroyImageView(device, view, nullptr);
}

void Engine::CopyBuffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size) {
	auto cmd = BeginOnceCmd();

	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0;
	copy_region.dstOffset = 0;
	copy_region.size = size;
	vkCmdCopyBuffer(cmd, src_buf, dst_buf, 1, &copy_region);

	EndOnceCmd(cmd);
}

void Engine::CopyData(VkDeviceMemory & memory, void * data, size_t size) {
	void* dest_data;
	vkMapMemory(device, memory, 0, size, 0, &dest_data);
	memcpy(dest_data, data, size);
	vkUnmapMemory(device, memory);
}

void Engine::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	auto cmd = BeginOnceCmd();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};
	vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	EndOnceCmd(cmd);
}

void Engine::CreateShaderModule(VkShaderModule & shader_module, uint32_t * data, size_t size) {
	VkShaderModuleCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = size;
	info.pCode = data;
	vkCreateShaderModule(device, &info, nullptr, &shader_module);
}

void Engine::DestroyShaderModule(VkShaderModule & shader_module) {
	vkDestroyShaderModule(device, shader_module, nullptr);
}

VkCommandBuffer Engine::GenCmd(bool secondary, bool join) {
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = cmd_pool;
	alloc_info.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VkCommandBuffer cmd;
	auto res = vkAllocateCommandBuffers(device, &alloc_info, &cmd);
	if (res != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}
	if (join) {
		cmds.push_back(cmd);
	}
	return cmd;
}

void Engine::FreeCmd(VkCommandBuffer & cmd) {
	vkFreeCommandBuffers(device, cmd_pool, 1, &cmd);
}

void Engine::CreateInstance() {
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

void Engine::DestroyInstance() {
	vkDestroyInstance(instance, nullptr);
}

void Engine::SetupDebugMessenger() {
	if (!kEnableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT create_info;
	PopulateDebugMessengerCreateInfo(create_info);

	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, &create_info, nullptr, &debug_messenger);
	}
}

void Engine::UninstallDebugMessenger() {
	if (!kEnableValidationLayers) return;
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debug_messenger, nullptr);
	}
}

void Engine::InitGpuInfo() {
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

void Engine::CreateSurface() {
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

void Engine::DestroySurface() {
	vkDestroySurfaceKHR(instance, surface, nullptr);
}

void Engine::CreateDevice() {
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

void Engine::DestroyDevice() {
	vkDestroyDevice(device, nullptr);
}

void Engine::InitQueue() {
	vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);
	if (graphics_queue_family_index == present_queue_family_index) {
		present_queue = graphics_queue;
	} else {
		vkGetDeviceQueue(device, present_queue_family_index, 0, &present_queue);
	}
}

void Engine::CreateSwapchain() {
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

void Engine::DestroySwapchain() {
	for (const auto& d : swapchain_images) {
		vkDestroyImageView(device, d.view, nullptr);
	}
	swapchain_images.clear();
	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void Engine::CreateDepthImage() {
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
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
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

void Engine::DestroyDepthImage() {
	vkDestroyImage(device, depth_image.image, nullptr);
	vkDestroyImageView(device, depth_image.view, nullptr);
	vkFreeMemory(device, depth_image.memory, nullptr);
}

void Engine::CreateFenceAndSemaphore() {
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

void Engine::DestroyFenceAndSemaphore() {
	vkDestroyFence(device, fence, nullptr);
	vkDestroySemaphore(device, draw_complete_semaphore, nullptr);
	vkDestroySemaphore(device, image_acquired_semaphore, nullptr);
}

void Engine::CreateCmdPool() {
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = graphics_queue_family_index;

	auto res = vkCreateCommandPool(device, &pool_info, nullptr, &cmd_pool);
	assert(VK_SUCCESS == res);
}

void Engine::DestroyCmdPool() {
	if (!cmds.empty()) {
		vkFreeCommandBuffers(device, cmd_pool, (uint32_t)cmds.size(), cmds.data());
	}
	vkDestroyCommandPool(device, cmd_pool, nullptr);
}

VkCommandBuffer Engine::BeginOnceCmd() {
	auto cmd = GenCmd(false, false);

	VkCommandBufferBeginInfo cmd_begin_info = {};
	cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmd_begin_info.pNext = NULL;
	cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmd_begin_info.pInheritanceInfo = NULL;

	vkBeginCommandBuffer(cmd, &cmd_begin_info);
	return cmd;
}

void Engine::EndOnceCmd(VkCommandBuffer & cmd) {
	VkResult res;
	res = vkEndCommandBuffer(cmd);
	assert(VK_SUCCESS == res);

	const VkCommandBuffer cmd_bufs[] = { cmd };
	VkFenceCreateInfo fenceInfo;
	VkFence cmdFence;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = 0;
	res = vkCreateFence(device, &fenceInfo, NULL, &cmdFence);
	assert(VK_SUCCESS == res);

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

	res = vkQueueSubmit(graphics_queue, 1, submit_info, cmdFence);
	assert(res == VK_SUCCESS);

	vkWaitForFences(device, 1, &cmdFence, VK_TRUE, UINT64_MAX);

	vkDestroyFence(device, cmdFence, nullptr);
	FreeCmd(cmd);
}

Engine & GetEngine() {
	static Engine sEngine{};
	return sEngine;
}

}