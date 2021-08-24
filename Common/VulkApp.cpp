#include "VulkApp.h"

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return VulkApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

VulkApp* VulkApp::mApp = nullptr;
VulkApp* VulkApp::GetApp()
{
	return mApp;
}

VulkApp::VulkApp(HINSTANCE hInstance)
	: mhAppInst(hInstance)
{
	// Only one D3DApp can be constructed.
	assert(mApp == nullptr);
	mApp = this;
}

VulkApp::~VulkApp()
{
	cleanupVulkan();
}

HINSTANCE VulkApp::AppInst()const
{
	return mhAppInst;
}

HWND VulkApp::MainWnd()const
{
	return mhMainWnd;
}

float VulkApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}


int VulkApp::Run()
{
	MSG msg = { 0 };

	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFrameStats();
				Update(mTimer);
				Draw(mTimer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

bool VulkApp::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitVulkan())
		return false;

	// Do the initial resize code.
	OnResize();

	return true;
}


void VulkApp::OnResize()
{
	DestroySwapchain();
	CreateSwapchain();
}

LRESULT VulkApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (mDevice!=VK_NULL_HANDLE)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
			//Set4xMsaaState(!m4xMsaaState);

		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool VulkApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}


bool VulkApp::InitVulkan() {
	std::vector<const char*> requiredExtensions{ "VK_KHR_surface",VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
	std::vector<const char*> requiredLayers{ "VK_LAYER_KHRONOS_validation" };
	mInstance = initInstance(requiredExtensions, requiredLayers);
	mSurface = initSurface(mInstance, mhAppInst, mhMainWnd);
	mPhysicalDevice = choosePhysicalDevice(mInstance, mSurface, mQueues);
	vkGetPhysicalDeviceProperties(mPhysicalDevice, &mDeviceProperties);
	vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);
	vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mDeviceFeatures);
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &mSurfaceCaps);
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
	mSurfaceFormats.resize(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, mSurfaceFormats.data());
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, nullptr);
	mPresentModes.resize(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, mPresentModes.data());
	
	mNumSamples = getMaxUsableSampleCount(mDeviceProperties);
	std::vector<const char*> deviceExtensions{ "VK_KHR_swapchain" };
	VkPhysicalDeviceFeatures enabledFeatures{};
	if (mDeviceFeatures.samplerAnisotropy)
		enabledFeatures.samplerAnisotropy = VK_TRUE;
	if (mDeviceFeatures.sampleRateShading)
		enabledFeatures.sampleRateShading = VK_TRUE;
	
	if (mGeometryShader && mDeviceFeatures.geometryShader)
		enabledFeatures.geometryShader = VK_TRUE;
	if (mAllowWireframe && mDeviceFeatures.fillModeNonSolid) {
		enabledFeatures.fillModeNonSolid = VK_TRUE;
	}
	mDevice = initDevice(mPhysicalDevice, deviceExtensions, mQueues, enabledFeatures);

	mGraphicsQueue = getDeviceQueue(mDevice, mQueues.graphicsQueueFamily);
	mPresentQueue = getDeviceQueue(mDevice, mQueues.presentQueueFamily);
	mComputeQueue = getDeviceQueue(mDevice, mQueues.computeQueueFamily);

	mPresentMode = chooseSwapchainPresentMode(mPresentModes);
	mSwapchainFormat = chooseSwapchainFormat(mSurfaceFormats);
	vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, mSwapchainFormat.format, &mFormatProperties);
	uint32_t swapChainImageCount = getSwapchainImageCount(mSurfaceCaps);
	mPresentComplete = initSemaphore(mDevice);
	mRenderComplete = initSemaphore(mDevice);
	mCommandPool = initCommandPool(mDevice, mQueues.graphicsQueueFamily);
	mCommandBuffer = initCommandBuffer(mDevice, mCommandPool);

	initCommandPools(mDevice, swapChainImageCount, mQueues.graphicsQueueFamily, mCommandPools);
	initCommandBuffers(mDevice, mCommandPools, mCommandBuffers);
	VkDevice device = mDevice;
	pvkAcquireNextImage = (PFN_vkAcquireNextImageKHR)vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR");
	assert(pvkAcquireNextImage);
	pvkQueuePresent = (PFN_vkQueuePresentKHR)vkGetDeviceProcAddr(device, "vkQueuePresentKHR");
	assert(pvkQueuePresent);
	pvkQueueSubmit = (PFN_vkQueueSubmit)vkGetDeviceProcAddr(device, "vkQueueSubmit");
	assert(pvkQueueSubmit);
	pvkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)vkGetDeviceProcAddr(device, "vkBeginCommandBuffer");
	assert(pvkBeginCommandBuffer);
	pvkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)vkGetDeviceProcAddr(device, "vkCmdBeginRenderPass");
	assert(pvkCmdBeginRenderPass);
	pvkCmdBindPipeline = (PFN_vkCmdBindPipeline)vkGetDeviceProcAddr(device, "vkCmdBindPipeline");
	assert(pvkCmdBindPipeline);
	pvkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)vkGetDeviceProcAddr(device, "vkCmdBindVertexBuffers");
	assert(pvkCmdBindVertexBuffers);
	pvkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)vkGetDeviceProcAddr(device, "vkCmdBindIndexBuffer");
	assert(pvkCmdBindIndexBuffer);
	pvkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)vkGetDeviceProcAddr(device, "vkCmdDrawIndexed");
	assert(pvkCmdDrawIndexed);
	pvkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)vkGetDeviceProcAddr(device, "vkCmdEndRenderPass");
	assert(pvkCmdEndRenderPass);
	pvkEndCommandBuffer = (PFN_vkEndCommandBuffer)vkGetDeviceProcAddr(device, "vkEndCommandBuffer");
	assert(pvkEndCommandBuffer);
	pvkQueueWaitIdle = (PFN_vkQueueWaitIdle)vkGetDeviceProcAddr(device, "vkQueueWaitIdle");
	assert(pvkQueueWaitIdle);
	pvkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)vkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets");
	assert(pvkCmdBindDescriptorSets);
	pvkCmdSetViewport = (PFN_vkCmdSetViewport)vkGetDeviceProcAddr(device, "vkCmdSetViewport");
	assert(pvkCmdSetViewport);
	pvkCmdSetScissor = (PFN_vkCmdSetScissor)vkGetDeviceProcAddr(device, "vkCmdSetScissor");
	mSubmitInfo.pWaitDstStageMask = &mSubmitPipelineStages;
	mSubmitInfo.waitSemaphoreCount = 1;
	mSubmitInfo.pWaitSemaphores = &mPresentComplete;
	mSubmitInfo.signalSemaphoreCount = 1;
	mSubmitInfo.pSignalSemaphores = &mRenderComplete;
	mSubmitInfo.commandBufferCount = 1;
	mRenderPassBeginInfo.clearValueCount = sizeof(mClearValues) / sizeof(mClearValues[0]);
	mRenderPassBeginInfo.pClearValues = mClearValues;
	mPresentInfo.swapchainCount = 1;
	mPresentInfo.pImageIndices = &mIndex;
	mPresentInfo.pWaitSemaphores = &mRenderComplete;
	mPresentInfo.waitSemaphoreCount = 1;
	return true;
}



