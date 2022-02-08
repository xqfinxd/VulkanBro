#pragma once

#include <cstdlib>
#include <vulkan/vulkan.h>

namespace marble {

struct Buffer {
public:
	Buffer();

	void BindBuffer(uint32_t usage);

	void BufferData(size_t size, void* data);

	void Clear();

	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size{ 0 };
	VkDeviceSize offset{ 0 };
	VkBufferUsageFlagBits usage;


};

}