#include "element.h"

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

void Buffer::SetData(void * data, size_t size) {
	GetEngine().CopyData(memory, &data, size);
}

void Buffer::Clear() {
	GetEngine().DestroyBuffer(buffer, memory);
}

Texture::Texture() {

}

void Texture::TexImage2D(uint32_t width_, uint32_t height_, uint32_t channel_, void * data) {
	height = height_;
	width = width_;
	channel = channel_;

	switch (channel) {
	case 1: format = VK_FORMAT_R8_UNORM; break;
	case 2: format = VK_FORMAT_R8G8_UNORM; break;
	case 3: format = VK_FORMAT_R8G8B8_UNORM; break;
	case 4: format = VK_FORMAT_R8G8B8A8_UNORM; break;
	default: break;
	}

	VkDeviceSize image_size = width * height * channel;
	VkBuffer staging_buffer;
	VkDeviceMemory staging_memory;
	GetEngine().CreateBuffer(image_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer, staging_memory);

	GetEngine().CopyData(staging_memory, data, image_size);

	GetEngine().CreateImage(width, height, 1, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

	GetEngine().SetImageLayout(image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT);
	GetEngine().CopyBufferToImage(staging_buffer, image, width, height);
	GetEngine().SetImageLayout(image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	GetEngine().DestroyBuffer(staging_buffer, staging_memory);
	GetEngine().CreateImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, view);
}

void Texture::EnableSampler() {
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

	auto res = vkCreateSampler(GetEngine().device, &sampler_info, NULL, &sampler);
	assert(res == VK_SUCCESS);
}

void Texture::Clear() {
	GetEngine().DestroyImage(image, memory);
	GetEngine().DestroyImageView(view);
	vkDestroySampler(GetEngine().device, sampler, nullptr);
}

}