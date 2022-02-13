#include "program.h"

#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "stb_image.h"

namespace marble {

extern SDL_Window* gWindow;
extern std::vector<char> ReadFile(const char* filename);

Program::Program() {
	
}

void Program::GetLayout() {
	CreateLayout();
	CreateDescriptorPool();
	CreateDescriptorSet();
}

void Program::CreateRenderpass() {
	VkAttachmentDescription attachments[2];
	attachments[0].format = GetEngine().format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].flags = 0;

	attachments[1].format = GetEngine().depth_image.format;
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

	auto res = vkCreateRenderPass(GetEngine().device, &rp_info, NULL, &renderpass);
	assert(res == VK_SUCCESS);
}

void Program::CreateFramebuffers() {
	VkImageView attachments[2];
	attachments[1] = GetEngine().depth_image.view;

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
	framebuffers.resize(GetEngine().swapchain_image_count);
	for (uint32_t i = 0; i < GetEngine().swapchain_image_count; i++) {
		attachments[0] = GetEngine().swapchain_images[i].view;
		res = vkCreateFramebuffer(GetEngine().device, &fb_info, NULL, &framebuffers[i]);
		assert(res == VK_SUCCESS);
	}
}

void Program::CreateDescriptorPool() {
	std::map<VkDescriptorType, uint32_t> type_counts{};
	for (const auto& sl : shader_layouts) {
		type_counts[sl.second.descriptorType] += sl.second.descriptorCount;
	}

	std::vector<VkDescriptorPoolSize> pool_sizes{};
	for (const auto& tc : type_counts) {
		VkDescriptorPoolSize ps;
		ps.type = tc.first;
		ps.descriptorCount = tc.second;
		pool_sizes.push_back(ps);
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.pNext = nullptr;
	pool_info.maxSets = 1;
	pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
	pool_info.pPoolSizes = pool_sizes.data();

	auto res = vkCreateDescriptorPool(GetEngine().device, &pool_info, nullptr, &descriptor_pool);
	assert(res == VK_SUCCESS);
}

void Program::BindUniformBuffer(uint32_t set, uint32_t binding, const Buffer& buffer, uint32_t size) {
	assert(set < descriptor_set.size());
	VkDescriptorBufferInfo buffer_info;
	buffer_info.buffer = buffer.buffer;
	buffer_info.offset = 0;
	buffer_info.range = size;
	VkWriteDescriptorSet write;
	write.pNext = nullptr;
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set[set];
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.pImageInfo = nullptr;
	write.pBufferInfo = &buffer_info;
	write.pTexelBufferView = nullptr;
	vkUpdateDescriptorSets(GetEngine().device, 1, &write, 0, nullptr);
}

void Program::BindTexture(uint32_t set, uint32_t binding, const Texture & tex) {
	assert(set < descriptor_set.size());
	VkDescriptorImageInfo image_info;
	image_info.imageView = tex.view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.sampler = tex.sampler;
	VkWriteDescriptorSet write;
	write.pNext = nullptr;
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set[set];
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &image_info;
	write.pBufferInfo = nullptr;
	write.pTexelBufferView = nullptr;
	vkUpdateDescriptorSets(GetEngine().device, 1, &write, 0, nullptr);
}

void Program::AddShaderUniform(uint32_t set, uint32_t binding, uint32_t stage, uint32_t type) {
	Location loc;
	loc.set = set;
	loc.binding = binding;
	auto& bindings = shader_layouts[loc.key];
	bindings.binding = binding;
	bindings.descriptorCount = 1;
	bindings.pImmutableSamplers = nullptr;
	bindings.stageFlags = (VkShaderStageFlags)stage;
	bindings.descriptorType = (VkDescriptorType)type;
}

void Program::AddShaderUniform(uint32_t set, uint32_t binding) {
	Location loc;
	loc.set = set;
	loc.binding = binding;
	auto iter = shader_layouts.find(loc.key);
	if (shader_layouts.end() != iter) {
		iter->second.descriptorCount++;
	}
}

void Program::AddVertexBuffer(float * data, size_t size) {
	Buffer v_buffer;
	v_buffer.BindBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	v_buffer.BufferData(size, data);
	vertex_buffers.push_back(v_buffer);
}

void Program::CreateIndexBuffer(uint32_t * data, size_t size) {
	index_buffer.BindBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	index_buffer.BufferData(size, data);
}

void Program::VertexBinding(uint32_t binding, uint32_t stride) {
	VkVertexInputBindingDescription binding_desc{};
	binding_desc.binding = binding;
	binding_desc.stride = stride;
	binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertex_bindings.push_back(binding_desc);
}

void Program::VertexAttribute(uint32_t binding, uint32_t location, uint32_t fmt, uint32_t offset) {
	VkVertexInputAttributeDescription attr_desc{};
	attr_desc.binding = binding;
	attr_desc.location = location;
	attr_desc.format = (VkFormat)fmt;
	attr_desc.offset = offset;
	vertex_attributes.push_back(attr_desc);
}

void Program::CreateLayout() {
	VkResult res;

	uint32_t set_num = 0;
	{
		auto last = shader_layouts.crbegin();
		if (last != shader_layouts.crend()) {
			Location last_loc;
			last_loc.key = last->first;
			set_num = last_loc.set + 1;
		}
	}

	VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
	descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_layout.pNext = nullptr;
	descriptor_layout.flags = 0;
	descriptor_layout.bindingCount = 0;
	descriptor_layout.pBindings = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> set_bindings{};
	descriptor_set_layout.resize(set_num);
	for (uint32_t i = 0; i < set_num; i++) {
		set_bindings.clear();
		for (const auto& sl : shader_layouts) {
			Location loc;
			loc.key = sl.first;
			if (loc.set == i) {
				set_bindings.push_back(sl.second);
			}
		}
		descriptor_layout.bindingCount = (uint32_t)set_bindings.size();
		descriptor_layout.pBindings = set_bindings.data();
		res = vkCreateDescriptorSetLayout(GetEngine().device, &descriptor_layout, nullptr, &descriptor_set_layout[i]);
		assert(res == VK_SUCCESS);
	}

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.pNext = NULL;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = NULL;
	pipeline_layout_info.setLayoutCount = (uint32_t)descriptor_set_layout.size();
	pipeline_layout_info.pSetLayouts = descriptor_set_layout.data();

	res = vkCreatePipelineLayout(GetEngine().device, &pipeline_layout_info, NULL, &pipeline_layout);
	assert(res == VK_SUCCESS);
}

void Program::CreateDescriptorSet() {
	descriptor_set.resize(descriptor_set_layout.size());

	VkDescriptorSetAllocateInfo alloc_info[1];
	alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info[0].pNext = nullptr;
	alloc_info[0].descriptorPool = descriptor_pool;
	alloc_info[0].descriptorSetCount = (uint32_t)descriptor_set_layout.size();
	alloc_info[0].pSetLayouts = descriptor_set_layout.data();

	auto res = vkAllocateDescriptorSets(GetEngine().device, alloc_info, descriptor_set.data());
	assert(res == VK_SUCCESS);
}

void Program::AddShader(uint32_t stage, const char * filename) {
	auto stage_bit = (VkShaderStageFlagBits)stage;
	if (shader_modules.count(stage_bit)) {
		return;
	}
	VkShaderModule shader;
	auto shader_code = ReadFile(filename);
	GetEngine().CreateShaderModule(shader, (uint32_t*)shader_code.data(), shader_code.size());
	shader_modules.insert(std::make_pair(stage_bit, shader));
}

void Program::RemoveShader(uint32_t stage) {
	auto stage_bit = (VkShaderStageFlagBits)stage;
	auto shader_iter = shader_modules.find(stage_bit);
	if (shader_modules.end() == shader_iter) {
		return;
	}
	GetEngine().DestroyShaderModule(shader_iter->second);
	shader_modules.erase(shader_iter);
}

void Program::CreatePipeline() {
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
	vi.vertexBindingDescriptionCount = (uint32_t)vertex_bindings.size();
	vi.pVertexBindingDescriptions = vertex_bindings.data();
	vi.vertexAttributeDescriptionCount = (uint32_t)vertex_attributes.size();
	vi.pVertexAttributeDescriptions = vertex_attributes.data();
	VkPipelineInputAssemblyStateCreateInfo ia;
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.pNext = NULL;
	ia.flags = 0;
	ia.primitiveRestartEnable = VK_FALSE;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

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
	viewports.width = 1.f * width;
	viewports.height = 1.f * height;
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

	std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
	for (const auto& sm : shader_modules) {
		VkPipelineShaderStageCreateInfo stage;
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.stage = sm.first;
		stage.module = sm.second;
		stage.pName = "main";
		stage.flags = 0;
		stage.pNext = nullptr;
		stage.pSpecializationInfo = nullptr;
		shader_stages.push_back(stage);
	}

	VkPipelineCacheCreateInfo cache_info;
	cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	cache_info.pNext = NULL;
	cache_info.initialDataSize = 0;
	cache_info.pInitialData = NULL;
	cache_info.flags = 0;
	auto res = vkCreatePipelineCache(GetEngine().device, &cache_info, NULL, &cache);
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
	pipeline_info.pStages = shader_stages.data();
	pipeline_info.stageCount = (uint32_t)shader_stages.size();
	pipeline_info.renderPass = renderpass;
	pipeline_info.subpass = 0;

	res = vkCreateGraphicsPipelines(GetEngine().device, cache, 1, &pipeline_info, NULL, &pipeline);
	assert(res == VK_SUCCESS);
}

void Program::Build() {
	CreateRenderpass();
	CreateFramebuffers();

	float vertices[] = {
		-1.0f,  1.0f,  1.0f, 1.0f,
		 1.0f,  1.0f,  1.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 1.0f,
		-1.0f, -1.0f,  1.0f, 1.0f,
	};
	float uvs[] = {
		0.0f,  0.0f,
		1.0f,  0.0f,
		1.0f,  1.0f,
		0.0f,  1.0f
	};
	uint32_t indices[] = {
		0, 1, 2,
		0, 3, 2
	};
	AddVertexBuffer(vertices, sizeof(vertices));
	AddVertexBuffer(uvs, sizeof(uvs));
	VertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
	VertexAttribute(1, 1, VK_FORMAT_R32G32_SFLOAT, 0);
	VertexBinding(0, sizeof(float) * 4);
	VertexBinding(1, sizeof(float) * 2);
	CreateIndexBuffer(indices, sizeof(indices));
	
	AddShader(0x00000001, "resources/textured.vert.spv");
	AddShader(0x00000010, "resources/textured.frag.spv");

	CreatePipeline();

	int width = 0, height = 0;
	SDL_Vulkan_GetDrawableSize(gWindow, &width, &height);

	for (uint32_t i = 0; i < GetEngine().swapchain_image_count; i++) {
		auto cmd = GetEngine().GenCmd(false, false);

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
		vkCmdBindVertexBuffers(cmd, 0, (uint32_t)_buffers.size(), _buffers.data(), _offsets.data());
		vkCmdBindIndexBuffer(cmd, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, (uint32_t)descriptor_set.size(), descriptor_set.data(), 0, nullptr);

		vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

		vkCmdEndRenderPass(cmd);

		vkEndCommandBuffer(cmd);

		cmds.push_back(cmd);
	}
}

void Program::Draw() {
	
	if (!GetEngine().NextImage()) {
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
	submit_info.pWaitSemaphores = &GetEngine().image_acquired_semaphore;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmds[GetEngine().current_buffer];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &GetEngine().draw_complete_semaphore;
	res = vkQueueSubmit(GetEngine().graphics_queue, 1, &submit_info, GetEngine().fence);
	assert(res == VK_SUCCESS);

	vkWaitForFences(GetEngine().device, 1, &GetEngine().fence, VK_TRUE, UINT64_MAX);
	vkResetFences(GetEngine().device, 1, &GetEngine().fence);

	VkPresentInfoKHR present;
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.pNext = nullptr;
	present.swapchainCount = 1;
	present.pSwapchains = &GetEngine().swapchain;
	present.pImageIndices = &GetEngine().current_buffer;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = &GetEngine().draw_complete_semaphore;
	present.pResults = nullptr;

	res = vkQueuePresentKHR(GetEngine().present_queue, &present);
	assert(res == VK_SUCCESS);
}

void Program::Clear() {
	
	auto& device = GetEngine().device;
	vkDeviceWaitIdle(device);
	for (const auto& sm : shader_modules) {
		vkDestroyShaderModule(GetEngine().device, sm.second, nullptr);
	}
	vkDestroyRenderPass(device, renderpass, nullptr);
	for (auto& fb : framebuffers) {
		vkDestroyFramebuffer(device, fb, nullptr);
	}
	for (auto& cmd : cmds) {
		GetEngine().FreeCmd(cmd);
	}
	vkDestroyPipelineCache(device, cache, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	for (auto& dsl : descriptor_set_layout) {
		vkDestroyDescriptorSetLayout(device, dsl, nullptr);
	}
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
	index_buffer.Clear();
	for (auto& buffer : vertex_buffers) {
		buffer.Clear();
	}
}

Program * CreateProgram() {
	Program* program = new Program();
	return program;
}

void DestoryProgram(Program* program) {
	program->Clear();
	delete program;
}

}