void VulkApp::cleanupVulkan() {
	DestroySwapchain();
	cleanupSwapchain(mDevice, mSwapchain);
	cleanupCommandBuffers(mDevice, mCommandPools, mCommandBuffers);
	cleanupCommandPools(mDevice, mCommandPools);
	cleanupCommandBuffer(mDevice,mCommandPool, mCommandBuffer);
	cleanupCommandPool(mDevice, mCommandPool);

	cleanupSemaphore(mDevice,mRenderComplete);
	cleanupSemaphore(mDevice,mPresentComplete);
	cleanupDevice(mDevice);
	cleanupSurface(mInstance, mSurface);
	cleanupInstance(mInstance);
}

void VulkApp::CreateSwapchain() {
	VkSwapchainKHR oldSwapchain = mSwapchain;
	
	mSwapchain = initSwapchain(mDevice, mSurface, mClientWidth, mClientHeight, mSurfaceCaps, mPresentMode, mSwapchainFormat, mSwapchainExtent,oldSwapchain);
	if (oldSwapchain != VK_NULL_HANDLE) {
		cleanupSwapchain(mDevice, oldSwapchain);
	}
	getSwapchainImages(mDevice, mSwapchain, mSwapchainImages);
	initSwapchainImageViews(mDevice, mSwapchainImages, mSwapchainFormat.format, mSwapchainImageViews);
	
	if (mDepthBuffer) {
		initDepthImage(mDevice, mDepthFormat, mFormatProperties, mMemoryProperties, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mMSAA? mNumSamples : VK_SAMPLE_COUNT_1_BIT, mClientWidth, mClientHeight, mDepthImage);
	}
	if (mMSAA) {
		initMSAAImage(mDevice, mSwapchainFormat.format, mFormatProperties, mMemoryProperties, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mClientWidth, mClientHeight, mNumSamples, mMsaaImage);
		if (mDepthBuffer) {
			mRenderPass = initMSAARenderPass(mDevice, mSwapchainFormat.format, mDepthFormat, mNumSamples);
			initMSAAFramebuffers(mDevice, mRenderPass, mSwapchainImageViews, mDepthImage.imageView, mMsaaImage.imageView, mClientWidth, mClientHeight, mFramebuffers);
		}
		else {
			mRenderPass = initMSAARenderPass(mDevice, mSwapchainFormat.format, mNumSamples);
			initMSAAFramebuffers(mDevice, mRenderPass, mSwapchainImageViews, mMsaaImage.imageView, mClientWidth, mClientHeight, mFramebuffers);
		}
	}
	else {
		if (mDepthBuffer) {
			mRenderPass = initRenderPass(mDevice, mSwapchainFormat.format, mDepthFormat);
			initFramebuffers(mDevice, mRenderPass, mSwapchainImageViews, mDepthImage.imageView, mClientWidth, mClientHeight, mFramebuffers);
		}
		else {
			mRenderPass = initRenderPass(mDevice, mSwapchainFormat.format);
			initFramebuffers(mDevice, mRenderPass, mSwapchainImageViews, mClientWidth, mClientHeight, mFramebuffers);
		}
	}
	mRenderPassBeginInfo.renderPass = mRenderPass;
	mRenderPassBeginInfo.renderArea = { 0,0,(uint32_t)mClientWidth,(uint32_t)mClientHeight };

	mPresentInfo.pSwapchains = &mSwapchain;
}

