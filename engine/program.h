#pragma once

#include <vector>
#include <map>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "engine.h"
#include "element.h"

namespace marble {

struct Program {
public:
	Program();

	void Build();

	void Draw();

	void Update(float x, float y, float z);

	void Clear();

	std::vector<VkCommandBuffer> cmds;
	VkRenderPass renderpass;
	std::vector<VkFramebuffer> framebuffers;
	VkDescriptorPool descriptor_pool;

	Buffer uniform_buffer;
	Texture sampled_image;
	VkSampler sampler;

	std::vector<Buffer> vertex_buffers;
	Buffer index_buffer;

	std::vector<VkVertexInputBindingDescription> vertex_bindings;
	std::vector<VkVertexInputAttributeDescription> vertex_attributes;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet descriptor_set;
	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;
	VkPipelineCache cache;

	std::map<VkShaderStageFlagBits, VkShaderModule> shader_modules;

private:

	void CreateRenderpass();

	void CreateFramebuffers();

	void CreateDescriptorPool();

	void CreateUniformBuffer();

	void CreateImage(const char* image);

	void CreateSampler();

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
void Build(Program*);
void Draw(Program*);
void Update(Program*, float x, float y, float z);

}