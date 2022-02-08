#include "buffer.h"

#include "engine.h"

namespace marble {

Buffer::Buffer() {

}

void Buffer::BindBuffer(uint32_t usage_) {
	usage = (VkBufferUsageFlagBits)usage_;
}

void Buffer::BufferData(size_t size_, void* data) {
	constexpr VkBufferUsageFlags staging_flag = 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	size = size_;
	if (staging_flag & usage) {
		VkMemoryPropertyFlags mem_prop = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VkBuffer staging_buffer;
		VkDeviceMemory staging_memory;
		GetEngine().CreateBuffer(size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staging_buffer, staging_memory);

		if (data) {
			GetEngine().CopyData(staging_memory, data, size);
		}

		GetEngine().CreateBuffer(size, 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			buffer, memory);

		GetEngine().CopyBuffer(staging_buffer, buffer, size);

		GetEngine().DestroyBuffer(staging_buffer, staging_memory);
	} else {
		VkMemoryPropertyFlags mem_prop = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		GetEngine().CreateBuffer(size, usage, mem_prop, buffer, memory);
		if (data) {
			GetEngine().CopyData(memory, data, size);
		}
	}
}

void Buffer::Clear() {
	GetEngine().DestroyBuffer(buffer, memory);
}

}