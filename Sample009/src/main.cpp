#include <vector>

#include "sl12/application.h"
#include "sl12/command_list.h"
#include "sl12/root_signature.h"
#include "sl12/texture.h"
#include "sl12/texture_view.h"
#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/command_queue.h"
#include "sl12/descriptor.h"
#include "sl12/descriptor_heap.h"
#include "sl12/swapchain.h"

#include "CompiledShaders/test.lib.hlsl.h"


namespace
{
	static const int	kScreenWidth = 1280;
	static const int	kScreenHeight = 720;

	static LPCWSTR		kRayGenName			= L"RayGenerator";
	static LPCWSTR		kClosestHitName		= L"ClosestHitProcessor";
	static LPCWSTR		kMissName			= L"MissProcessor";
	static LPCWSTR		kHitGroupName		= L"HitGroup";
}

class SampleApplication
	: public sl12::Application
{
	struct Viewport
	{
		float left;
		float top;
		float right;
		float bottom;
	};

	struct RayGenCB
	{
		Viewport viewport;
		Viewport stencil;
	};

public:
	SampleApplication(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight)
		: Application(hInstance, nCmdShow, screenWidth, screenHeight)
	{}

	bool Initialize() override
	{
		// �R�}���h���X�g�̏�����
		auto&& gqueue = device_.GetGraphicsQueue();
		for (auto&& v : cmdLists_)
		{
			if (!v.Initialize(&device_, &gqueue, true))
			{
				return false;
			}
		}

		// ���[�g�V�O�l�`���̏�����
		{
			D3D12_DESCRIPTOR_RANGE ranges[] = {
				{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			};

			D3D12_ROOT_PARAMETER params[2];
			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			params[0].DescriptorTable.NumDescriptorRanges = 1;
			params[0].DescriptorTable.pDescriptorRanges = ranges;
			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			params[1].Descriptor.ShaderRegister = 0;
			params[1].Descriptor.RegisterSpace = 0;

			D3D12_ROOT_SIGNATURE_DESC sigDesc{};
			sigDesc.NumParameters = ARRAYSIZE(params);
			sigDesc.pParameters = params;
			sigDesc.NumStaticSamplers = 0;
			sigDesc.pStaticSamplers = nullptr;
			sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			if (!globalRootSig_.Initialize(&device_, sigDesc))
			{
				return false;
			}
		}
		{
			sl12::RootParameter params[] = {
				sl12::RootParameter(sl12::RootParameterType::ConstantBuffer, sl12::ShaderVisibility::All, 0),
			};
			sl12::RootSignatureDesc desc;
			desc.pParameters = params;
			desc.numParameters = ARRAYSIZE(params);
			desc.flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			if (!localRootSig_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// �p�C�v���C���X�e�[�g�I�u�W�F�N�g�̏�����
		if (!CreatePipelineState())
		{
			return false;
		}

		// �o�͐�̃e�N�X�`���𐶐�
		{
			sl12::TextureDesc desc;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.mipLevels = 1;
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			desc.sampleCount = 1;
			desc.clearColor[4] = { 0.0f };
			desc.clearDepth = 1.0f;
			desc.clearStencil = 0;
			desc.isRenderTarget = false;
			desc.isDepthBuffer = false;
			desc.isUav = true;
			if (!resultTexture_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!resultTextureView_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}
		}

		// �W�I���g���𐶐�����
		if (!CreateGeometry())
		{
			return false;
		}

		// AS�𐶐�����
		if (!CreateAccelerationStructure())
		{
			return false;
		}

		// �V�F�[�_�e�[�u���𐶐�����
		if (!CreateShaderTable())
		{
			return false;
		}

		return true;
	}

	bool Execute() override
	{
		device_.WaitPresent();

		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto&& cmdList = cmdLists_[frameIndex];
		auto&& d3dCmdList = cmdList.GetCommandList();
		auto&& dxrCmdList = cmdList.GetDxrCommandList();

		cmdList.Reset();

		auto&& swapchain = device_.GetSwapchain();
		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
		d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView()->GetDesc()->GetCpuHandle(), color, 0, nullptr);

		// �O���[�o�����[�g�V�O�l�`����ݒ�
		d3dCmdList->SetComputeRootSignature(globalRootSig_.GetRootSignature());

		// �f�X�N���v�^�q�[�v��ݒ�
		ID3D12DescriptorHeap* pDescHeaps[] = {
			device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
			device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
		};
		d3dCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescHeaps), pDescHeaps);

		// �O���[�o���ݒ�̃V�F�[�_���\�[�X��ݒ肷��
		d3dCmdList->SetComputeRootDescriptorTable(0, resultTextureView_.GetDesc()->GetGpuHandle());
		d3dCmdList->SetComputeRootShaderResourceView(1, topAS_.GetResourceDep()->GetGPUVirtualAddress());

		// ���C�g���[�X�����s
		D3D12_DISPATCH_RAYS_DESC desc{};
		desc.HitGroupTable.StartAddress = hitGroupTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.HitGroupTable.SizeInBytes = hitGroupTable_.GetSize();
		desc.HitGroupTable.StrideInBytes = desc.HitGroupTable.SizeInBytes;
		desc.MissShaderTable.StartAddress = missTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.MissShaderTable.SizeInBytes = missTable_.GetSize();
		desc.MissShaderTable.StrideInBytes = desc.MissShaderTable.SizeInBytes;
		desc.RayGenerationShaderRecord.StartAddress = rayGenTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.RayGenerationShaderRecord.SizeInBytes = rayGenTable_.GetSize();
		desc.Width = kScreenWidth;
		desc.Height = kScreenHeight;
		desc.Depth = 1;
		dxrCmdList->SetPipelineState1(pStateObject_);
		dxrCmdList->DispatchRays(&desc);

		cmdList.UAVBarrier(&resultTexture_);

		// ���\�[�X�o���A
		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

		d3dCmdList->CopyResource(swapchain.GetCurrentTexture()->GetResourceDep(), resultTexture_.GetResourceDep());

		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// �R�}���h�I���Ǝ��s
		cmdList.Close();
		cmdList.Execute();
		device_.WaitDrawDone();

		// ���̃t���[����
		device_.Present(1);

		return true;
	}

	void Finalize() override
	{
		rayGenTable_.Destroy();
		missTable_.Destroy();
		hitGroupTable_.Destroy();

		rayGenCBV_.Destroy();
		rayGenCB_.Destroy();

		topAS_.Destroy();
		bottomAS_.Destroy();

		geometryIBV_.Destroy();
		geometryVBV_.Destroy();
		geometryIB_.Destroy();
		geometryVB_.Destroy();

		resultTextureView_.Destroy();
		resultTexture_.Destroy();

		sl12::SafeRelease(pStateObject_);

		localRootSig_.Destroy();
		globalRootSig_.Destroy();
		for (auto&& v : cmdLists_) v.Destroy();
	}

private:
	bool CreateRootSig(const D3D12_ROOT_SIGNATURE_DESC& desc, ID3D12RootSignature** ppSig)
	{
		ID3DBlob* blob = nullptr;
		ID3DBlob* error = nullptr;
		bool ret = true;

		auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
		if (FAILED(hr))
		{
			ret = false;
			goto D3D_ERROR;
		}

		hr = device_.GetDeviceDep()->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(ppSig));
		if (FAILED(hr))
		{
			ret = false;
			goto D3D_ERROR;
		}

	D3D_ERROR:
		sl12::SafeRelease(blob);
		sl12::SafeRelease(error);
		return ret;
	}

	bool CreatePipelineState()
	{
		// DXR�p�̃p�C�v���C���X�e�[�g�𐶐����܂�.
		// Graphics�p�ACompute�p�̃p�C�v���C���X�e�[�g�͐������ɌŒ�T�C�Y�̋L�q�q��p�ӂ��܂�.
		// ����ɑ΂��āADXR�p�̃p�C�v���C���X�e�[�g�͉ό̃T�u�I�u�W�F�N�g��K�v�Ƃ��܂�.
		// �܂��A�T�u�I�u�W�F�N�g�̒��ɂ͑��̃T�u�I�u�W�F�N�g�ւ̃|�C���^��K�v�Ƃ�����̂����邽�߁A�T�u�I�u�W�F�N�g�z��̐����ɂ͒��ӂ��K�v�ł�.

		std::vector<D3D12_STATE_SUBOBJECT> subobjects;
		subobjects.reserve(32);
		auto AddSubobject = [&](D3D12_STATE_SUBOBJECT_TYPE type, const void* desc)
		{
			D3D12_STATE_SUBOBJECT sub;
			sub.Type = type;
			sub.pDesc = desc;
			subobjects.push_back(sub);
		};

		// DXIL���C�u�����T�u�I�u�W�F�N�g
		// 1�̃V�F�[�_���C�u�����ƁA�����ɓo�^����Ă���V�F�[�_�̃G���g���[�|�C���g���G�N�X�|�[�g���邽�߂̃T�u�I�u�W�F�N�g�ł�.
		D3D12_EXPORT_DESC libExport[] = {
			{ kRayGenName,		nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kClosestHitName,	nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kMissName,		nullptr, D3D12_EXPORT_FLAG_NONE },
		};

		D3D12_DXIL_LIBRARY_DESC dxilDesc{};
		dxilDesc.DXILLibrary.pShaderBytecode = g_pTestLib;
		dxilDesc.DXILLibrary.BytecodeLength = sizeof(g_pTestLib);
		dxilDesc.NumExports = ARRAYSIZE(libExport);
		dxilDesc.pExports = libExport;
		AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxilDesc);

		// �q�b�g�O���[�v�T�u�I�u�W�F�N�g
		// Intersection, AnyHit, ClosestHit�̑g�ݍ��킹���`���A�q�b�g�O���[�v���ł܂Ƃ߂�T�u�I�u�W�F�N�g�ł�.
		// �}�e���A�����Ƃ�p�r����(�}�e���A���A�V���h�E�Ȃ�)�ɃT�u�I�u�W�F�N�g��p�ӂ��܂�.
		D3D12_HIT_GROUP_DESC hitGroupDesc{};
		hitGroupDesc.HitGroupExport = kHitGroupName;
		hitGroupDesc.ClosestHitShaderImport = kClosestHitName;
		AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDesc);

		// �V�F�[�_�R���t�B�O�T�u�I�u�W�F�N�g
		// �q�b�g�V�F�[�_�A�~�X�V�F�[�_�̈����ƂȂ�Payload, IntersectionAttributes�̍ő�T�C�Y��ݒ肵�܂�.
		D3D12_RAYTRACING_SHADER_CONFIG shaderConfigDesc{};
		shaderConfigDesc.MaxPayloadSizeInBytes = sizeof(float) * 4;		// float4 color
		shaderConfigDesc.MaxAttributeSizeInBytes = sizeof(float) * 2;	// float2 barycentrics
		AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfigDesc);

		// ���[�J�����[�g�V�O�l�`���T�u�I�u�W�F�N�g
		// �V�F�[�_���R�[�h���Ƃɐݒ肳��郋�[�g�V�O�l�`����ݒ肵�܂�.
		auto localRS = localRootSig_.GetRootSignature();
		AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &localRS);

		// Exports Assosiation �T�u�I�u�W�F�N�g
		// �V�F�[�_���R�[�h�ƃ��[�J�����[�g�V�O�l�`���̃o�C���h���s���T�u�I�u�W�F�N�g�ł�.
		LPCWSTR kExports[] = {
			kRayGenName,
			kMissName,
			kHitGroupName,
		};
		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION assocDesc{};
		assocDesc.pSubobjectToAssociate = &subobjects.back();
		assocDesc.NumExports = ARRAYSIZE(kExports);
		assocDesc.pExports = kExports;
		AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &assocDesc);

		// �O���[�o�����[�g�V�O�l�`���T�u�I�u�W�F�N�g
		// ���ׂẴV�F�[�_�e�[�u���ŎQ�Ƃ����O���[�o���ȃ��[�g�V�O�l�`����ݒ肵�܂�.
		auto globalRS = globalRootSig_.GetRootSignature();
		AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &globalRS);

		// ���C�g���[�X�R���t�B�O�T�u�I�u�W�F�N�g
		// TraceRay()���s�����Ƃ��ł���ő�[�x���w�肷��T�u�I�u�W�F�N�g�ł�.
		D3D12_RAYTRACING_PIPELINE_CONFIG rtConfigDesc{};
		rtConfigDesc.MaxTraceRecursionDepth = 1;
		AddSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &rtConfigDesc);

		// PSO����
		D3D12_STATE_OBJECT_DESC psoDesc{};
		psoDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		psoDesc.pSubobjects = subobjects.data();
		psoDesc.NumSubobjects = (UINT)subobjects.size();

		auto hr = device_.GetDxrDeviceDep()->CreateStateObject(&psoDesc, IID_PPV_ARGS(&pStateObject_));
		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	bool CreateGeometry()
	{
		const float size = 0.7f;
		const float depth = 1.0f;
		float vertices[] = {
			size, -size, depth,
			-size, -size, depth,
			size,  size, depth,
			-size,  size, depth,
		};
		UINT16 indices[] =
		{
			0, 1, 2,
			1, 3, 2,
		};

		if (!geometryVB_.Initialize(&device_, sizeof(vertices), 0, sl12::BufferUsage::VertexBuffer, true, false))
		{
			return false;
		}
		if (!geometryIB_.Initialize(&device_, sizeof(indices), 0, sl12::BufferUsage::IndexBuffer, true, false))
		{
			return false;
		}

		void* p = geometryVB_.Map(nullptr);
		memcpy(p, vertices, sizeof(vertices));
		geometryVB_.Unmap();

		p = geometryIB_.Map(nullptr);
		memcpy(p, indices, sizeof(indices));
		geometryIB_.Unmap();

		if (!geometryVBV_.Initialize(&device_, &geometryVB_))
		{
			return false;
		}
		if (!geometryIBV_.Initialize(&device_, &geometryIB_))
		{
			return false;
		}

		return true;
	}

	bool CreateAccelerationStructure()
	{
		// AS�̐�����GPU�ōs�����߁A�R�}���h��ς�GPU�𓮍삳����K�v������܂�.
		auto&& cmdList = cmdLists_[0];
		cmdList.Reset();

		// AS�̐����ɂ̓W�I���g����񂪕K�v�ɂȂ�܂�.
		// �W�I���g����ʁA���_�E�C���f�b�N�X�����L�q���܂�.
		// ���_�o�b�t�@�A�C���f�b�N�X�o�b�t�@�A�g�����X�t�H�[���s��o�b�t�@��GPU�ŏ�������邽�߁AGPU���̃������A�h���X��K�v�Ƃ��܂�.
		D3D12_RAYTRACING_GEOMETRY_DESC geoDesc{};
		geoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geoDesc.Triangles.IndexBuffer = geometryIBV_.GetView().BufferLocation;
		geoDesc.Triangles.IndexCount = static_cast<UINT>(geometryIB_.GetSize()) / sizeof(UINT16);
		geoDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
		geoDesc.Triangles.Transform3x4 = 0;
		geoDesc.Triangles.VertexBuffer.StartAddress = geometryVBV_.GetView().BufferLocation;
		geoDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(float) * 3;
		geoDesc.Triangles.VertexCount = static_cast<UINT>(geometryVB_.GetSize() / geoDesc.Triangles.VertexBuffer.StrideInBytes);
		geoDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

		// AS�r���h���̃t���O�ł�.
		// �����ł̓g���[�X�����������邱�Ƃ�ړI�Ƃ����t���O��ݒ肵�܂�.
		// AS���X�V����ꍇ�̓A�b�v�f�[�g�t���O�ƃr���h�������t���O���w�肷��ق����悢��������܂���.
		// AS�r���h��GPU�h���C�o�Ɉˑ����邽�߁A�ǂ̒��x����������邩�̓h���C�o�ɂ����̂Ǝv���܂�.
		auto buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		// AS�ɕK�v�ȃo�b�t�@�T�C�Y�����߂܂�.
		// AS��GPU�Ő�������邽�߁A���O�ɕK�v�ȃT�C�Y�̃o�b�t�@���m�ۂ��Ă����K�v������܂�.
		// �܂��A�������ɍ�ƃo�b�t�@���K�v�ƂȂ�܂��̂ŁA��������\�ߊm�ۂ���K�v������܂�.
		// ��ƃo�b�t�@�͎w�肳�ꂽ�T�C�Y�ȏ�ł���Ζ��Ȃ��̂ŁA�����ɕK�v�ȍő�T�C�Y���m�ۂ��Ă����Ζ�肠��܂���.
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topPrebuildInfo{};
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomPrebuildInfo{};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topInput{};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomInput{};
		{
			// TopAS
			topInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
			topInput.Flags = buildFlags;
			topInput.NumDescs = 1;
			topInput.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			device_.GetDxrDeviceDep()->GetRaytracingAccelerationStructurePrebuildInfo(&topInput, &topPrebuildInfo);
			if (topPrebuildInfo.ResultDataMaxSizeInBytes == 0)
				return false;

			// BottomAS
			bottomInput = topInput;
			bottomInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			bottomInput.pGeometryDescs = &geoDesc;
			device_.GetDxrDeviceDep()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomInput, &bottomPrebuildInfo);
			if (bottomPrebuildInfo.ResultDataMaxSizeInBytes == 0)
				return false;
		}

		// �X�N���b�`���\�[�X�𐶐����܂�.
		// �X�N���b�`���\�[�X�Ƃ�AS�r���h�Ɏg�p������ƃo�b�t�@�ł�.
		// AS���A�b�v�f�[�g����ꍇ�͕ێ����Ă������ق��������Ǝv���܂����A�A�b�v�f�[�g���Ȃ��ꍇ�͐�����ɔj�����Ė�肠��܂���.
		sl12::Buffer scratchResource;
		auto scratchSize = std::max(topPrebuildInfo.ScratchDataSizeInBytes, bottomPrebuildInfo.ScratchDataSizeInBytes);
		if (!scratchResource.Initialize(&device_, scratchSize, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false, true))
		{
			return false;
		}

		// TopAS, BottomAS�̃o�b�t�@���m�ۂ��܂�
		{
			D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
			if (!topAS_.Initialize(&device_, topPrebuildInfo.ResultDataMaxSizeInBytes, 0, sl12::BufferUsage::ShaderResource, initialState, false, true))
			{
				return false;
			}
			if (!bottomAS_.Initialize(&device_, bottomPrebuildInfo.ResultDataMaxSizeInBytes, 0, sl12::BufferUsage::ShaderResource, initialState, false, true))
			{
				return false;
			}

			// �V�F�[�_���\�[�X�Ƃ��Ďg�p�����TopAS��View���쐬���Ă���
			if (!topAS_Srv_.Initialize(&device_, &topAS_, 0, 0))
			{
				return false;
			}
		}

		// TopAS�p�̃C���X�^���X�o�b�t�@�𐶐����܂�.
		// BottomAS�̐����ɕK�v�Ȓ��_�o�b�t�@�A�C���f�b�N�X�o�b�t�@�͂��łɐ�������Ă��܂�.
		// TopAS��BottomAS�̃C���X�^���X�����ɐ�������邽�߁A�K�v�ȃC���X�^���X���`�����o�b�t�@���K�v�ɂȂ�܂�.
		// ���̃o�b�t�@��TopAS������ɂ͕s�v�ƂȂ�܂�.
		sl12::Buffer instanceBuffer;
		{
			D3D12_RAYTRACING_INSTANCE_DESC desc{};
			desc.Transform[0][0] = desc.Transform[1][1] = desc.Transform[2][2] = 1.0f;
			desc.InstanceID = 0;
			desc.InstanceMask = 0xff;
			desc.InstanceContributionToHitGroupIndex = 0;
			desc.Flags = 0;
			desc.AccelerationStructure = bottomAS_.GetResourceDep()->GetGPUVirtualAddress();

			if (!instanceBuffer.Initialize(&device_, sizeof(desc), 0, sl12::BufferUsage::ShaderResource, true, false))
			{
				return false;
			}

			auto p = instanceBuffer.Map(nullptr);
			memcpy(p, &desc, sizeof(desc));
			instanceBuffer.Unmap();
		}

		// BottomAS�r���h�p�̋L�q�q
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomBuildDesc{};
		{
			bottomBuildDesc.DestAccelerationStructureData = bottomAS_.GetResourceDep()->GetGPUVirtualAddress();
			bottomBuildDesc.Inputs = bottomInput;
			bottomBuildDesc.ScratchAccelerationStructureData = scratchResource.GetResourceDep()->GetGPUVirtualAddress();
		}

		// TopAS�r���h�p�̋L�q�q
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topBuildDesc{};
		{
			topBuildDesc.DestAccelerationStructureData = topAS_.GetResourceDep()->GetGPUVirtualAddress();
			topBuildDesc.Inputs = topInput;
			topBuildDesc.Inputs.InstanceDescs = instanceBuffer.GetResourceDep()->GetGPUVirtualAddress();
			topBuildDesc.ScratchAccelerationStructureData = scratchResource.GetResourceDep()->GetGPUVirtualAddress();
		}

		// AS�r���h�p�̃R�}���h�𔭍s
		{
			// BottomAS�r���h
			// TopAS�r���h�̍ۂ�BottomAS�͕K���K�v�ɂȂ邽�߁A��������Ƀr���h���܂�.
			cmdList.GetDxrCommandList()->BuildRaytracingAccelerationStructure(&bottomBuildDesc, 0, nullptr);

			// TopAS�r���h�O��BottomAS�̃r���h������҂K�v������܂�.
			// ���\�[�X�o���A�𒣂邱�Ƃ�BottomAS�r���h���������Ă��邱�Ƃ�ۏ؂��܂�.
			cmdList.UAVBarrier(&bottomAS_);

			// TopAS�r���h
			cmdList.GetDxrCommandList()->BuildRaytracingAccelerationStructure(&topBuildDesc, 0, nullptr);
		}

		// �R�}���h���s�ƏI���҂�
		cmdList.Close();
		cmdList.Execute();
		device_.WaitDrawDone();

		instanceBuffer.Destroy();
		scratchResource.Destroy();

		return true;
	}

	bool CreateShaderTable()
	{
		// ���C�����V�F�[�_�A�~�X�V�F�[�_�A�q�b�g�O���[�v��ID���擾���܂�.
		// �e�V�F�[�_��ʂ��ƂɃV�F�[�_�e�[�u�����쐬���܂����A���̃T���v���ł͊e�V�F�[�_��ʂ͂��ꂼ��1�̃V�F�[�_�������ƂɂȂ�܂�.
		void* rayGenShaderIdentifier;
		void* missShaderIdentifier;
		void* hitGroupShaderIdentifier;
		{
			ID3D12StateObjectProperties* prop;
			pStateObject_->QueryInterface(IID_PPV_ARGS(&prop));
			rayGenShaderIdentifier = prop->GetShaderIdentifier(kRayGenName);
			missShaderIdentifier = prop->GetShaderIdentifier(kMissName);
			hitGroupShaderIdentifier = prop->GetShaderIdentifier(kHitGroupName);
			prop->Release();
		}

		UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		// ���C�����V�F�[�_�Ŏg�p����萔�o�b�t�@�𐶐�����
		struct Viewport
		{
			float		left, top, right, bottom;
		};
		struct RayGenCB
		{
			Viewport	viewport, stencil;
		};
		if (!rayGenCB_.Initialize(&device_, sizeof(RayGenCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
		{
			return false;
		}
		else
		{
			auto cb = reinterpret_cast<RayGenCB*>(rayGenCB_.Map(nullptr));
			cb->viewport = { -1.0f, -1.0f, 1.0f, 1.0f };
			cb->stencil = { -0.9f, -0.9f, 0.9f, 0.9f };
			rayGenCB_.Unmap();

			if (!rayGenCBV_.Initialize(&device_, &rayGenCB_))
			{
				return false;
			}
		}

		auto Align = [](UINT size, UINT align)
		{
			return ((size + align - 1) / align) * align;
		};

		// �V�F�[�_���R�[�h�T�C�Y
		// �V�F�[�_���R�[�h�̓V�F�[�_�e�[�u���̗v�f1�ł�.
		// ����̓V�F�[�_ID�ƃ��[�J�����[�g�V�O�l�`���ɐݒ肳���ϐ��̑g�ݍ��킹�ō\������Ă��܂�.
		// �V�F�[�_���R�[�h�̃T�C�Y�̓V�F�[�_�e�[�u�����œ���łȂ���΂Ȃ�Ȃ����߁A����V�F�[�_�e�[�u�����ōő�̃��R�[�h�T�C�Y���w�肷�ׂ��ł�.
		// �{�T���v���ł͂��ׂẴV�F�[�_���R�[�h�ɂ��ăT�C�Y������ƂȂ�܂�.
		UINT descHandleOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
		UINT shaderRecordSize = Align(descHandleOffset + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		auto GenShaderTable = [&](void* shaderId, sl12::Buffer& buffer)
		{
			if (!buffer.Initialize(&device_, shaderRecordSize, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			auto p = reinterpret_cast<char*>(buffer.Map(nullptr));
			memcpy(p, shaderId, shaderIdentifierSize);
			auto cbHandle = rayGenCBV_.GetDesc()->GetGpuHandle();
			memcpy(p + descHandleOffset, &cbHandle, sizeof(cbHandle));
			buffer.Unmap();

			return true;
		};

		if (!GenShaderTable(rayGenShaderIdentifier, rayGenTable_))
		{
			return false;
		}
		if (!GenShaderTable(missShaderIdentifier, missTable_))
		{
			return false;
		}
		if (!GenShaderTable(hitGroupShaderIdentifier, hitGroupTable_))
		{
			return false;
		}

		return true;
	}

private:
	static const int kBufferCount = sl12::Swapchain::kMaxBuffer;

	sl12::CommandList		cmdLists_[kBufferCount];
	sl12::RootSignature		globalRootSig_, localRootSig_;

	ID3D12StateObject*			pStateObject_ = nullptr;
	sl12::Texture				resultTexture_;
	sl12::UnorderedAccessView	resultTextureView_;

	sl12::Buffer			geometryVB_, geometryIB_;
	sl12::VertexBufferView	geometryVBV_;
	sl12::IndexBufferView	geometryIBV_;

	sl12::Buffer			topAS_, bottomAS_;
	sl12::BufferView		topAS_Srv_;

	sl12::Buffer				rayGenCB_;
	sl12::ConstantBufferView	rayGenCBV_;

	sl12::Buffer			rayGenTable_, missTable_, hitGroupTable_;

	int		frameIndex_ = 0;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
