#pragma once

#include <cstdlib>
#include <vulkan/vulkan.h>

namespace marble {

struct Buffer {
public:
	Buffer();

	void BindBuffer(uint32_t usage);

	void BufferData(size_t size, void* data);

	void SetData(void* data, size_t size);

	void Clear();

	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size{ 0 };
	VkDeviceSize offset{ 0 };
	VkBufferUsageFlagBits usage;
};


struct Texture {
	Texture();

	void TexImage2D(uint32_t width, uint32_t height, uint32_t channel, void* data);
	void EnableSampler();

	void Clear();

	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	VkSampler sampler;
	uint32_t width{ 0 };
	uint32_t height{ 0 };
	uint32_t channel{ 0 };
	VkFormat format{ VK_FORMAT_UNDEFINED };
};

}