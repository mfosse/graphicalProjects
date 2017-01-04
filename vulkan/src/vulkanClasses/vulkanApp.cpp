/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "main/global.h"


#include "vulkanApp.h"

using namespace vkx;

//vk::Result vulkanApp::createInstance(bool enableValidation)






vulkanApp::vulkanApp(bool enableValidation) {
	// Check for validation command line flag
	#if defined(_WIN32)
		for (int32_t i = 0; i < __argc; i++) {
			if (__argv[i] == std::string("-validation")) {
				enableValidation = true;
			}
		}
	#elif defined(__ANDROID__)
		// Vulkan library is loaded dynamically on Android
		bool libLoaded = loadVulkanLibrary();
		assert(libLoaded);
	#endif

	#if !defined(__ANDROID__)
		// Android Vulkan initialization is handled in APP_CMD_INIT_WINDOW event
		initVulkan(enableValidation);
	#endif
}




vulkanApp::~vulkanApp() {
	// Clean up Vulkan resources
	swapChain.cleanup();
	if (descriptorPool) {
		device.destroyDescriptorPool(descriptorPool);
	}
	if (!primaryCmdBuffers.empty()) {
		device.freeCommandBuffers(cmdPool, primaryCmdBuffers);
		primaryCmdBuffers.clear();
	}
	if (!drawCmdBuffers.empty()) {
		device.freeCommandBuffers(cmdPool, drawCmdBuffers);
		drawCmdBuffers.clear();
	}
	if (!textCmdBuffers.empty()) {
		device.freeCommandBuffers(cmdPool, textCmdBuffers);
		textCmdBuffers.clear();
	}
	device.destroyRenderPass(renderPass);
	for (uint32_t i = 0; i < framebuffers.size(); i++) {
		device.destroyFramebuffer(framebuffers[i]);
	}

	for (auto& shaderModule : shaderModules) {
		device.destroyShaderModule(shaderModule);
	}
	depthStencil.destroy();

	if (textureLoader) {
		delete textureLoader;
	}

	if (enableTextOverlay) {
		delete textOverlay;
	}

	device.destroySemaphore(semaphores.acquireComplete);
	device.destroySemaphore(semaphores.renderComplete);

	context.destroyContext();

	#if defined(__ANDROID__)
	// todo : android cleanup (if required)
	#else
		//glfwDestroyWindow(window);
		//glfwTerminate();
	#endif
}

void vulkanApp::run() {

	#if defined(_WIN32)
		setupWindow();
	#elif defined(__ANDROID__)
		// Attach vulkan example to global android application state
		state->userData = vulkanExample;
		state->onAppCmd = VulkanExample::handleAppCommand;
		state->onInputEvent = VulkanExample::handleAppInput;
		androidApp = state;
		#elif defined(__linux__)
		setupWindow();
	#endif

	#if !defined(__ANDROID__)
		prepare();
	#endif

	renderLoop();

	// Once we exit the render loop, wait for everything to become idle before proceeding to the destructor.
	queue.waitIdle();
	device.waitIdle();
}

void vulkanApp::initVulkan(bool enableValidation) {
	context.createContext(enableValidation);
	swapChain.setContext(this->context);

	// Find a suitable depth format
	depthFormat = getSupportedDepthFormat(physicalDevice);

	// Create synchronization objects
	vk::SemaphoreCreateInfo semaphoreCreateInfo;
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
	semaphores.acquireComplete = device.createSemaphore(semaphoreCreateInfo);
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	semaphores.renderComplete = device.createSemaphore(semaphoreCreateInfo);

	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	submitInfo = vk::SubmitInfo();
	submitInfo.pWaitDstStageMask = &submitPipelineStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &semaphores.acquireComplete;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphores.renderComplete;
}









/* CROSS PLATFORM */

