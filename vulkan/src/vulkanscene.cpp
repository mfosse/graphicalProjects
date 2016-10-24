﻿/*
* Vulkan Demo Scene
*
* Don't take this a an example, it's more of a personal playground
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* Note : Different license than the other examples!
*
* This code is licensed under the Mozilla Public License Version 2.0 (http://opensource.org/licenses/MPL-2.0)
*/

#include "vulkanApp.h"

static std::vector<std::string> names{ "logos", "background", "models", "skybox" };

std::vector<vkx::VertexLayout> vertexLayout =
{
	vkx::VertexLayout::VERTEX_LAYOUT_POSITION,
	vkx::VertexLayout::VERTEX_LAYOUT_NORMAL,
	vkx::VertexLayout::VERTEX_LAYOUT_UV,
	vkx::VertexLayout::VERTEX_LAYOUT_COLOR
};

class VulkanExample : public vkx::vulkanApp {
public:

	/*struct DemoMeshes {
		vk::PipelineVertexInputStateCreateInfo inputState;
		std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
		std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
		//vk::Pipeline pipeline;
		vkx::MeshLoader* logos;
		vkx::MeshLoader* background;
		vkx::MeshLoader* models;
		vkx::MeshLoader* skybox;
	} demoMeshes;*/



	vk::PipelineVertexInputStateCreateInfo inputState;
	std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
	std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
	
	vkx::MeshLoader* logos;
	vkx::MeshLoader* background;
	vkx::MeshLoader* models;
	vkx::MeshLoader* skybox;
	vkx::MeshLoader* plane;

	std::vector<vkx::MeshLoader*> meshLoaders;
	std::vector<vkx::MeshBuffer> meshBuffers;
	std::vector<vkx::Mesh> meshes;

	struct {
		vkx::UniformData meshVS;
	} uniformData;

	struct {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 normal;
		glm::mat4 view;
		glm::vec4 lightPos;
	} uboVS;

	struct {
		vkx::Texture skybox;
	} textures;

	struct {
		vk::Pipeline logos;
		vk::Pipeline models;
		vk::Pipeline skybox;
	} pipelines;

	vk::PipelineLayout pipelineLayout;
	vk::DescriptorSet descriptorSet;
	vk::DescriptorSetLayout descriptorSetLayout;

	glm::vec4 lightPos = glm::vec4(1.0f, 2.0f, 0.0f, 0.0f);

	VulkanExample() : vkx::vulkanApp(ENABLE_VALIDATION) {
		size.width = 1280;
		size.height = 720;


		camera.setTranslation({ -1.0f, -1.0f, -3.0f });
		//camera.matrices.projection = glm::perspectiveRH(glm::radians(60.0f), (float)size.width / (float)size.height, 0.001f, 256.0f);

		title = "Vulkan Demo Scene";
	}

	~VulkanExample() {
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		device.destroyPipeline(pipelines.logos);
		device.destroyPipeline(pipelines.models);
		device.destroyPipeline(pipelines.skybox);

		device.destroyPipelineLayout(pipelineLayout);
		device.destroyDescriptorSetLayout(descriptorSetLayout);

		uniformData.meshVS.destroy();

		for (auto& mesh : meshes) {
			//mesh.buffers.vertices.buffer
			device.destroyBuffer(mesh.buffers.vertices.buffer);
			device.freeMemory(mesh.buffers.vertices.memory);

			device.destroyBuffer(mesh.buffers.indices.buffer);
			device.freeMemory(mesh.buffers.indices.memory);
		}

		textures.skybox.destroy();

		delete(logos);
		delete(background);
		delete(models);
		delete(skybox);
	}

	void loadTextures() {
		textures.skybox = textureLoader->loadCubemap(getAssetPath() + "textures/cubemap_vulkan.ktx", vk::Format::eR8G8B8A8Unorm);
	}

	void updateDrawCommandBuffer(const vk::CommandBuffer& cmdBuffer) {
		cmdBuffer.setViewport(0, vkx::viewport(size));
		cmdBuffer.setScissor(0, vkx::rect2D(size));
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSet, nullptr);
		
