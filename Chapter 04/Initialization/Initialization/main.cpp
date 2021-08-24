#include "../../../Common/VulkApp.h"

class InitVulkanApp : public VulkApp {
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
public:
	InitVulkanApp(HINSTANCE hInstance);
	~InitVulkanApp();
	virtual bool Initialize()override;
};

InitVulkanApp::InitVulkanApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mClearValues[0] = { { 0.690196097f, 0.768627524f, 0.870588303f, 1.000000000f } };
	mMSAA = false;
}

InitVulkanApp::~InitVulkanApp() {

}

bool InitVulkanApp::Initialize() {
	if (!VulkApp::Initialize()) {
		return false;
	}
	return true;
}

void InitVulkanApp::OnResize() {
	VulkApp::OnResize();
}

void InitVulkanApp::Update(const GameTimer& gt) {

}


void InitVulkanApp::Draw(const GameTimer& gt) {
	
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };

	cmd = BeginRender();




	EndRender(cmd);
	
}



int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try
	{
		InitVulkanApp theApp(GetModuleHandle(NULL));
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "HR Failed", MB_OK);
		return 0;
	}
}