void vulkanApp::setupWindow()
{

	SDL_Window *mySDLWindow;
	SDL_SysWMinfo windowInfo;

	std::string windowTitle = "test";

	SDL_Init(SDL_INIT_EVERYTHING);

	mySDLWindow = SDL_CreateWindow(
		windowTitle.c_str(),
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		size.width,// width
		size.height,// height
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
	);

	SDL_VERSION(&windowInfo.version); // initialize info structure with SDL version info

	if (SDL_GetWindowWMInfo(mySDLWindow, &windowInfo)) {
		std::cout << "SDL window initialization success." << std::endl;
	} else {
		std::cout << "ERROR!: SDL window init: Couldn't get window information: " << SDL_GetError() << std::endl;
	}

	/* WINDOWS */
	#if defined(_WIN32)
		// get hinstance from sdl window
		//HINSTANCE SDLhinstance = GetModuleHandle(NULL);// not sure how this works? (gets current window?) might limit number of windows later
		//this->windowInstance = SDLhinstance;

		// get window handle from sdl window
		//HWND SDLhandle = windowInfo.info.win.window;
		//this->windowHandle = SDLhandle;

	#elif defined(__linux__)
		// TODO
	#endif

	this->SDLWindow = mySDLWindow;
	this->windowInfo = windowInfo;

	swapChain.createSurface(this->SDLWindow, this->windowInfo);
}





// A default draw implementation
void vulkanApp::draw() {
	// Get next image in the swap chain (back/front buffer)
	prepareFrame();
	// Execute the compiled command buffer for the current swap chain image
	drawCurrentCommandBuffer();
	// Push the rendered frame to the surface
	submitFrame();
}



// Pure virtual render function (override in derived class)
void vulkanApp::render() {
	if (!prepared) {
		return;
	}
	draw();
}

void vulkanApp::update(float deltaTime) {

	bool updateView = true;

	
	// z-up translations
	if (!keyStates.shift) {
		if (keyStates.w) {
			camera.translateLocal(glm::vec3(0.0f, camera.movementSpeed, 0.0f));
		}
		if (keyStates.s) {
			camera.translateLocal(glm::vec3(0.0f, -camera.movementSpeed, 0.0f));
		}
		if (keyStates.a) {
			camera.translateLocal(glm::vec3(-camera.movementSpeed, 0.0f, 0.0f));
		}
		if (keyStates.d) {
			camera.translateLocal(glm::vec3(camera.movementSpeed, 0.0f, 0.0f));
		}
		if (keyStates.q) {
			camera.translateLocal(glm::vec3(0.0f, 0.0f, -camera.movementSpeed));
		}
		if (keyStates.e) {
			camera.translateLocal(glm::vec3(0.0f, 0.0f, camera.movementSpeed));
		}
	} else {
		if (keyStates.w) {
			camera.translateWorld(glm::vec3(0.0f, camera.movementSpeed, 0.0f));
		}
		if (keyStates.s) {
			camera.translateWorld(glm::vec3(0.0f, -camera.movementSpeed, 0.0f));
		}
		if (keyStates.a) {
			camera.translateWorld(glm::vec3(-camera.movementSpeed, 0.0f, 0.0f));
		}
		if (keyStates.d) {
			camera.translateWorld(glm::vec3(camera.movementSpeed, 0.0f, 0.0f));
		}
		if (keyStates.q) {
			camera.translateWorld(glm::vec3(0.0f, 0.0f, -camera.movementSpeed));
		}
		if (keyStates.e) {
			camera.translateWorld(glm::vec3(0.0f, 0.0f, camera.movementSpeed));
		}
	}



	// z-up rotations
	camera.rotationSpeed = -0.005f;

	//if (mouse.leftMouseButton.state) {
	//	float angle = -mouse.delta.x*camera.rotationSpeed;
	//	glm::vec3 axis(0.0, 0.0, 1.0);
	//	glm::quat q = glm::angleAxis(angle, axis);

	//	angle = -mouse.delta.x*camera.rotationSpeed;
	//	glm
	//	camera.rotateWorld(q);
	//}

	if (mouse.leftMouseButton.state) {
		camera.rotateWorldZ(-mouse.delta.x*camera.rotationSpeed);
		camera.rotateWorldX(-mouse.delta.y*camera.rotationSpeed);

		//camera.rotateWorld(glm::vec3(-mouse.delta.y*camera.rotationSpeed, 0, -mouse.delta.x*camera.rotationSpeed));
		//camera.rotateWorld(glm::vec3(-mouse.delta.y*camera.rotationSpeed, 0, -mouse.delta.x*camera.rotationSpeed));
	}


	camera.rotationSpeed = -0.02f;

	
	if (keyStates.up_arrow) {
		//camera.rotateLocal(glm::vec3(camera.rotationSpeed, 0, 0));
	}
	if (keyStates.down_arrow) {
		//camera.rotateLocal(glm::vec3(-camera.rotationSpeed, 0, 0));
	}

	if (!keyStates.shift) {
		if (keyStates.left_arrow) {
			camera.rotateWorldZ(camera.rotationSpeed);
		}
		if (keyStates.right_arrow) {
			camera.rotateWorldZ(-camera.rotationSpeed);
		}
	} else {
		//if (keyStates.left_arrow) {
		//	camera.rotateWorldY(-camera.rotationSpeed);
		//}
		//if (keyStates.right_arrow) {
		//	camera.rotateWorldY(camera.rotationSpeed);
		//}
	}



	/*if (camera.changed) {
		camera.update();
	}*/
	
	if (updateView) {
		viewChanged();
	}

}


