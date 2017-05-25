﻿#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class Descriptor;
	class CommandQueue;

	class Swapchain
	{
	public:
		static const u32	kMaxBuffer = 3;

	public:
		Swapchain()
		{}
		~Swapchain()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, CommandQueue* pQueue, HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT format);
		void Destroy();

		void Present(int syncInterval);

		// getter
		ID3D12Resource* GetRenderTarget(int index) { return pRenderTargets_[index]; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetDescHandle(int index);
		ID3D12Resource* GetCurrentRenderTarget(int offset = 0) { return pRenderTargets_[(frameIndex_ + offset) % kMaxBuffer]; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentDescHandle(int offset = 0);
		int32_t GetFrameIndex() const { return frameIndex_; }

	private:
		IDXGISwapChain3*		pSwapchain_{ nullptr };
		Descriptor*				pRtvDescs_[kMaxBuffer]{ nullptr };
		ID3D12Resource*			pRenderTargets_[kMaxBuffer]{ nullptr };
		int32_t					frameIndex_{ 0 };
	};	// class Swapchain

}	// namespace sl12

//	EOF
