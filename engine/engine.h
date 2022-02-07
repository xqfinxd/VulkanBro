#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

/*
VertexShader					= 0x00000001,
TessellationControlShader		= 0x00000002,
TessellationEvaluationShader	= 0x00000004,
GeometryShader					= 0x00000008,
FragmentShader					= 0x00000010,
*/

namespace marble {

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
	VkCommandPool cmd_pool;
	std::vector<VkCommandBuffer> cmds;

	Engine();
	~Engine();

	void Init();
	void Quit();

	bool GetMemoryType(uint32_t type_bits, VkFlags mask, uint32_t &type_index);

	bool NextImage();

	void SetImageLayout(VkImage image, VkImageAspectFlags aspect_mask, VkImageLayout old_image_layout,
		VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages);

	void CreateShaderModule(VkShaderModule& shader_module, uint32_t* data, size_t size);
	void DestroyShaderModule(VkShaderModule& shader_module);

	VkCommandBuffer GenCmd(bool secondary, bool join);
	void FreeCmd(VkCommandBuffer& cmd);

private:
	void CreateInstance();
	void DestroyInstance();

	void SetupDebugMessenger();
	void UninstallDebugMessenger();

	void InitGpuInfo();

	void CreateSurface();
	void DestroySurface();

	void CreateDevice();
	void DestroyDevice();

	void InitQueue();

	void CreateSwapchain();
	void DestroySwapchain();

	void CreateDepthImage();
	void DestroyDepthImage();

	void CreateFenceAndSemaphore();
	void DestroyFenceAndSemaphore();

	void CreateCmdPool();
	void DestroyCmdPool();

};

Engine& GetEngine();

}