void vulkanApp::viewChanged() {
}

void vulkanApp::windowResized() {
}


void vulkanApp::setupDepthStencil() {
	depthStencil.destroy();

	vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
	vk::ImageCreateInfo image;
	image.imageType = vk::ImageType::e2D;
	image.extent = vk::Extent3D{ size.width, size.height, 1 };
	image.format = depthFormat;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc;
	depthStencil = context.createImage(image, vk::MemoryPropertyFlagBits::eDeviceLocal);

	context.withPrimaryCommandBuffer([&](const vk::CommandBuffer& setupCmdBuffer) {
		setImageLayout(
			setupCmdBuffer,
			depthStencil.image,
			aspect,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal);
	});


	vk::ImageViewCreateInfo depthStencilView;
	depthStencilView.viewType = vk::ImageViewType::e2D;
	depthStencilView.format = depthFormat;
	depthStencilView.subresourceRange.aspectMask = aspect;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.layerCount = 1;
	depthStencilView.image = depthStencil.image;
	depthStencil.view = device.createImageView(depthStencilView);
}

void vulkanApp::setupFrameBuffer() {
	// Recreate the frame buffers
	if (!framebuffers.empty()) {
		for (uint32_t i = 0; i < framebuffers.size(); i++) {
			device.destroyFramebuffer(framebuffers[i]);
		}
		framebuffers.clear();
	}

	vk::ImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depthStencil.view;

	vk::FramebufferCreateInfo framebufferCreateInfo;
	framebufferCreateInfo.renderPass = renderPass;
	framebufferCreateInfo.attachmentCount = 2;
	framebufferCreateInfo.pAttachments = attachments;
	framebufferCreateInfo.width = size.width;
	framebufferCreateInfo.height = size.height;
	framebufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	framebuffers = swapChain.createFramebuffers(framebufferCreateInfo);
}