		for (auto& mesh : meshLoaders) {
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh->pipeline);
			cmdBuffer.bindVertexBuffers(VERTEX_BUFFER_BIND_ID, mesh->vertexBuffer.buf, { 0 });
			cmdBuffer.bindIndexBuffer(mesh->indexBuffer.buf, 0, vk::IndexType::eUint32);
			cmdBuffer.drawIndexed(mesh->indexBuffer.count, 1, 0, 0, 0);
		}

		for (auto& mesh : meshes) {
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh.pipeline);
			cmdBuffer.bindVertexBuffers(VERTEX_BUFFER_BIND_ID, mesh.buffers.vertices.buffer, vk::DeviceSize()/*{ 0 }*/);
			cmdBuffer.bindIndexBuffer(mesh.buffers.indices.buffer, 0, vk::IndexType::eUint32);
			cmdBuffer.drawIndexed(mesh.buffers.indexCount, 1, 0, 0, 0);
			/*mesh.drawIndexed(cmdBuffer);*/
		}
	}

	void prepareVertices() {
		struct Vertex {
			float pos[3];
			float normal[3];
			float uv[2];
			float color[3];
		};

		// Load meshes for demos scene
		logos = new vkx::MeshLoader();
		background = new vkx::MeshLoader();
		models = new vkx::MeshLoader();
		skybox = new vkx::MeshLoader();


		logos->load(getAssetPath() + "models/vulkanscenelogos.dae");
		background->load(getAssetPath() + "models/vulkanscenebackground.dae");
		models->load(getAssetPath() + "models/vulkanscenemodels.dae");
		skybox->load(getAssetPath() + "models/cube.obj");

		// re-usable? meshloader class// definitely not reusable// important
		vkx::MeshLoader* loader = new vkx::MeshLoader();

		loader/*.*/->load(getAssetPath() + "models/plane.obj");
		vkx::Mesh planeMesh = loader/*.*/->createMeshFromBuffers(context, vertexLayout, 1.0f, VERTEX_BUFFER_BIND_ID);

		loader = new vkx::MeshLoader();

		loader/*.*/->load(getAssetPath() + "models/vulkanscenelogos.dae");
		vkx::Mesh logoMesh = loader/*.*/->createMeshFromBuffers(context, vertexLayout, 1.0f, VERTEX_BUFFER_BIND_ID);

		meshes.push_back(planeMesh);
		meshes.push_back(logoMesh);






		std::vector<vkx::MeshLoader*> meshLoaderList;
		meshLoaderList.push_back(skybox); // skybox first because of depth writes
		meshLoaderList.push_back(logos);
		meshLoaderList.push_back(background);
		meshLoaderList.push_back(models);
		//meshLoaderList.push_back(plane);

		// todo : Use mesh function for loading
		/*float scale = 1.0f;
		for (auto& meshLoader : meshLoaderList) {
			// Generate vertex buffer (pos, normal, uv, color)
			std::vector<Vertex> vertexBuffer;
			for (int m = 0; m < meshLoader->m_Entries.size(); m++) {
				for (int i = 0; i < meshLoader->m_Entries[m].Vertices.size(); i++) {

					glm::vec3 pos = meshLoader->m_Entries[m].Vertices[i].m_pos * scale;
					glm::vec3 normal = meshLoader->m_Entries[m].Vertices[i].m_normal;
					glm::vec2 uv = meshLoader->m_Entries[m].Vertices[i].m_tex;
					glm::vec3 col = meshLoader->m_Entries[m].Vertices[i].m_color;
					Vertex vert = {
						{ pos.x, pos.y, pos.z },
						{ normal.x, -normal.y, normal.z },
						{ uv.s, uv.t },
						{ col.r, col.g, col.b }
					};

					// Offset Vulkan meshes
					// todo : center before export
					//if (mesh != demoMeshes.skybox) {
						//vert.pos[1] += 1.15f;
					//}

					vertexBuffer.push_back(vert);
				}
			}
			auto result = context.createBuffer(vk::BufferUsageFlagBits::eVertexBuffer, vertexBuffer);
			meshLoader->vertexBuffer.buf = result.buffer;
			meshLoader->vertexBuffer.mem = result.memory;
			std::vector<uint32_t> indexBuffer;
			for (int m = 0; m < meshLoader->m_Entries.size(); m++) {
				int indexBase = indexBuffer.size();
				for (int i = 0; i < meshLoader->m_Entries[m].Indices.size(); i++) {
					indexBuffer.push_back(meshLoader->m_Entries[m].Indices[i] + indexBase);
				}
			}
			result = context.createBuffer(vk::BufferUsageFlagBits::eVertexBuffer, indexBuffer);
			meshLoader->indexBuffer.buf = result.buffer;
			meshLoader->indexBuffer.mem = result.memory;
			meshLoader->indexBuffer.count = indexBuffer.size();

			meshLoaders.push_back(meshLoader);
		}*/






		// Binding description
		bindingDescriptions.resize(1);
		bindingDescriptions[0] =
			vkx::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), vk::VertexInputRate::eVertex);

		// Attribute descriptions
		attributeDescriptions.resize(4);
		// Location 0 : Position
		attributeDescriptions[0] =
			vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, vk::Format::eR32G32B32Sfloat, 0);
		// Location 1 : Normal
		attributeDescriptions[1] =
			vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, vk::Format::eR32G32B32Sfloat, sizeof(float) * 3);
		// Location 2 : Texture coordinates
		attributeDescriptions[2] =
			vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, vk::Format::eR32G32Sfloat, sizeof(float) * 6);
		// Location 3 : Color
		attributeDescriptions[3] =
			vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, vk::Format::eR32G32B32Sfloat, sizeof(float) * 8);

		inputState.vertexBindingDescriptionCount = bindingDescriptions.size();
		inputState.pVertexBindingDescriptions = bindingDescriptions.data();

		inputState.vertexAttributeDescriptionCount = attributeDescriptions.size();
		inputState.pVertexAttributeDescriptions = attributeDescriptions.data();
	}

	void setupDescriptorPool() {
		// Example uses one ubo and one image sampler
		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			vkx::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2),
			vkx::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1)
		};

		vk::DescriptorPoolCreateInfo descriptorPoolInfo =
			vkx::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 2);

		descriptorPool = device.createDescriptorPool(descriptorPoolInfo);
	}

	void setupDescriptorSetLayout() {
		std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkx::descriptorSetLayoutBinding(
				vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eVertex,
				0),
			// Binding 1 : Fragment shader color map image sampler
			vkx::descriptorSetLayoutBinding(
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eFragment,
				1)
		};

		vk::DescriptorSetLayoutCreateInfo descriptorLayout =
			vkx::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), setLayoutBindings.size());

		descriptorSetLayout = device.createDescriptorSetLayout(descriptorLayout);


		vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vkx::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

		pipelineLayout = device.createPipelineLayout(pPipelineLayoutCreateInfo);

	}

	void setupDescriptorSet() {
		vk::DescriptorSetAllocateInfo allocInfo =
			vkx::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

		// Cube map image descriptor
		vk::DescriptorImageInfo texDescriptorCubeMap =
			vkx::descriptorImageInfo(textures.skybox.sampler, textures.skybox.view, vk::ImageLayout::eGeneral);

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vkx::writeDescriptorSet(
				descriptorSet,
				vk::DescriptorType::eUniformBuffer,
				0,
				&uniformData.meshVS.descriptor),
			// Binding 1 : Fragment shader image sampler
			vkx::writeDescriptorSet(
				descriptorSet,
				vk::DescriptorType::eCombinedImageSampler,
				1,
				&texDescriptorCubeMap)
		};

		device.updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void preparePipelines() {
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState;
		inputAssemblyState.topology = vk::PrimitiveTopology::eTriangleList;

		vk::PipelineRasterizationStateCreateInfo rasterizationState =
			vkx::pipelineRasterizationStateCreateInfo(
				vk::PolygonMode::eFill,
				vk::CullModeFlagBits::eBack,
				vk::FrontFace::eClockwise);

		vk::PipelineColorBlendAttachmentState blendAttachmentState;
		blendAttachmentState.colorWriteMask = vkx::fullColorWriteMask();

		vk::PipelineColorBlendStateCreateInfo colorBlendState;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &blendAttachmentState;

		vk::PipelineDepthStencilStateCreateInfo depthStencilState;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = vk::CompareOp::eLessOrEqual;

		vk::PipelineViewportStateCreateInfo viewportState;
		viewportState.scissorCount = 1;
		viewportState.viewportCount = 1;

		vk::PipelineMultisampleStateCreateInfo multisampleState;

		std::vector<vk::DynamicState> dynamicStateEnables = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
		vk::PipelineDynamicStateCreateInfo dynamicState =
			vkx::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), dynamicStateEnables.size());

		// vk::Pipeline for the meshes (armadillo, bunny, etc.)
		// Load shaders
		std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;
		shaderStages[0] = context.loadShader(getAssetPath() + "shaders/vulkanscene/mesh.vert.spv", vk::ShaderStageFlagBits::eVertex);
		shaderStages[1] = context.loadShader(getAssetPath() + "shaders/vulkanscene/mesh.frag.spv", vk::ShaderStageFlagBits::eFragment);

		vk::GraphicsPipelineCreateInfo pipelineCreateInfo =
			vkx::pipelineCreateInfo(pipelineLayout, renderPass);

		pipelineCreateInfo.pVertexInputState = &inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = shaderStages.size();
		pipelineCreateInfo.pStages = shaderStages.data();

		pipelines.models = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];


		// vk::Pipeline for the logos
		shaderStages[0] = context.loadShader(getAssetPath() + "shaders/vulkanscene/logo.vert.spv", vk::ShaderStageFlagBits::eVertex);
		shaderStages[1] = context.loadShader(getAssetPath() + "shaders/vulkanscene/logo.frag.spv", vk::ShaderStageFlagBits::eFragment);
		pipelines.logos = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];


		// vk::Pipeline for the sky sphere (todo)
		rasterizationState.cullMode = vk::CullModeFlagBits::eFront; // Inverted culling
		depthStencilState.depthWriteEnable = VK_FALSE; // No depth writes
		shaderStages[0] = context.loadShader(getAssetPath() + "shaders/vulkanscene/skybox.vert.spv", vk::ShaderStageFlagBits::eVertex);
		shaderStages[1] = context.loadShader(getAssetPath() + "shaders/vulkanscene/skybox.frag.spv", vk::ShaderStageFlagBits::eFragment);
		pipelines.skybox = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];


		// Assign pipelines
		logos->pipeline = pipelines.logos;
		models->pipeline = pipelines.models;
		background->pipeline = pipelines.models;
		skybox->pipeline = pipelines.skybox;

		meshes[0].pipeline = pipelines.models;
		meshes[1].pipeline = pipelines.logos;
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers() {
		// Vertex shader uniform buffer block
		uniformData.meshVS = context.createUniformBuffer(uboVS);
		updateUniformBuffers();
	}

	void updateUniformBuffers() {
		uboVS.projection = camera.matrices.projection;
		uboVS.view = camera.matrices.view;
		//uboVS.model = camera.matrices.skyboxView;
		uboVS.normal = glm::inverseTranspose(uboVS.view * uboVS.model);
		uboVS.lightPos = lightPos;
		uniformData.meshVS.copy(uboVS);
	}

	void prepare() {
		vulkanApp::prepare();
		loadTextures();
		prepareVertices();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		updateDrawCommandBuffers();
		prepared = true;
	}

	virtual void render() {
		if (!prepared)
			return;
		draw();
	}

	virtual void viewChanged() {
		updateTextOverlay(); //disable this
		updateUniformBuffers();
	}

	virtual void getOverlayText(vkx::TextOverlay *textOverlay)
	{
		textOverlay->addText("camera stats:", 5.0f, 70.0f, vkx::TextOverlay::alignLeft);
		textOverlay->addText("rotation(q) w: " + std::to_string(camera.rotation.w), 5.0f, 90.0f, vkx::TextOverlay::alignLeft);
		textOverlay->addText("rotation(q) x: " + std::to_string(camera.rotation.x), 5.0f, 110.0f, vkx::TextOverlay::alignLeft);
		textOverlay->addText("rotation(q) y: " + std::to_string(camera.rotation.y), 5.0f, 130.0f, vkx::TextOverlay::alignLeft);
		textOverlay->addText("rotation(q) z: " + std::to_string(camera.rotation.z), 5.0f, 150.0f, vkx::TextOverlay::alignLeft);

		textOverlay->addText("pos x: " + std::to_string(camera.translation.x), 5.0f, 170.0f, vkx::TextOverlay::alignLeft);
		textOverlay->addText("pos y: " + std::to_string(camera.translation.y), 5.0f, 190.0f, vkx::TextOverlay::alignLeft);
		textOverlay->addText("pos z: " + std::to_string(camera.translation.z), 5.0f, 210.0f, vkx::TextOverlay::alignLeft);
	}

};

VulkanExample *vulkanExample;

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	VulkanExample* example = new VulkanExample();
	example->run();
	delete(example);
	return 0;
}