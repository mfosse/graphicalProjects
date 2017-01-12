/*
* Vulkan Example - Indirect drawing
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*
* Summary:
* Use a device local buffer that stores draw commands for instanced rendering of different meshes stored
* in the same buffer.
*
* Indirect drawing offloads draw command generation and offers the ability to update them on the GPU
* without the CPU having to touch the buffer again, also reducing the number of drawcalls.
*
* The example shows how to setup and fill such a buffer on the CPU side, stages it to the device and
* shows how to render it using only one draw command.
*
* See readme.md for details
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <vector>
#include <random>

#include "main/global.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.hpp>
#include "vulkanApp.h"
#include "vulkanBuffer.h"

#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1
#define ENABLE_VALIDATION true

// Number of instances per object
#if defined(__ANDROID__)
	#define OBJECT_INSTANCE_COUNT 1024
	// Circular range of plant distribution
#define PLANT_RADIUS 20.0f
#else
	#define OBJECT_INSTANCE_COUNT 512
	// Circular range of plant distribution
	#define PLANT_RADIUS 25.0f
#endif

// Vertex layout for this example
std::vector<vkMeshLoader::VertexLayout> vertexLayout =
{
	vkMeshLoader::VERTEX_LAYOUT_POSITION,
	vkMeshLoader::VERTEX_LAYOUT_NORMAL,
	vkMeshLoader::VERTEX_LAYOUT_UV,
	vkMeshLoader::VERTEX_LAYOUT_COLOR
};

class VulkanExample : public vulkanApp
{
public:
	struct {
		vk::PipelineVertexInputStateCreateInfo inputState;
		std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
		std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct {
		vkMeshLoader::MeshBuffer plants;
		vkMeshLoader::MeshBuffer ground;
		vkMeshLoader::MeshBuffer skysphere;
	} meshes;

	struct {
		vkTools::VulkanTexture plants;
		vkTools::VulkanTexture ground;
	} textures;

	// Per-instance data block
	struct InstanceData {
		glm::vec3 pos;
		glm::vec3 rot;
		float scale;
		uint32_t texIndex;
	};

	// Contains the instanced data
	vkx::Buffer instanceBuffer;
	// Contains the indirect drawing commands
	vkx::Buffer indirectCommandsBuffer;
	uint32_t indirectDrawCount;

	struct {
		glm::mat4 projection;
		glm::mat4 view;
	} uboVS;

	struct {
		vkx::Buffer scene;
	} uniformData;

	struct {
		vk::Pipeline instancedMeshes;
		vk::Pipeline skinnedMeshes;
		vk::Pipeline animatedModels;
		vk::Pipeline staticGeometries;
		vk::Pipeline plants;
		vk::Pipeline ground;
		vk::Pipeline skysphere;
	} pipelines;

	vk::PipelineLayout pipelineLayout;
	vk::DescriptorSet descriptorSet;
	vk::DescriptorSetLayout descriptorSetLayout;

	vk::Sampler samplerRepeat;

	uint32_t objectCount = 0;

	// Store the indirect draw commands containing index offsets and instance count per object
	std::vector<vk::DrawIndexedIndirectCommand> indirectCommands;

	VulkanExample() : vulkanApp(ENABLE_VALIDATION)
	{
		enableTextOverlay = true;
		title = "Vulkan Example - Indirect rendering";
		camera.type = Camera::CameraType::firstperson;
		camera.setProjection(60.0f, (float)width / (float)height, 0.1f, 512.0f);
		//camera.setRotation(glm::quat(glm::vec3(-12.0f, 159.0f, 0.0f)));
		camera.setTranslation(glm::vec3(0.4f, 1.25f, 0.0f));
		camera.movementSpeed = 5.0f;
		camera.rotationSpeed = 0.01f;
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipelines.plants, nullptr);
		vkDestroyPipeline(device, pipelines.ground, nullptr);
		vkDestroyPipeline(device, pipelines.skysphere, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkMeshLoader::freeMeshBufferResources(device, &meshes.plants);
		vkMeshLoader::freeMeshBufferResources(device, &meshes.ground);
		vkMeshLoader::freeMeshBufferResources(device, &meshes.skysphere);
		textureLoader->destroyTexture(textures.plants);
		textureLoader->destroyTexture(textures.ground);
		instanceBuffer.destroy();
		indirectCommandsBuffer.destroy();
		uniformData.scene.destroy();
	}

	void reBuildCommandBuffers()
	{
		if (!checkCommandBuffers()) {
			destroyCommandBuffers();
			createCommandBuffers();
		}
		buildCommandBuffers();
	}

	void buildCommandBuffers()
	{
		vk::CommandBufferBeginInfo cmdBufInfo;

		vk::ClearValue clearValues[2];
		//clearValues[0].color = { { 0.18f, 0.27f, 0.5f, 0.0f } };
		clearValues[0].color = std::array<float, 4>{0.18f, 0.27f, 0.5f, 0.0f};
		clearValues[1].depthStencil = { 1.0f, 0 };

		vk::RenderPassBeginInfo renderPassBeginInfo;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			//VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			drawCmdBuffers[i].begin(&cmdBufInfo);

			//vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, vk::SubpassContents::eInline);
			drawCmdBuffers[i].beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);

			vk::Viewport viewport = vkx::viewport((float)width, (float)height, 0.0f, 1.0f);
			//vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			drawCmdBuffers[i].setViewport(0, 1, &viewport);

			vk::Rect2D scissor = vkx::rect2D(width, height, 0, 0);
			//vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
			drawCmdBuffers[i].setScissor(0, 1, &scissor);

			vk::DeviceSize offsets[1] = { 0 };
			//vkCmdBindDescriptorSets(drawCmdBuffers[i], vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			drawCmdBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			// Plants
			//vkCmdBindPipeline(drawCmdBuffers[i], vk::PipelineBindPoint::eGraphics, pipelines.plants);
			drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.plants);
			// Binding point 0 : Mesh vertex buffer
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.plants.vertices.buf, offsets);
			drawCmdBuffers[i].bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1, &meshes.plants.vertices.buf, offsets);

			// Binding point 1 : Instance data buffer
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer.buffer, offsets);
			drawCmdBuffers[i].bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer.buffer, offsets);

			//vkCmdBindIndexBuffer(drawCmdBuffers[i], meshes.plants.indices.buf, 0, vk::IndexType::eUint32);
			drawCmdBuffers[i].bindIndexBuffer(meshes.plants.indices.buf, 0, vk::IndexType::eUint32);

			// If the multi draw feature is supported:
			// One draw call for an arbitrary number of ojects
			// Index offsets and instance count are taken from the indirect buffer
			if (vulkanDevice->features.multiDrawIndirect) {
				//vkCmdDrawIndexedIndirect(drawCmdBuffers[i], indirectCommandsBuffer.buffer, 0, indirectDrawCount, sizeof(vk::DrawIndexedIndirectCommand));
				drawCmdBuffers[i].drawIndexedIndirect(indirectCommandsBuffer.buffer, 0, indirectDrawCount, sizeof(vk::DrawIndexedIndirectCommand));
			} else {
				// If multi draw is not available, we must issue separate draw commands
				for (auto j = 0; j < indirectCommands.size(); j++) {
					//vkCmdDrawIndexedIndirect(drawCmdBuffers[i], indirectCommandsBuffer.buffer, j * sizeof(vk::DrawIndexedIndirectCommand), 1, sizeof(vk::DrawIndexedIndirectCommand));
					drawCmdBuffers[i].drawIndexedIndirect(indirectCommandsBuffer.buffer, j * sizeof(vk::DrawIndexedIndirectCommand), 1, sizeof(vk::DrawIndexedIndirectCommand));
				}
			}

			// Ground
			//vkCmdBindPipeline(drawCmdBuffers[i], vk::PipelineBindPoint::eGraphics, pipelines.ground);
			drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.ground);
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.ground.vertices.buf, offsets);
			drawCmdBuffers[i].bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1, &meshes.ground.vertices.buf, offsets);
			//vkCmdBindIndexBuffer(drawCmdBuffers[i], meshes.ground.indices.buf, 0, vk::IndexType::eUint32);
			drawCmdBuffers[i].bindIndexBuffer(meshes.ground.indices.buf, 0, vk::IndexType::eUint32);
			//vkCmdDrawIndexed(drawCmdBuffers[i], meshes.ground.indexCount, 1, 0, 0, 0);
			drawCmdBuffers[i].drawIndexed(meshes.ground.indexCount, 1, 0, 0, 0);

			// Skysphere
			//vkCmdBindPipeline(drawCmdBuffers[i], vk::PipelineBindPoint::eGraphics, pipelines.skysphere);
			drawCmdBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.skysphere);
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.skysphere.vertices.buf, offsets);
			drawCmdBuffers[i].bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1, &meshes.skysphere.vertices.buf, offsets);
			//vkCmdBindIndexBuffer(drawCmdBuffers[i], meshes.skysphere.indices.buf, 0, vk::IndexType::eUint32);
			drawCmdBuffers[i].bindIndexBuffer(meshes.skysphere.indices.buf, 0, vk::IndexType::eUint32);
			//vkCmdDrawIndexed(drawCmdBuffers[i], meshes.skysphere.indexCount, 1, 0, 0, 0);
			drawCmdBuffers[i].drawIndexed(meshes.skysphere.indexCount, 1, 0, 0, 0);

			//vkCmdEndRenderPass(drawCmdBuffers[i]);
			drawCmdBuffers[i].endRenderPass();

			//VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
			drawCmdBuffers[i].end();
		}
	}

	void loadAssets()
	{
		loadMesh(getAssetPath() + "models/plants.dae", &meshes.plants, vertexLayout, 0.0025f);
		loadMesh(getAssetPath() + "models/plane_circle.dae", &meshes.ground, vertexLayout, PLANT_RADIUS + 1.0f);
		loadMesh(getAssetPath() + "models/skysphere.dae", &meshes.skysphere, vertexLayout, 512.0f / 10.0f);

		textureLoader->loadTextureArray(getAssetPath() + "textures/texturearray_plants_bc3.ktx", vk::Format::eBc3UnormBlock, &textures.plants);
		textureLoader->loadTexture(getAssetPath() + "textures/ground_dry_bc3.ktx", vk::Format::eBc3UnormBlock, &textures.ground);
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(2);

		// Mesh vertex buffer (description) at binding point 0
		vertices.bindingDescriptions[0] =
			vkx::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				vkMeshLoader::vertexSize(vertexLayout),
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
			vkx::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				vk::Format::eR32G32B32Sfloat,
				sizeof(float) * 3)
		);
		// Location 2 : Texture coordinates
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				vk::Format::eR32G32Sfloat,
				sizeof(float) * 6)
		);
		// Location 3 : Color
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				3,
				vk::Format::eR32G32B32Sfloat,
				sizeof(float) * 8)
		);

		// Instanced attributes
		// Location 4: Position
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID, 4, vk::Format::eR32G32B32Sfloat, offsetof(InstanceData, pos))
		);
		// Location 5: Rotation
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID, 5, vk::Format::eR32G32B32Sfloat, offsetof(InstanceData, rot))
		);
		// Location 6: Scale
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID, 6, vk::Format::eR32Sfloat, offsetof(InstanceData, scale))
		);
		// Location 7: Texture array layer index
		vertices.attributeDescriptions.push_back(
			vkx::vertexInputAttributeDescription(
				INSTANCE_BUFFER_BIND_ID, 7, vk::Format::eR32Sint, offsetof(InstanceData, texIndex))
		);

		vertices.inputState = vkx::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		// Example uses one ubo 
		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			vkx::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
			vkx::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 2),
		};

		vk::DescriptorPoolCreateInfo descriptorPoolInfo =
			vkx::descriptorPoolCreateInfo(
				static_cast<uint32_t>(poolSizes.size()),
				poolSizes.data(),
				2);

		//VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
		device.createDescriptorPool(&descriptorPoolInfo, nullptr, &descriptorPool);
	}

	void setupDescriptorSetLayout()
	{
		std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings =
		{
			// Binding 0: Vertex shader uniform buffer
			vkx::descriptorSetLayoutBinding(
				vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eVertex,
				0),
			// Binding 1: Fragment shader combined sampler (plants texture array)
			vkx::descriptorSetLayoutBinding(
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eFragment,
				1),
			// Binding 1: Fragment shader combined sampler (ground texture)
			vkx::descriptorSetLayoutBinding(
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eFragment,
				2),
		};

		vk::DescriptorSetLayoutCreateInfo descriptorLayout =
			vkx::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				static_cast<uint32_t>(setLayoutBindings.size()));

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
		device.allocateDescriptorSets(&allocInfo, &descriptorSet);

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0: Vertex shader uniform buffer
			vkx::writeDescriptorSet(
				descriptorSet,
				vk::DescriptorType::eUniformBuffer,
				0,
				&uniformData.scene.descriptor),
			// Binding 1: Plants texture array combined 
			vkx::writeDescriptorSet(
				descriptorSet,
				vk::DescriptorType::eCombinedImageSampler,
				1,
				&textures.plants.descriptor),
			// Binding 2: Ground texture combined 
			vkx::writeDescriptorSet(
				descriptorSet,
				vk::DescriptorType::eCombinedImageSampler,
				2,
				&textures.ground.descriptor)
		};

		//vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
		device.updateDescriptorSets(static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vkx::pipelineInputAssemblyStateCreateInfo(
				vk::PrimitiveTopology::eTriangleList,
				vk::PipelineInputAssemblyStateCreateFlags(),
				VK_FALSE);

		vk::PipelineRasterizationStateCreateInfo rasterizationState =
			vkx::pipelineRasterizationStateCreateInfo(
				vk::PolygonMode::eFill,
				vk::CullModeFlagBits::eNone,
				vk::FrontFace::eClockwise,
				vk::PipelineRasterizationStateCreateFlags());

		vk::PipelineColorBlendAttachmentState blendAttachmentState =
			vkx::pipelineColorBlendAttachmentState(
				vk::ColorComponentFlagBits::eA,//0xf,//important
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
				static_cast<uint32_t>(dynamicStateEnables.size()),
				vk::PipelineDynamicStateCreateFlags());

		vk::GraphicsPipelineCreateInfo pipelineCreateInfo =
			vkx::pipelineCreateInfo(
				pipelineLayout,
				renderPass,
				vk::PipelineCreateFlags());

		std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;

		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		// Indirect (and instanced) pipeline for the plants
		shaderStages[0] = loadShader(getAssetPath() + "shaders/indirectdraw/indirectdraw.vert.spv", vk::ShaderStageFlagBits::eVertex);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/indirectdraw/indirectdraw.frag.spv", vk::ShaderStageFlagBits::eFragment);
		//VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.plants));
		pipelines.plants = device.createGraphicsPipeline(pipelineCache, pipelineCreateInfo, nullptr);

		// Ground
		shaderStages[0] = loadShader(getAssetPath() + "shaders/indirectdraw/ground.vert.spv", vk::ShaderStageFlagBits::eVertex);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/indirectdraw/ground.frag.spv", vk::ShaderStageFlagBits::eFragment);
		//rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		//VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.ground));
		pipelines.ground = device.createGraphicsPipeline(pipelineCache, pipelineCreateInfo, nullptr);

		// Skysphere
		shaderStages[0] = loadShader(getAssetPath() + "shaders/indirectdraw/skysphere.vert.spv", vk::ShaderStageFlagBits::eVertex);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/indirectdraw/skysphere.frag.spv", vk::ShaderStageFlagBits::eFragment);
		//rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		//VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skysphere));
		//device.createGraphicsPipeline(pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skysphere);
		pipelines.skysphere = device.createGraphicsPipeline(pipelineCache, pipelineCreateInfo, nullptr);//what?
	}

	// Prepare (and stage) a buffer containing the indirect draw commands
	void prepareIndirectData()
	{
		indirectCommands.clear();

		// Create on indirect command for each mesh in the scene
		uint32_t m = 0;
		for (auto& meshDescriptor : meshes.plants.meshDescriptors)
		{
			vk::DrawIndexedIndirectCommand indirectCmd{};
			indirectCmd.instanceCount = OBJECT_INSTANCE_COUNT;
			indirectCmd.firstInstance = m * OBJECT_INSTANCE_COUNT;
			indirectCmd.firstIndex = meshDescriptor.indexBase;
			indirectCmd.indexCount = meshDescriptor.indexCount;

			indirectCommands.push_back(indirectCmd);

			m++;
		}

		indirectDrawCount = static_cast<uint32_t>(indirectCommands.size());

		objectCount = 0;
		for (auto indirectCmd : indirectCommands)
		{
			objectCount += indirectCmd.instanceCount;
		}

		vkx::Buffer stagingBuffer;
		/*VK_CHECK_RESULT(*/vulkanDevice->createBuffer(
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&stagingBuffer,
			indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand),
			indirectCommands.data())/*)*/;

		/*VK_CHECK_RESULT(*/vulkanDevice->createBuffer(
			vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			&indirectCommandsBuffer,
			stagingBuffer.size)/*)*/;

		vulkanDevice->copyBuffer(&stagingBuffer, &indirectCommandsBuffer, queue);

		stagingBuffer.destroy();
	}

	// Prepare (and stage) a buffer containing instanced data for the mesh draws
	void prepareInstanceData()
	{
		std::vector<InstanceData> instanceData;
		instanceData.resize(objectCount);

		std::mt19937 rndGenerator((unsigned)time(NULL));
		std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);

		for (uint32_t i = 0; i < objectCount; i++)
		{
			instanceData[i].rot = glm::vec3(0.0f, float(M_PI) * uniformDist(rndGenerator), 0.0f);
			float theta = 2 * float(M_PI) * uniformDist(rndGenerator);
			float phi = acos(1 - 2 * uniformDist(rndGenerator));
			instanceData[i].pos = glm::vec3(sin(phi) * cos(theta), 0.0f, cos(phi)) * PLANT_RADIUS;
			instanceData[i].scale = 1.0f + uniformDist(rndGenerator) * 2.0f;
			instanceData[i].texIndex = i / OBJECT_INSTANCE_COUNT;
		}

		vkx::Buffer stagingBuffer;
		/*VK_CHECK_RESULT(*/vulkanDevice->createBuffer(
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&stagingBuffer,
			instanceData.size() * sizeof(InstanceData),
			instanceData.data())/*)*/;

		/*VK_CHECK_RESULT(*/vulkanDevice->createBuffer(
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			&instanceBuffer,
			stagingBuffer.size)/*)*/;

		vulkanDevice->copyBuffer(&stagingBuffer, &instanceBuffer, queue);

		stagingBuffer.destroy();
	}

	void prepareUniformBuffers()
	{
		/*VK_CHECK_RESULT(*/vulkanDevice->createBuffer(
			vk::BufferUsageFlagBits::eUniformBuffer ,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&uniformData.scene,
			sizeof(uboVS))/*)*/;

		//VK_CHECK_RESULT(uniformData.scene.map());
		uniformData.scene.map();

		updateUniformBuffer(true);
	}

	void updateUniformBuffer(bool viewChanged)
	{
		if (viewChanged) {
			uboVS.projection = camera.matrices.projection;
			uboVS.view = camera.matrices.view;
		}

		memcpy(uniformData.scene.mapped, &uboVS, sizeof(uboVS));
	}

	void draw()
	{
		vulkanApp::prepareFrame();

		// Command buffer to be sumitted to the queue
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		// Submit to queue
		//VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		queue.submit(1, &submitInfo, VK_NULL_HANDLE);

		vulkanApp::submitFrame();
	}

	void prepare()
	{

		vulkanApp::prepare();
		loadAssets();

		prepareIndirectData();
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
	}

	virtual void viewChanged()
	{
		updateUniformBuffer(true);
	}

	virtual void getOverlayText(vkx::VulkanTextOverlay *textOverlay)
	{
		textOverlay->addText(std::to_string(objectCount) + " objects", 5.0f, 85.0f, vkx::VulkanTextOverlay::alignLeft);
		if (!vulkanDevice->features.multiDrawIndirect) {
			textOverlay->addText("multiDrawIndirect not supported", 5.0f, 105.0f, vkx::VulkanTextOverlay::alignLeft);
		}
	}
};

//VULKAN_EXAMPLE_MAIN()

VulkanExample *vulkanExample;																		
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)						
{																									
	if (vulkanExample != NULL) {																							
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);									
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	vulkanExample = new VulkanExample();
	vulkanExample->setupWindow(hInstance, WndProc);
	vulkanExample->initSwapchain();
	vulkanExample->prepare();
	vulkanExample->renderLoop();
	delete(vulkanExample);
	return 0;
}