void vulkanApp::setupRenderPass() {
	if (renderPass) {
		device.destroyRenderPass(renderPass);
	}

	std::vector<vk::AttachmentDescription> attachments;
	attachments.resize(2);

	// Color attachment
	//attachments[0].format = colorformat;
	attachments[0].format = swapChain.colorFormat;
	attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
	attachments[0].initialLayout = vk::ImageLayout::eUndefined;
	attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
	attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].initialLayout = vk::ImageLayout::eUndefined;
	attachments[1].finalLayout = vk::ImageLayout::eUndefined;

	// Only one depth attachment, so put it first in the references
	vk::AttachmentReference depthReference;
	depthReference.attachment = 1;
	depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	std::vector<vk::AttachmentReference> colorAttachmentReferences;
	{
		vk::AttachmentReference colorReference;
		colorReference.attachment = 0;
		colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachmentReferences.push_back(colorReference);
	}

	std::vector<vk::SubpassDescription> subpasses;
	std::vector<vk::SubpassDependency> subpassDependencies;
	{
		vk::SubpassDependency dependency;
		dependency.srcSubpass = 0;
		dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
		dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
		dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
		subpassDependencies.push_back(dependency);

		vk::SubpassDescription subpass;
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.pDepthStencilAttachment = &depthReference;
		subpass.colorAttachmentCount = colorAttachmentReferences.size();
		subpass.pColorAttachments = colorAttachmentReferences.data();
		subpasses.push_back(subpass);
	}

	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = subpasses.size();
	renderPassInfo.pSubpasses = subpasses.data();
	renderPassInfo.dependencyCount = subpassDependencies.size();
	renderPassInfo.pDependencies = subpassDependencies.data();
	renderPass = device.createRenderPass(renderPassInfo);
}


void vulkanApp::populateSubCommandBuffers(std::vector<vk::CommandBuffer>& cmdBuffers, std::function<void(const vk::CommandBuffer& commandBuffer)> f) {
	if (!cmdBuffers.empty()) {
		context.trashCommandBuffers(cmdBuffers);
	}

	vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
	cmdBufAllocateInfo.commandPool = context.getCommandPool();
	cmdBufAllocateInfo.commandBufferCount = swapChain.imageCount;
	cmdBufAllocateInfo.level = vk::CommandBufferLevel::eSecondary;
	cmdBuffers = device.allocateCommandBuffers(cmdBufAllocateInfo);

	vk::CommandBufferInheritanceInfo inheritance;
	inheritance.renderPass = renderPass;
	inheritance.subpass = 0;
	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse;
	beginInfo.pInheritanceInfo = &inheritance;
	for (size_t i = 0; i < swapChain.imageCount; ++i) {
		currentBuffer = i;
		inheritance.framebuffer = framebuffers[i];
		vk::CommandBuffer& cmdBuffer = cmdBuffers[i];
		cmdBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
		cmdBuffer.begin(beginInfo);
		f(cmdBuffer);
		cmdBuffer.end();
	}
	currentBuffer = 0;
}

void vulkanApp::updatePrimaryCommandBuffer(const vk::CommandBuffer& cmdBuffer) {

}

void vulkanApp::updateDrawCommandBuffers() {
	populateSubCommandBuffers(drawCmdBuffers, [&](const vk::CommandBuffer& cmdBuffer) {
		updateDrawCommandBuffer(cmdBuffer);
	});
	primaryCmdBuffersDirty = true;
}

void vulkanApp::drawCurrentCommandBuffer(const vk::Semaphore& semaphore) {
	vk::Fence fence = swapChain.getSubmitFence();

	{
		uint32_t fenceIndex = currentBuffer;
		context.dumpster.push_back([fenceIndex, this] {
			swapChain.clearSubmitFence(fenceIndex);
		});
	}

	// Command buffer(s) to be sumitted to the queue
	std::vector<vk::Semaphore> waitSemaphores{ { semaphore == vk::Semaphore() ? semaphores.acquireComplete : semaphore } };
	std::vector<vk::PipelineStageFlags> waitStages{ submitPipelineStages };
	if (semaphores.transferComplete) {
		auto transferComplete = semaphores.transferComplete;
		semaphores.transferComplete = vk::Semaphore();
		waitSemaphores.push_back(transferComplete);
		waitStages.push_back(vk::PipelineStageFlagBits::eTransfer);
		context.dumpster.push_back([transferComplete, this] {
			device.destroySemaphore(transferComplete);
		});
	}

	context.emptyDumpster(fence);

	vk::Semaphore transferPending;
	std::vector<vk::Semaphore> signalSemaphores{ { semaphores.renderComplete } };
	if (!pendingUpdates.empty()) {
		transferPending = device.createSemaphore(vk::SemaphoreCreateInfo());
		signalSemaphores.push_back(transferPending);
	}

	{
		vk::SubmitInfo submitInfo;
		submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.pWaitDstStageMask = waitStages.data();
		submitInfo.signalSemaphoreCount = signalSemaphores.size();
		submitInfo.pSignalSemaphores = signalSemaphores.data();
		submitInfo.commandBufferCount = 1;
		if (primaryCmdBuffers.size() < 1) {
			return;
		}

		submitInfo.pCommandBuffers = &primaryCmdBuffers[currentBuffer];
		// Submit to queue
		queue.submit(submitInfo, fence);
	}

	executePendingTransfers(transferPending);
	context.recycle();
}


