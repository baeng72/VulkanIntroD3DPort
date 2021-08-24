#pragma once
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <exception>
#include <cassert>
#include <string>
#include <windows.h>
#include <windowsx.h>
#include "Vulkan.h"
#include "GameTimer.h"

class VulkApp
{
protected:

    VulkApp(HINSTANCE hInstance);
    VulkApp(const VulkApp& rhs) = delete;
    VulkApp& operator=(const VulkApp& rhs) = delete;
    virtual ~VulkApp();

    virtual void OnResize();
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;

    // Convenience overrides for handling mouse input.
    virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
    virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
    virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

    bool InitMainWindow();
    bool InitVulkan();
    void CreateSwapchain();
    void DestroySwapchain();
    void cleanupVulkan();

    void CalculateFrameStats();

    static VulkApp* mApp;

    HINSTANCE mhAppInst = nullptr; // application instance handle
    HWND      mhMainWnd = nullptr; // main window handle
    bool      mAppPaused = false;  // is the application paused?
    bool      mMinimized = false;  // is the application minimized?
    bool      mMaximized = false;  // is the application maximized?
    bool      mResizing = false;   // are the resize bars being dragged?
    bool      mFullscreenState = false;// fullscreen enabled
    bool        mGeometryShader{ false };
    bool        mDepthBuffer{ true };
    bool        mMSAA{ true };
    bool    mAllowWireframe{ false };
     // Used to keep track of the “delta-time” and game time (§4.4).
    GameTimer mTimer;

    std::wstring mMainWndCaption = L"Vulk App";

    int mClientWidth = 800;
    int mClientHeight = 600;

    VkInstance                          mInstance{ VK_NULL_HANDLE };
    VkSurfaceKHR                        mSurface{ VK_NULL_HANDLE };
    VkPhysicalDevice                    mPhysicalDevice{ VK_NULL_HANDLE };
    Queues                              mQueues;
    VkQueue                             mGraphicsQueue{ VK_NULL_HANDLE };
    VkQueue                             mPresentQueue{ VK_NULL_HANDLE };
    VkQueue                             mComputeQueue{ VK_NULL_HANDLE };
    VkPhysicalDeviceProperties          mDeviceProperties;
    VkPhysicalDeviceMemoryProperties    mMemoryProperties;
    VkPhysicalDeviceFeatures            mDeviceFeatures;
    VkSurfaceCapabilitiesKHR            mSurfaceCaps;
    std::vector<VkSurfaceFormatKHR>     mSurfaceFormats;
    std::vector<VkPresentModeKHR>       mPresentModes;
    VkSampleCountFlagBits               mNumSamples{ VK_SAMPLE_COUNT_1_BIT };
    VkDevice                            mDevice{ VK_NULL_HANDLE };
    VkPresentModeKHR                    mPresentMode;
    VkSurfaceFormatKHR                  mSwapchainFormat;
    VkFormatProperties                  mFormatProperties;
    VkExtent2D                          mSwapchainExtent{};
    VkSwapchainKHR                      mSwapchain{ VK_NULL_HANDLE };
    std::vector<VkImage>                mSwapchainImages;
    std::vector<VkImageView>            mSwapchainImageViews;
    VkSemaphore                         mPresentComplete{ VK_NULL_HANDLE };
    VkSemaphore                         mRenderComplete{ VK_NULL_HANDLE };
    VkCommandPool                       mCommandPool{ VK_NULL_HANDLE };
    VkCommandBuffer                     mCommandBuffer{ VK_NULL_HANDLE };
    std::vector<VkCommandPool>          mCommandPools;
    std::vector<VkCommandBuffer>        mCommandBuffers;
    VkFormat                            mDepthFormat = VK_FORMAT_D32_SFLOAT;
    VkRenderPass                        mRenderPass{ VK_NULL_HANDLE };
    Image                               mDepthImage;
    Image                               mMsaaImage;
    std::vector<VkFramebuffer>          mFramebuffers;
    PFN_vkAcquireNextImageKHR pvkAcquireNextImage{ nullptr };
    PFN_vkQueuePresentKHR pvkQueuePresent{ nullptr };
    PFN_vkQueueSubmit pvkQueueSubmit{ nullptr };
    PFN_vkBeginCommandBuffer pvkBeginCommandBuffer{ nullptr };
    PFN_vkCmdBeginRenderPass pvkCmdBeginRenderPass{ nullptr };
    PFN_vkCmdBindPipeline pvkCmdBindPipeline{ nullptr };
    PFN_vkCmdBindVertexBuffers pvkCmdBindVertexBuffers{ nullptr };
    PFN_vkCmdBindIndexBuffer pvkCmdBindIndexBuffer{ nullptr };
    PFN_vkCmdDrawIndexed pvkCmdDrawIndexed{ nullptr };
    PFN_vkCmdEndRenderPass pvkCmdEndRenderPass{ nullptr };
    PFN_vkEndCommandBuffer pvkEndCommandBuffer{ nullptr };
    PFN_vkQueueWaitIdle pvkQueueWaitIdle{ nullptr };
    PFN_vkCmdBindDescriptorSets pvkCmdBindDescriptorSets{ nullptr };
    PFN_vkCmdDispatch pvkCmdDispatch{ nullptr };
    PFN_vkCmdDraw pvkCmdDraw{ nullptr };
    PFN_vkCmdSetViewport pvkCmdSetViewport{ nullptr };
    PFN_vkCmdSetScissor pvkCmdSetScissor{ nullptr };
    VkClearValue                        mClearValues[2] = { {0.0f, 0.0f, 0.0f, 0.0f},{1.0f,0.0f } };
    VkCommandBufferBeginInfo            mBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkRenderPassBeginInfo               mRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    VkDeviceSize                        mOffsets[1] = { 0 };
    VkPipelineStageFlags                mSubmitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo		                mSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkPresentInfoKHR                    mPresentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    uint32_t                            mIndex = 0;
    uint32_t                            mFrameCount = 0;

    VkCommandBuffer BeginRender();
    void            EndRender(VkCommandBuffer cmd,VkFence fence=VK_NULL_HANDLE);
public:
    static VulkApp* GetApp();

    HINSTANCE AppInst()const;
    HWND      MainWnd()const;
    float     AspectRatio()const;
    int Run();

    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

};