void VulkApp::DestroySwapchain() {
	cleanupFramebuffers(mDevice, mFramebuffers);
	if (mMsaaImage.image != VK_NULL_HANDLE)
		cleanupImage(mDevice, mMsaaImage);
	if (mDepthImage.image != VK_NULL_HANDLE)
		cleanupImage(mDevice, mDepthImage);
	cleanupRenderPass(mDevice, mRenderPass);
	cleanupSwapchainImageViews(mDevice, mSwapchainImageViews);
	//cleanupSwapchain(mDevice, mSwapchain);
}

VkCommandBuffer VulkApp::BeginRender() {
	VkResult res = pvkAcquireNextImage(mDevice, mSwapchain, UINT64_MAX, mPresentComplete, nullptr, &mIndex);
	assert(res == VK_SUCCESS);


	VkCommandBuffer cmd = mCommandBuffers[mIndex];


	pvkBeginCommandBuffer(cmd, &mBeginInfo);


	mRenderPassBeginInfo.framebuffer = mFramebuffers[mIndex];
	pvkCmdBeginRenderPass(cmd, &mRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	return cmd;
}

void VulkApp::EndRender(VkCommandBuffer cmd,VkFence fence) {
	pvkCmdEndRenderPass(cmd);


	VkResult res = pvkEndCommandBuffer(cmd);
	assert(res == VK_SUCCESS);

	mSubmitInfo.pCommandBuffers = &cmd;
	res = pvkQueueSubmit(mGraphicsQueue, 1, &mSubmitInfo, fence);
	assert(res == VK_SUCCESS);

	res = pvkQueuePresent(mPresentQueue, &mPresentInfo);
	assert(res == VK_SUCCESS);
	//pvkQueueWaitIdle(mPresentQueue);
	mFrameCount++;
}


void VulkApp::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}