void vulkanApp::executePendingTransfers(vk::Semaphore transferPending) {
	if (!pendingUpdates.empty()) {
		vk::Fence transferFence = device.createFence(vk::FenceCreateInfo());
		semaphores.transferComplete = device.createSemaphore(vk::SemaphoreCreateInfo());
		assert(transferPending);
		assert(semaphores.transferComplete);
		// Command buffers store a reference to the
		// frame buffer inside their render pass info
		// so for static usage without having to rebuild
		// them each frame, we use one per frame buffer
		vk::CommandBuffer transferCmdBuffer;
		{
			vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
			cmdBufAllocateInfo.commandPool = cmdPool;
			cmdBufAllocateInfo.commandBufferCount = 1;
			transferCmdBuffer = device.allocateCommandBuffers(cmdBufAllocateInfo)[0];
		}


		{
			vk::CommandBufferBeginInfo cmdBufferBeginInfo;
			cmdBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
			transferCmdBuffer.begin(cmdBufferBeginInfo);
			for (const auto& update : pendingUpdates) {
				transferCmdBuffer.updateBuffer(update.buffer, update.offset, update.size, update.data);
			}
			transferCmdBuffer.end();
		}

		{
			vk::PipelineStageFlags stageFlagBits = vk::PipelineStageFlagBits::eAllCommands;
			vk::SubmitInfo transferSubmitInfo;
			transferSubmitInfo.pWaitDstStageMask = &stageFlagBits;
			transferSubmitInfo.pWaitSemaphores = &transferPending;
			transferSubmitInfo.signalSemaphoreCount = 1;
			transferSubmitInfo.pSignalSemaphores = &semaphores.transferComplete;
			transferSubmitInfo.waitSemaphoreCount = 1;
			transferSubmitInfo.commandBufferCount = 1;
			transferSubmitInfo.pCommandBuffers = &transferCmdBuffer;
			queue.submit(transferSubmitInfo, transferFence);
		}

		context.recycler.push({ transferFence, [transferPending, transferCmdBuffer, this] {
			device.destroySemaphore(transferPending);
			device.freeCommandBuffers(cmdPool, transferCmdBuffer);
		} });
		pendingUpdates.clear();
	}
}


//prepare goes here
void vulkanApp::prepare() {
	if (enableValidation) {
		debug::setupDebugging(instance, vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning);
	}
	if (enableDebugMarkers) {
		debug::marker::setup(device);
	}
	cmdPool = context.getCommandPool();

	swapChain.create(size, enableVsync);
	setupDepthStencil();
	setupRenderPass();
	setupRenderPassBeginInfo();
	setupFrameBuffer();

	// Create a simple texture loader class
	// todo: move this into asset manager class
	textureLoader = new TextureLoader(this->context);


	// todo: add mesh loader here// important

	#if defined(__ANDROID__)
	textureLoader->assetManager = androidApp->activity->assetManager;
	#endif
	if (enableTextOverlay) {
		// Load the text rendering shaders
		textOverlay = new TextOverlay(this->context,
			size.width,
			size.height,
			renderPass);
		updateTextOverlay();
	}
}

