﻿#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class Buffer;
	class Descriptor;

	//-------------------------------------------------------------------------
	class ConstantBufferView
	{
	public:
		ConstantBufferView()
		{}
		~ConstantBufferView()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, Buffer* pBuffer);
		void Destroy();

		// getter
		Descriptor* GetDesc() { return pDesc_; }

	private:
		Descriptor*	pDesc_{ nullptr };
	};	// class ConstantBufferView


	//-------------------------------------------------------------------------
	class VertexBufferView
	{
	public:
		VertexBufferView()
		{}
		~VertexBufferView()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, Buffer* pBuffer);
		void Destroy();

		// getter
		const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return view_; }

	private:
		D3D12_VERTEX_BUFFER_VIEW	view_{};
	};	// class VertexBufferView


	//-------------------------------------------------------------------------
	class IndexBufferView
	{
	public:
		IndexBufferView()
		{}
		~IndexBufferView()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, Buffer* pBuffer);
		void Destroy();

		// getter
		const D3D12_INDEX_BUFFER_VIEW& GetView() const { return view_; }

	private:
		D3D12_INDEX_BUFFER_VIEW	view_{};
	};	// class IndexBufferView


	//----------------------------------------------------------------------------
	class BufferView
	{
	public:
		BufferView()
		{}
		~BufferView()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, Buffer* pBuffer, u32 firstElement, u32 stride);
		void Destroy();

		// getter
		Descriptor* GetDesc() { return pDesc_; }

	private:
		Descriptor*	pDesc_{ nullptr };
	};	// class BufferView

}	// namespace sl12

//	EOF
