#pragma once

#include <vector>
#include <map>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "engine.h"
#include "element.h"

namespace marble {

union Location {
	struct {
		uint32_t binding;
		uint32_t set;
	};
	uint64_t key;
};

struct Program {
public:
	Program();

	void GetLayout();

	void Build();

	void Draw();

	void Clear();

	void BindUniformBuffer(uint32_t set, uint32_t binding, const Buffer& buffer, uint32_t size);

	void BindTexture(uint32_t set, uint32_t binding, const Texture& tex);

	void AddShaderUniform(uint32_t set, uint32_t binding, uint32_t stage, uint32_t type);
	void AddShaderUniform(uint32_t set, uint32_t binding);

	std::vector<VkCommandBuffer> cmds;
	VkRenderPass renderpass;
	std::vector<VkFramebuffer> framebuffers;
	VkDescriptorPool descriptor_pool;

	std::vector<Buffer> vertex_buffers;
	Buffer index_buffer;

	std::vector<VkVertexInputBindingDescription> vertex_bindings;
	std::vector<VkVertexInputAttributeDescription> vertex_attributes;
	std::map<uint64_t, VkDescriptorSetLayoutBinding> shader_layouts;

	std::vector<VkDescriptorSetLayout> descriptor_set_layout;
	std::vector<VkDescriptorSet> descriptor_set;
	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;
	VkPipelineCache cache;

	std::map<VkShaderStageFlagBits, VkShaderModule> shader_modules;

private:

	void CreateRenderpass();

	void CreateFramebuffers();

	void CreateDescriptorPool();

	void AddVertexBuffer(float* data, size_t size);

	void CreateIndexBuffer(uint32_t* data, size_t size);

	void VertexBinding(uint32_t binding, uint32_t stride);

	void VertexAttribute(uint32_t binding, uint32_t location, uint32_t fmt, uint32_t offset);

	void CreateLayout();

	void CreateDescriptorSet();

	void AddShader(uint32_t stage, const char* filename);

	void RemoveShader(uint32_t stage);

	void CreatePipeline();

};

Program* CreateProgram();
void DestoryProgram(Program*);

}