// todo: remove this
MeshBuffer vulkanApp::loadMesh(const std::string& filename, const std::vector<VertexLayout>& vertexLayout, float scale) {
	MeshLoader loader(this->context, this->assetManager);
	#if defined(__ANDROID__)
	loader.assetManager = androidApp->activity->assetManager;
	#endif
	loader.load(filename);
	assert(loader.m_Entries.size() > 0);
	loader.createMeshBuffer(this->context, vertexLayout, scale);
	return loader.combinedBuffer;
}


void vulkanApp::updateKeyboardMouseInfo() {

	
	// todo: move this somewhere else
	SDL_Event e;
	
	// poll events
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) {
			quit = true;
			//SDL_DestroyWindow(this->SDLWindow);
			//SDL_Quit();
		}

		if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
			bool state = (e.type == SDL_KEYDOWN) ? true : false;
			switch (e.key.keysym.sym) {
				case SDLK_ESCAPE:
					quit = true;
					break;
				case SDLK_w:
					keyStates.w = state;
					break;
				case SDLK_s:
					keyStates.s = state;
					break;
				case SDLK_a:
					keyStates.a = state;
					break;
				case SDLK_d:
					keyStates.d = state;
					break;
				case SDLK_q:
					keyStates.q = state;
					break;
				case SDLK_e:
					keyStates.e = state;
					break;
				case SDLK_SPACE:
					keyStates.space = state;
					break;
				case SDLK_UP:
					keyStates.up_arrow = state;
					break;
				case SDLK_DOWN:
					keyStates.down_arrow = state;
					break;
				case SDLK_LEFT:
					keyStates.left_arrow = state;
					break;
				case SDLK_RIGHT:
					keyStates.right_arrow = state;
					break;
				case SDLK_LSHIFT:
					keyStates.shift = state;
					break;
				// another wsadqe
				case SDLK_i:
					keyStates.i = state;
					break;
				case SDLK_k:
					keyStates.k = state;
					break;
				case SDLK_j:
					keyStates.j = state;
					break;
				case SDLK_l:
					keyStates.l = state;
					break;
				case SDLK_u:
					keyStates.u = state;
					break;
				case SDLK_o:
					keyStates.o = state;
					break;

				default:
					break;
			}
		}

		if (e.type == SDL_MOUSEMOTION) {
			mouse.delta.x = e.motion.xrel;
			mouse.delta.y = e.motion.yrel;
			mouse.current.x = e.motion.x;
			mouse.current.y = e.motion.y;
			mouse.movedThisFrame = true;// why does sdl2 not report no movement?
		}

		// should be if else's / switch case

		if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
			bool state = (e.type == SDL_MOUSEBUTTONDOWN) ? true : false;

			// todo: implement pressed and released coords
			if (e.button.button == SDL_BUTTON_LEFT) {
				mouse.leftMouseButton.state = state;
			} else if (e.button.button == SDL_BUTTON_MIDDLE) {
				mouse.middleMouseButton.state = state;
			} else if (e.button.button == SDL_BUTTON_RIGHT) {
				mouse.rightMouseButton.state = state;
			}
			//mouse.delta.x = e.motion.xrel;
			//mouse.delta.y = e.motion.yrel;
			//mouse.current.x = e.motion.x;
			//mouse.current.y = e.motion.y;
		}
	}
	if (!mouse.movedThisFrame) {
		mouse.delta.x = 0;
		mouse.delta.y = 0;
	}
	mouse.movedThisFrame = false;

}




