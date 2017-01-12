﻿/*
* Vulkan Example - Instanced mesh rendering, uses a separate vertex buffer for instanced data
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h> 
#include <vector>
#include <random>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.hpp>
#include "vulkanClasses/vulkanApp.h"

#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1
#define ENABLE_VALIDATION true
#define INSTANCE_COUNT 8192

// Vertex layout for this example
std::vector<vkx::VertexLayout> vertexLayout =
{
	vkx::VertexLayout::VERTEX_LAYOUT_POSITION,
	vkx::VertexLayout::VERTEX_LAYOUT_NORMAL,
	vkx::VertexLayout::VERTEX_LAYOUT_UV,
	vkx::VertexLayout::VERTEX_LAYOUT_COLOR
};

class VulkanExample : public vkx::vulkanApp
{
public:
	struct {
		vk::PipelineVertexInputStateCreateInfo inputState;
		std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
		std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct {
		vkx::MeshBuffer example;
	} meshes;

	struct {
		vkx::Texture colorMap;
	} textures;

	// Per-instance data block
	struct InstanceData {
		glm::vec3 pos;
		glm::vec3 rot;
		float scale;
		uint32_t texIndex;
	};

	// Contains the instanced data
	struct {
		vk::Buffer buffer = VK_NULL_HANDLE;
		vk::DeviceMemory memory = VK_NULL_HANDLE;
		size_t size = 0;
		vk::DescriptorBufferInfo descriptor;
	} instanceBuffer;

	struct {
		glm::mat4 projection;
		glm::mat4 view;
		float time = 0.0f;
	} uboVS;

	struct {
		vkx::UniformData vsScene;
	} uniformData;

	struct {
		vk::Pipeline solid;
	} pipelines;

	vk::PipelineLayout pipelineLayout;
	vk::DescriptorSet descriptorSet;
	vk::DescriptorSetLayout descriptorSetLayout;

	VulkanExample() : vulkanApp(ENABLE_VALIDATION)
	{
		zoom = -12.0f;
		rotationSpeed = 0.25f;
		enableTextOverlay = true;
		title = "Vulkan Example - Instanced mesh rendering";
		srand(time(NULL));
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipelines.solid, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyBuffer(device, instanceBuffer.buffer, nullptr);
		vkFreeMemory(device, instanceBuffer.memory, nullptr);
		vkx::freeMeshBufferResources(device, &meshes.example);
		vkx::destroyUniformData(device, &uniformData.vsScene);
		textureLoader->destroyTexture(textures.colorMap);
	}

	void buildCommandBuffers()
	{
		vk::CommandBufferBeginInfo cmdBufInfo;

		vk::ClearValue clearValues[2];
		//clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[0].color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
		clearValues[1].depthStencil = { 1.0f, 0 };

		vk::RenderPassBeginInfo renderPassBeginInfo;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.extent.width = size.width;
		renderPassBeginInfo.renderArea.extent.height = size.height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = framebuffers[i];

			//VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			drawCmdBuffers[i].begin(&cmdBufInfo);

			//vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			drawCmdBuffers[i].beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);

			vk::Viewport viewport = vkx::viewport((float)size.width, (float)size.height, 0.0f, 1.0f);
			//vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			drawCmdBuffers[i].setViewport(0, 1, &viewport);

			vk::Rect2D scissor = vkx::rect2D(size.width, size.height, 0, 0);
			//vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
			drawCmdBuffers[i].setScissor(0, 1, &scissor);

			//vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			drawCmdBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			//vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solid);
			drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.solid);

			vk::DeviceSize offsets[1] = { 0 };
			// Binding point 0 : Mesh vertex buffer
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.example.vertices.buf, offsets);
			drawCmdBuffers[i].bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1, &meshes.example.vertices.buf, offsets);
			// Binding point 1 : Instance data buffer
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer.buffer, offsets);
			drawCmdBuffers[i].bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer.buffer, offsets);

			//vkCmdBindIndexBuffer(drawCmdBuffers[i], meshes.example.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			drawCmdBuffers[i].bindIndexBuffer(meshes.example.indices.buf, 0, vk::IndexType::eUint32);

			// Render instances
			//vkCmdDrawIndexed(drawCmdBuffers[i], meshes.example.indexCount, INSTANCE_COUNT, 0, 0, 0);
			drawCmdBuffers[i].drawIndexed(meshes.example.indexCount, INSTANCE_COUNT, 0, 0, 0);

			//vkCmdEndRenderPass(drawCmdBuffers[i]);
			drawCmdBuffers[i].endRenderPass();

			//VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
			drawCmdBuffers[i].end();
		}
	}

	void loadMeshes()
	{
		loadMesh(getAssetPath() + "models/rock01.dae", &meshes.example, vertexLayout, 0.1f);
	}

	void loadTextures()
	{
		textureLoader->loadTextureArray(
			getAssetPath() + "textures/texturearray_rocks_bc3.ktx",
			vk::Format::eBc3UnormBlock,
			&textures.colorMap);
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(2);

		// Mesh vertex buffer (description) at binding point 0
		vertices.bindingDescriptions[0] =
			vkx::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				vkx::vertexSize(vertexLayout),
				// Input rate for the data passed to shader
				// Step for each vertex rendered
				vk::VertexInputRate::eVertex);

		vertices.bindingDescriptions[1] =
			vkx::vertexInputBindingDescription(
				INSTANCE_BUFFER_BIND_ID,
				sizeof(InstanceData),
				// Input rate for the data passed to shader
				// Step for each instance rendered
				vk::VertexInputRate::eInstance);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.clear();

		// Per-Vertex attributes
		// Location 0 : Position
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				vk::Format::eR32G32B32Sfloat,
				0)
		);
		// Location 1 : Normal
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, vk::Format::eR32G32B32Sfloat, sizeof(float) * 3)
		);
		// Location 2 : Texture coordinates
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, vk::Format::eR32G32Sfloat, sizeof(float) * 6)
		);
		// Location 3 : Color
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, vk::Format::eR32G32B32Sfloat, sizeof(float) * 8)
		);

		// Instanced attributes
		// Location 4 : Position
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 5, vk::Format::eR32G32B32Sfloat, sizeof(float) * 3)
		);
		// Location 5 : Rotation
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 4, vk::Format::eR32G32B32Sfloat, 0)
		);
		// Location 6 : Scale
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 6, vk::Format::eR32Sfloat, sizeof(float) * 6)
		);
		// Location 7 : Texture array layer index
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID, 7, vk::Format::eR32Sint, sizeof(float) * 7)
		);


		vertices.inputState = vkx::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		// Example uses one ubo 
		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			vkx::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
			vkx::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1),
		};

		vk::DescriptorPoolCreateInfo descriptorPoolInfo =
			vkx::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 2);

		//VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
		//device.createDescriptorPool(&descriptorPoolInfo, nullptr, &descriptorPool);
		descriptorPool = device.createDescriptorPool(descriptorPoolInfo);
	}

	void setupDescriptorSetLayout()
	{
		std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkx::descriptorSetLayoutBinding(
				vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eVertex,
				0),
			// Binding 1 : Fragment shader combined sampler
			vkx::descriptorSetLayoutBinding(
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eFragment,
				1),
		};

		vk::DescriptorSetLayoutCreateInfo descriptorLayout =
			vkx::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				setLayoutBindings.size());

		//VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));
		device.createDescriptorSetLayout(&descriptorLayout, nullptr, &descriptorSetLayout);

		vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vkx::pipelineLayoutCreateInfo(
				&descriptorSetLayout,
				1);

		//VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		device.createPipelineLayout(&pPipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	}

	void setupDescriptorSet()
	{
		vk::DescriptorSetAllocateInfo allocInfo =
			vkx::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayout,
				1);

		//VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

		vk::DescriptorImageInfo texDescriptor =
			vkx::descriptorImageInfo(
				textures.colorMap.sampler,
				textures.colorMap.view,
				vk::ImageLayout::eGeneral);

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkx::writeDescriptorSet(
				descriptorSet,
				vk::DescriptorType::eUniformBuffer,
				0,
				&uniformData.vsScene.descriptor),
			// Binding 1 : Color map 
			vkx::writeDescriptorSet(
				descriptorSet,
				vk::DescriptorType::eCombinedImageSampler,
				1,
				&texDescriptor)
		};

		//vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
		device.updateDescriptorSets(writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{

		vk::ColorComponentFlags allFlags(
			vk::ColorComponentFlagBits::eR |
			vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB |
			vk::ColorComponentFlagBits::eA);

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vkx::pipelineInputAssemblyStateCreateInfo(
				vk::PrimitiveTopology::eTriangleList,
				vk::PipelineInputAssemblyStateCreateFlags(),
				VK_FALSE);

		vk::PipelineRasterizationStateCreateInfo rasterizationState =
			vkx::pipelineRasterizationStateCreateInfo(
				vk::PolygonMode::eFill,
				vk::CullModeFlagBits::eBack,
				vk::FrontFace::eClockwise,
				vk::PipelineRasterizationStateCreateFlags());

		vk::PipelineColorBlendAttachmentState blendAttachmentState =
			vkx::pipelineColorBlendAttachmentState(
				allFlags,
				VK_FALSE);

		vk::PipelineColorBlendStateCreateInfo colorBlendState =
			vkx::pipelineColorBlendStateCreateInfo(
				1,
				&blendAttachmentState);

		vk::PipelineDepthStencilStateCreateInfo depthStencilState =
			vkx::pipelineDepthStencilStateCreateInfo(
				VK_TRUE,
				VK_TRUE,
				vk::CompareOp::eLessOrEqual);

		vk::PipelineViewportStateCreateInfo viewportState =
			vkx::pipelineViewportStateCreateInfo(1, 1, vk::PipelineViewportStateCreateFlags());

		vk::PipelineMultisampleStateCreateInfo multisampleState =
			vkx::pipelineMultisampleStateCreateInfo(
				vk::SampleCountFlagBits::e1,
				vk::PipelineMultisampleStateCreateFlags());

		std::vector<vk::DynamicState> dynamicStateEnables = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
		vk::PipelineDynamicStateCreateInfo dynamicState =
			vkx::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				dynamicStateEnables.size(),
				vk::PipelineDynamicStateCreateFlagBits());

		// Instacing pipeline
		// Load shaders
		std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/instancing/instancing.vert.spv", vk::ShaderStageFlagBits::eVertex);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/instancing/instancing.frag.spv", vk::ShaderStageFlagBits::eFragment);

		vk::GraphicsPipelineCreateInfo pipelineCreateInfo =
			vkx::pipelineCreateInfo(
				pipelineLayout,
				renderPass,
				vk::PipelineCreateFlags());

		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = shaderStages.size();
		pipelineCreateInfo.pStages = shaderStages.data();

		//VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid));
		pipelines.solid = device.createGraphicsPipeline(pipelineCache, pipelineCreateInfo, nullptr);
	}

	float rnd(float range)
	{
		return range * (rand() / double(RAND_MAX));
	}

	void prepareInstanceData()
	{
		std::vector<InstanceData> instanceData;
		instanceData.resize(INSTANCE_COUNT);

		std::mt19937 rndGenerator(time(NULL));
		std::uniform_real_distribution<double> uniformDist(0.0, 1.0);

		for (auto i = 0; i < INSTANCE_COUNT; i++)
		{
			instanceData[i].rot = glm::vec3(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator));
			float theta = 2 * M_PI * uniformDist(rndGenerator);
			float phi = acos(1 - 2 * uniformDist(rndGenerator));
			glm::vec3 pos;
			instanceData[i].pos = glm::vec3(sin(phi) * cos(theta), sin(theta) * uniformDist(rndGenerator) / 1500.0f, cos(phi)) * 7.5f;
			instanceData[i].scale = 1.0f + uniformDist(rndGenerator) * 2.0f;
			instanceData[i].texIndex = rnd(textures.colorMap.layerCount);
		}

		instanceBuffer.size = instanceData.size() * sizeof(InstanceData);

		// Staging
		// Instanced data is static, copy to device local memory 
		// This results in better performance

		struct {
			vk::DeviceMemory memory;
			vk::Buffer buffer;
		} stagingBuffer;

		vulkanApp::createBuffer(
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible,
			instanceBuffer.size,
			instanceData.data(),
			&stagingBuffer.buffer,
			&stagingBuffer.memory);

		vulkanApp::createBuffer(
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			instanceBuffer.size,
			nullptr,
			&instanceBuffer.buffer,
			&instanceBuffer.memory);

		// Copy to staging buffer
		vk::CommandBuffer copyCmd = vulkanApp::createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

		vk::BufferCopy copyRegion;
		copyRegion.size = instanceBuffer.size;
		/*vkCmdCopyBuffer(
			copyCmd,
			stagingBuffer.buffer,
			instanceBuffer.buffer,
			1,
			&copyRegion);*/
		copyCmd.copyBuffer(
			stagingBuffer.buffer,
			instanceBuffer.buffer,
			1,
			&copyRegion);

		vulkanApp::flushCommandBuffer(copyCmd, queue, true);

		instanceBuffer.descriptor.range = instanceBuffer.size;
		instanceBuffer.descriptor.buffer = instanceBuffer.buffer;
		instanceBuffer.descriptor.offset = 0;

		// Destroy staging resources
		vkDestroyBuffer(device, stagingBuffer.buffer, nullptr);
		vkFreeMemory(device, stagingBuffer.memory, nullptr);
	}

	void prepareUniformBuffers()
	{
		createBuffer(
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			sizeof(uboVS),
			nullptr,
			&uniformData.vsScene.buffer,
			&uniformData.vsScene.memory,
			&uniformData.vsScene.descriptor);

		// Map for host access
		//VK_CHECK_RESULT(vkMapMemory(device, uniformData.vsScene.memory, 0, sizeof(uboVS), 0, (void **)&uniformData.vsScene.mapped));
		uniformData.vsScene.mapped = (void**)device.mapMemory(uniformData.vsScene.memory, 0, sizeof(uboVS), vk::MemoryMapFlags());

		updateUniformBuffer(true);
	}

	void updateUniformBuffer(bool viewChanged)
	{
		if (viewChanged)
		{
			uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.001f, 256.0f);
			uboVS.view = glm::translate(glm::mat4(), cameraPos + glm::vec3(0.0f, 0.0f, zoom));
			uboVS.view = glm::rotate(uboVS.view, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			uboVS.view = glm::rotate(uboVS.view, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
			uboVS.view = glm::rotate(uboVS.view, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		}

		if (!paused)
		{
			/*
			auto now = std::chrono::system_clock::now();
			auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
			auto epoch = now_ms.time_since_epoch();
			double time = epoch.count();

			time = time / 147405412324800.0;
			*/


			/*auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) -
				std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
			auto msT = 16*sin(ms.count() * 0.000005);*/

			//uboVS.time += frameTimer * 0.05f;

			auto now = std::chrono::system_clock::now();
			auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
			auto epoch = now_ms.time_since_epoch();
			auto msT = 0.5 * sin(epoch.count() * 0.003);
			
			uboVS.time = msT;

		}

		memcpy(uniformData.vsScene.mapped, &uboVS, sizeof(uboVS));
	}

	void draw()
	{
		vulkanApp::prepareFrame();

		// Command buffer to be sumitted to the queue
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		// Submit to queue
		queue.submit(1, &submitInfo, VK_NULL_HANDLE);

		vulkanApp::submitFrame();
	}

	void prepare()
	{
		vulkanApp::prepare();
		loadTextures();
		loadMeshes();
		prepareInstanceData();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared) {
			return;
		}
		draw();
		if (!paused) {
			updateUniformBuffer(false);
		}
	}

	virtual void viewChanged()
	{
		updateUniformBuffer(true);
	}

	virtual void getOverlayText(vkx::VulkanTextOverlay * textOverlay)
	{

		/*auto now = std::chrono::system_clock::now();
		auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
		auto epoch = now_ms.time_since_epoch();
		auto msT = 0.005 * sin(epoch.count() * 0.001);*/

		/*auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) -
			std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
		auto msT = ms.count();//1 * sin(ms.count() * 0.001);*/

		textOverlay->addText("Rendering " + std::to_string(INSTANCE_COUNT) + " instances", 5.0f, 85.0f, vkx::VulkanTextOverlay::alignLeft);
		//textOverlay->addText("Current Time: " + std::to_string(msT), 5.0f, 105.0f, VulkanTextOverlay::alignLeft);
	}
};

//VULKAN_EXAMPLE_MAIN()

VulkanExample *vulkanExample;
/*LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (vulkanExample != NULL) {
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}*/
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	vulkanExample = new VulkanExample();
	vulkanExample->setupWindow();
	vulkanExample->prepare();
	vulkanExample->renderLoop();
	delete(vulkanExample);
	return 0;
}

