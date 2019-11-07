#include <d3d11.h>
#include <stdio.h>
#include "./minhook/minhook/include/MinHook.h"

typedef HRESULT(__stdcall* PFNVideoProcessorBlt)(
	ID3D11VideoContext*,
	_In_ ID3D11VideoProcessor*,
	_In_ ID3D11VideoProcessorOutputView*,
	_In_ UINT,
	_In_ UINT StreamCount,
	_In_reads_(StreamCount) const D3D11_VIDEO_PROCESSOR_STREAM*);

PFNVideoProcessorBlt pOrigVideoProcessorBlt = nullptr;
HRESULT WINAPI pNewVideoProcessorBlt(
	ID3D11VideoContext* This,
	_In_ ID3D11VideoProcessor* pVideoProcessor,
	_In_ ID3D11VideoProcessorOutputView* pView,
	_In_ UINT OutputFrame,
	_In_ UINT StreamCount,
	_In_reads_(StreamCount) const D3D11_VIDEO_PROCESSOR_STREAM* pStreams)
{
#ifdef _DEBUG
	wchar_t buf[512] = {};
	swprintf_s(buf, _countof(buf), L"VideoProcessorBlt(): OutputFrame - %u, StreamCount - %u, InputFrameOrField - %u, OutputIndex - %u, FutureFrames - %u, PastFrames - %u\n",
		OutputFrame, StreamCount, pStreams->InputFrameOrField, pStreams->OutputIndex, pStreams->FutureFrames, pStreams->PastFrames);
	OutputDebugStringW(buf);
#endif

	return pOrigVideoProcessorBlt(This, pVideoProcessor, pView, OutputFrame, StreamCount, pStreams);
}

template <typename T>
inline bool HookFunc(T** ppSystemFunction, PVOID pHookFunction)
{
	auto bHookingSuccessful = MH_Initialize() == MH_OK;
	bHookingSuccessful = bHookingSuccessful && MH_CreateHook(*ppSystemFunction, pHookFunction, reinterpret_cast<LPVOID*>(ppSystemFunction)) == MH_OK;
	bHookingSuccessful = bHookingSuccessful && MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
	return bHookingSuccessful;
}

DWORD __stdcall SetHookThread(LPVOID)
{
	HMODULE hD3D11 = GetModuleHandle(L"d3d11.dll");
	while (!hD3D11) {
		Sleep(50);
		hD3D11 = GetModuleHandle(L"d3d11.dll");
	}

	OutputDebugStringW(L"SetHookThread() - d3d11.dll loaded\n");

	const auto hWnd = GetForegroundWindow();
	if (hWnd) {
		PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN pD3D11CreateDeviceAndSwapChain = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
		if (pD3D11CreateDeviceAndSwapChain) {
			ID3D11Device* pDevice = nullptr;
			ID3D11DeviceContext* pContext = nullptr;
			IDXGISwapChain* pSwapChain = nullptr;

			D3D_FEATURE_LEVEL featureLevels[] = {
				D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
				D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
			};

			DXGI_SWAP_CHAIN_DESC swapChainDesc;
			ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
			swapChainDesc.BufferCount = 1;
			swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.OutputWindow = hWnd;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.Windowed = TRUE;
			swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			if (SUCCEEDED(pD3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, _countof(featureLevels), D3D11_SDK_VERSION, &swapChainDesc, &pSwapChain, &pDevice, nullptr, &pContext))) {
				ID3D11VideoContext* pVideoContext = nullptr;
				pContext->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&pVideoContext);
				if (pVideoContext) {
					DWORD_PTR* pVideoContextVtable = (DWORD_PTR*)pVideoContext;
					pVideoContextVtable = (DWORD_PTR*)pVideoContextVtable[0];

					pOrigVideoProcessorBlt = (PFNVideoProcessorBlt)(pVideoContextVtable[53]);
					const auto ret = HookFunc(&pOrigVideoProcessorBlt, pNewVideoProcessorBlt);
					if (ret) {
						OutputDebugStringW(L"SetHookThread() - hook for VideoProcessorBlt() set\n");
					} else {
						OutputDebugStringW(L"SetHookThread() - hook for VideoProcessorBlt() fail\n");
					}

					pVideoContext->Release();
				}

				pContext->Release();
				pSwapChain->Release();
				pDevice->Release();
			}
		}
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			CreateThread(nullptr, 0, SetHookThread, nullptr, 0, nullptr);
			break;
		case DLL_PROCESS_DETACH:
			MH_Uninitialize();
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
    }

    return TRUE;
}