//should not be here
void vulkanApp::renderLoop() {

	// when the frame started
	auto tFrameStart = std::chrono::high_resolution_clock::now();
	// the current time
	auto tNow = std::chrono::high_resolution_clock::now();
	// the time it took for the frame to render, i.e. tCurrent - tFrameStart
	//auto tFrameTime = std::chrono::high_resolution_clock::now();

	auto tStart = std::chrono::high_resolution_clock::now();
	//auto tEnd = std::chrono::high_resolution_clock::now();

	auto tStart2 = std::chrono::high_resolution_clock::now();
	auto tEnd2 = std::chrono::high_resolution_clock::now();

	std::vector<double> times;

	while (!quit) {


		// get current time
		tNow = std::chrono::high_resolution_clock::now();
		// number of frames that have been rendered
		frameCounter++;

		// the time it took to render the frame
		auto tFrameTime = std::chrono::duration<float, std::milli>(tNow - tFrameStart);
		
		// set FPS
		double FPS = 60.0;
		std::chrono::duration<double, std::milli> minWorkTime(1000.0 / FPS);

		//if (tFrameTime < minWorkTime) {
			std::this_thread::sleep_for(minWorkTime - tFrameTime);
		//}


		

		

		// calculate new frame time after sleeping
		tNow = std::chrono::high_resolution_clock::now();
		tFrameTime = std::chrono::duration<double, std::milli>(tNow - tFrameStart);

		frameTimer = tFrameTime.count() / 1000.0;
		lastFPS = 1.0 / frameTimer;


		// start of frame
		tFrameStart = std::chrono::high_resolution_clock::now();



		//// the time it took to render the frame
		//auto tFrameTime = std::chrono::duration<float, std::milli>(tCurrent - tFrameStart).count();
		//// in ms
		//auto tFrameTimeMS = tFrameTime / 1000.0f;

		//// start of frame
		//tFrameStart = tCurrent;



		




		//// average over 1 second:

		//frameCounter2++;// reset every second
		//
		//// the amount of time that has passed
		//auto tDiff = std::chrono::duration<float, std::milli>(tCurrent - tStart).count();

		//auto tDiffMS = tDiff / 1000.0f;

		//// if more than a second has passed
		//if (tDiff >= 1000.0f) {

		//	// get average frame time
		//	// 1000 / (number of frames that occured in the last second)
		//	frameTimer = tDiffMS / frameCounter2;
		//	lastFPS = 1.0f / frameTimer;

		//	tStart = tCurrent;
		//	frameCounter2 = 0;
		//}

		////frameTimer = tFrameTimeMS;
		////lastFPS = 1.0f / frameTimer;

		//

		//tStart2 = std::chrono::high_resolution_clock::now();

		//tEnd2 = std::chrono::high_resolution_clock::now();




		//// the time it took to render the frame
		//auto tFrameTime2 = std::chrono::duration<double, std::milli>(tEnd2 - tStart2)/*.count()*/;
		//// in ms
		////auto tFrameTimeMS2 = tFrameTime2 / 1000.0f;

		////frameTimer = tFrameTime2.count() / 100.0;


		//double FPS = 60.0;
		////double numOfMS = (1000.0f / FPS);// this doesn't seem to work properly
		//std::chrono::duration<double, std::milli> minWorkTime(1000.0 / FPS);

		//if (tFrameTime2 < minWorkTime) {
		//	//double extraTime = (numOfMS - tFrameTime2) * 100000.0;
		//	//std::this_thread::sleep_for(std::chrono::microseconds((int)extraTime));
		//	std::this_thread::sleep_for(minWorkTime - tFrameTime2);

		//}

		//tEnd2 = std::chrono::high_resolution_clock::now();

		//tFrameTime2 = std::chrono::duration<double, std::milli>(tEnd2 - tStart2)/*.count()*/;
		//// in ms
		////auto tFrameTimeMS2 = tFrameTime2 / 1000.0f;

		//frameTimer = tFrameTime2.count() / 1000.0;



		updateKeyboardMouseInfo();
		update(0);

		//updateTextOverlay();

		// actually draw models
		updateDrawCommandBuffers();

		render();



		
		
	
	}
	SDL_DestroyWindow(this->SDLWindow);
	SDL_Quit();

}




