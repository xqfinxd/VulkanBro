#pragma once

#include <vulkan/vulkan.hpp>

struct QueueFamilyIndices {
	uint32_t graphicsFamily{ UINT32_MAX };
	uint32_t presentFamily{ UINT32_MAX };

	bool isComplete() {
		return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class MEngine {
public:
	void AttachDevice();
	void DetachDevice();

	void DrawFrame();

private:
	VkInstance instance_;
	VkDebugUtilsMessengerEXT debug_messenger_;
	VkSurfaceKHR surface_;

	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice device_;

	VkQueue graphics_queue_;
	VkQueue present_queue_;

	VkSwapchainKHR swapchain_;
	std::vector<VkImage> swapchain_images_;
	VkFormat swapchain_image_format_;
	VkExtent2D swapchain_extent_;
	std::vector<VkImageView> swapchain_image_views_;
	std::vector<VkFramebuffer> swapchain_framebuffers_;

	VkRenderPass renderpass_;
	VkPipelineLayout pipeline_layout_;
	VkPipeline graphics_pipeline_;

	VkCommandPool command_pool_;
	std::vector<VkCommandBuffer> command_buffers_;

	std::vector<VkSemaphore> image_available_semaphores_;
	std::vector<VkSemaphore> render_finished_semaphores_;
	std::vector<VkFence> inflight_fences_;
	std::vector<VkFence> images_inflight_;
	size_t current_frame_ = 0;

private:
	void createInstance();
	void setupDebugMessenger();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	void createSwapChain();
	void createImageViews();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSyncObjects();

	VkShaderModule createShaderModule(const std::vector<char>& code);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	bool isDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	std::vector<const char*> getRequiredExtensions();
	bool checkValidationLayerSupport();
};