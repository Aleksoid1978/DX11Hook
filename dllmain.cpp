#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include "./minhook/minhook/include/MinHook.h"

PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN pOrigD3D11CreateDeviceAndSwapChain = nullptr;
HRESULT WINAPI pNewD3D11CreateDeviceAndSwapChain(
	_In_opt_ IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	_In_opt_ CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	_COM_Outptr_opt_ IDXGISwapChain** ppSwapChain,
	_COM_Outptr_opt_ ID3D11Device** ppDevice,
	_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
	_COM_Outptr_opt_ ID3D11DeviceContext** ppImmediateContext)
{
	OutputDebugStringW(L"D3D11CreateDeviceAndSwapChain()");

	return pOrigD3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
}

typedef HRESULT(__stdcall* PFNCreateSwapChainForHwnd)(
	IDXGIFactory2*,
	_In_ IUnknown*,
	_In_ HWND,
	_In_ const DXGI_SWAP_CHAIN_DESC1*,
	_In_opt_ const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,
	_In_opt_ IDXGIOutput*,
	_COM_Outptr_ IDXGISwapChain1**);

PFNCreateSwapChainForHwnd pOrigCreateSwapChainForHwnd = nullptr;
HRESULT WINAPI pNewCreateSwapChainForHwnd(
	IDXGIFactory2* This,
	_In_ IUnknown* pDevice,
	_In_ HWND hWnd,
	_In_ const DXGI_SWAP_CHAIN_DESC1* pDesc,
	_In_opt_ const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
	_In_opt_ IDXGIOutput* pRestrictToOutput,
	_COM_Outptr_ IDXGISwapChain1** ppSwapChain)
{
	OutputDebugStringW(L"CreateSwapChainForHwnd()");

	return pOrigCreateSwapChainForHwnd(This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
}

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
	wchar_t buf[256] = {};
	swprintf_s(buf, _countof(buf), L"VideoProcessorBlt(): OutputFrame - %u, StreamCount - %u, InputFrameOrField - %u, OutputIndex - %u, FutureFrames - %u, PastFrames - %u\n",
								   OutputFrame, StreamCount, pStreams->InputFrameOrField, pStreams->OutputIndex, pStreams->FutureFrames, pStreams->PastFrames);
	OutputDebugStringW(buf);
#endif

	return pOrigVideoProcessorBlt(This, pVideoProcessor, pView, OutputFrame, StreamCount, pStreams);
}

typedef HRESULT(__stdcall* PFNPresent)(
	IDXGISwapChain*,
	/* [in] */ UINT,
	/* [in] */ UINT);

PFNPresent POrigPresent = nullptr;
HRESULT WINAPI PNewPresent(
	IDXGISwapChain* This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags)
{
#ifdef _DEBUG
	wchar_t buf[64] = {};
	swprintf_s(buf, _countof(buf), L"Present(): SyncInterval - %u, Flags - %u\n",
								   SyncInterval, Flags);
	OutputDebugStringW(buf);
#endif

	return POrigPresent(This, SyncInterval, Flags);
}

typedef HRESULT(__stdcall* PFNPresent1)(
	IDXGISwapChain1*,
	/* [in] */ UINT,
	/* [in] */ UINT,
	_In_ const DXGI_PRESENT_PARAMETERS*);

PFNPresent1 POrigPresent1 = nullptr;
HRESULT WINAPI PNewPresent1(
	IDXGISwapChain1* This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags,
	_In_ const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
#ifdef _DEBUG
	wchar_t buf[64] = {};
	swprintf_s(buf, _countof(buf), L"Present1(): SyncInterval - %u, Flags - %u\n",
								   SyncInterval, Flags);
	OutputDebugStringW(buf);
#endif

	return POrigPresent1(This, SyncInterval, Flags, pPresentParameters);
}