// NOT HERE
std::string vulkanApp::getWindowTitle() {
	std::string device(deviceProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device + " - " + std::to_string(frameCounter) + " fps";
	return windowTitle;
}

const std::string& vulkanApp::getAssetPath() {
	return vkx::getAssetPath();
}
// END



vk::SubmitInfo vulkanApp::prepareSubmitInfo(
	const std::vector<vk::CommandBuffer>& commandBuffers,
	vk::PipelineStageFlags *pipelineStages) {

	vk::SubmitInfo submitInfo;
	submitInfo.pWaitDstStageMask = pipelineStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &semaphores.acquireComplete;
	submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	submitInfo.pCommandBuffers = commandBuffers.data();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphores.renderComplete;
	return submitInfo;
}

void vulkanApp::updateTextOverlay() {
	if (!enableTextOverlay) {
		return;
	}

	// begin
	textOverlay->beginTextUpdate();

	// get text
	getOverlayText(textOverlay);

	// end
	textOverlay->endTextUpdate();

	context.trashCommandBuffers(textCmdBuffers);
	populateSubCommandBuffers(textCmdBuffers, [&](const vk::CommandBuffer& cmdBuffer) {
		textOverlay->writeCommandBuffer(cmdBuffer);
	});
	primaryCmdBuffersDirty = true;
}

void vulkanApp::getOverlayText(vkx::TextOverlay *textOverlay) {
	// Can be overriden in derived class
}

void vulkanApp::prepareFrame() {
	if (primaryCmdBuffersDirty) {
		buildCommandBuffers();
	}
	// Acquire the next image from the swap chain
	currentBuffer = swapChain.acquireNextImage(semaphores.acquireComplete);
}

void vulkanApp::submitFrame() {
	swapChain.queuePresent(queue, semaphores.renderComplete);
}






































void vulkanApp::setupRenderPassBeginInfo() {
	clearValues.clear();
	clearValues.push_back(vkx::clearColor(glm::vec4(0.1, 0.1, 0.1, 1.0)));
	clearValues.push_back(vk::ClearDepthStencilValue{ 1.0f, 0 });

	renderPassBeginInfo = vk::RenderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.extent = size;
	renderPassBeginInfo.clearValueCount = clearValues.size();
	renderPassBeginInfo.pClearValues = clearValues.data();
}

void vulkanApp::buildCommandBuffers() {
	if (drawCmdBuffers.empty()) {
		throw std::runtime_error("Draw command buffers have not been populated.");
	}
	context.trashCommandBuffers(primaryCmdBuffers);

	// FIXME find a better way to ensure that the draw and text buffers are no longer in use before 
	// executing them within this command buffer.
	queue.waitIdle();

	// Destroy command buffers if already present
	if (primaryCmdBuffers.empty()) {
		// Create one command buffer per image in the swap chain

		// Command buffers store a reference to the
		// frame buffer inside their render pass info
		// so for static usage without having to rebuild
		// them each frame, we use one per frame buffer
		vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
		cmdBufAllocateInfo.commandPool = cmdPool;
		cmdBufAllocateInfo.commandBufferCount = swapChain.imageCount;
		primaryCmdBuffers = device.allocateCommandBuffers(cmdBufAllocateInfo);
	}

	vk::CommandBufferBeginInfo cmdBufInfo;
	for (size_t i = 0; i < swapChain.imageCount; ++i) {
		const auto& cmdBuffer = primaryCmdBuffers[i];
		cmdBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
		cmdBuffer.begin(cmdBufInfo);

		// Let child classes execute operations outside the renderpass, like buffer barriers or query pool operations
		updatePrimaryCommandBuffer(cmdBuffer);

		renderPassBeginInfo.framebuffer = framebuffers[i];
		cmdBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eSecondaryCommandBuffers);
		if (!drawCmdBuffers.empty()) {
			cmdBuffer.executeCommands(drawCmdBuffers[i]);
		}
		if (enableTextOverlay && !textCmdBuffers.empty() && textOverlay && textOverlay->visible) {
			cmdBuffer.executeCommands(textCmdBuffers[i]);
		}
		cmdBuffer.endRenderPass();
		cmdBuffer.end();
	}
	primaryCmdBuffersDirty = false;
}