template <typename T>
inline bool HookFunc(T** ppSystemFunction, PVOID pHookFunction)
{
	return (MH_CreateHook(*ppSystemFunction, pHookFunction, reinterpret_cast<LPVOID*>(ppSystemFunction)) == MH_OK && MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
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
				pOrigD3D11CreateDeviceAndSwapChain = pD3D11CreateDeviceAndSwapChain;
				auto ret = HookFunc(&pOrigD3D11CreateDeviceAndSwapChain, pNewD3D11CreateDeviceAndSwapChain);
				if (ret) {
					OutputDebugStringW(L"SetHookThread() - hook for D3D11CreateDeviceAndSwapChain() set\n");
				} else {
					OutputDebugStringW(L"SetHookThread() - hook for D3D11CreateDeviceAndSwapChain() fail\n");
				}

				DWORD_PTR* pSwapChainVtable = (DWORD_PTR*)pSwapChain;
				pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];

				POrigPresent = (PFNPresent)(pSwapChainVtable[8]);

				ret = HookFunc(&POrigPresent, PNewPresent);
				if (ret) {
					OutputDebugStringW(L"SetHookThread() - hook for Present() set\n");
				} else {
					OutputDebugStringW(L"SetHookThread() - hook for Present() fail\n");
				}

				IDXGIDevice* pDXGIDevice = nullptr;
				pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice);
				if (pDXGIDevice) {
					IDXGIAdapter* pDXGIAdapter = nullptr;
					pDXGIDevice->GetAdapter(&pDXGIAdapter);
					if (pDXGIAdapter) {
						IDXGIFactory1* pDXGIFactory1 = nullptr;
						pDXGIAdapter->GetParent(__uuidof(IDXGIFactory1), (void**)&pDXGIFactory1);
						if (pDXGIFactory1) {
							IDXGIFactory2* pDXGIFactory2 = nullptr;
							pDXGIFactory1->QueryInterface(__uuidof(IDXGIFactory2), (void**)&pDXGIFactory2);
							if (pDXGIFactory2) {
								IDXGISwapChain1* pSwapChain1 = nullptr;

								DXGI_SWAP_CHAIN_DESC1 desc = {};
								desc.Width = 320;
								desc.Height = 200;
								desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
								desc.SampleDesc.Count = 1;
								desc.SampleDesc.Quality = 0;
								desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
								desc.BufferCount = 1;
								desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
								desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
								pDXGIFactory2->CreateSwapChainForHwnd(pDevice, hWnd, &desc, nullptr, nullptr, &pSwapChain1);
								if (pSwapChain1) {
									DWORD_PTR* pSwapChain1Vtable = (DWORD_PTR*)pSwapChain1;
									pSwapChain1Vtable = (DWORD_PTR*)pSwapChain1Vtable[0];

									POrigPresent1 = (PFNPresent1)(pSwapChain1Vtable[22]);

									ret = HookFunc(&POrigPresent1, PNewPresent1);
									if (ret) {
										OutputDebugStringW(L"SetHookThread() - hook for Present1() set\n");
									} else {
										OutputDebugStringW(L"SetHookThread() - hook for Present1() fail\n");
									}

									pSwapChain1->Release();
								}

								DWORD_PTR* pDXGIFactory2Vtable = (DWORD_PTR*)pDXGIFactory2;
								pDXGIFactory2Vtable = (DWORD_PTR*)pDXGIFactory2Vtable[0];

								pOrigCreateSwapChainForHwnd = (PFNCreateSwapChainForHwnd)(pDXGIFactory2Vtable[15]);

								ret = HookFunc(&pOrigCreateSwapChainForHwnd, pNewCreateSwapChainForHwnd);
								if (ret) {
									OutputDebugStringW(L"SetHookThread() - hook for CreateSwapChainForHwnd() set\n");
								} else {
									OutputDebugStringW(L"SetHookThread() - hook for CreateSwapChainForHwnd() fail\n");
								}

								pDXGIFactory2->Release();
							}

							pDXGIFactory1->Release();
						}

						pDXGIAdapter->Release();
					}

					pDXGIDevice->Release();
				}

				ID3D11VideoContext* pVideoContext = nullptr;
				pContext->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&pVideoContext);
				if (pVideoContext) {
					DWORD_PTR* pVideoContextVtable = (DWORD_PTR*)pVideoContext;
					pVideoContextVtable = (DWORD_PTR*)pVideoContextVtable[0];

					pOrigVideoProcessorBlt = (PFNVideoProcessorBlt)(pVideoContextVtable[53]);
					ret = HookFunc(&pOrigVideoProcessorBlt, pNewVideoProcessorBlt);
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
			MH_Initialize();
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
