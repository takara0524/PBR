//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "DXSample.h"
#include "SphereMesh.h"
#include "Model12.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class PBRSandbox12 : public DXSample
{
	struct  PBRParameter
	{
		float baseColor[4];
		float MetallicRougnessReflectance[4];
		float view[3];
		float pad2;
		float ambientColor[3];
	};

	struct Light
	{
		float direction[3];
		float intensity;
		float lightColor[3];
	};
	
	struct SceneConstantBuffer
	{
		DirectX::XMMATRIX mWVP;
		DirectX::XMMATRIX mWorld;
	};

public:
	PBRSandbox12(UINT width, UINT height, std::wstring name);

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

private:
	static const UINT FrameCount = 2;

	enum HEAP_OFFSET : uint32_t
	{
		IMGUI_HEAP_OFFSET = 0,
		SCENE_CONSTANT_BUFFER,
		MATERIAL_CONSTANT_BUFFER,
		LIGHT_CONSTANT_BUFFER,
		BASE_COLOR,
		METALLIC_ROUGHNESS,
		CUBEMAP_RADDIANCE,
		CUBEMAP_IRRADIANCE,
	};

	// Pipeline objects.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12Resource> m_depthStencil;

	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;

	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	UINT m_srvDescriptorSize;
	UINT m_rtvDescriptorSize;
	UINT m_dsvDescriptorSize;

	// App resources.
	ComPtr<ID3D12Resource> m_baseColorTexture;
	ComPtr<ID3D12Resource> m_metallicRoughnessTexture;
	ComPtr<ID3D12Resource> m_radianceCube;
	ComPtr<ID3D12Resource> m_irradianceCube;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_vsConstantBuffer;
	SceneConstantBuffer m_vsConstantBufferData;
	UINT8* m_pVSCbvDataBegin;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_psConstantBuffer;
	PBRParameter	m_psConstatnBufferData;
	UINT8* m_pPSCbvDataBegin;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_psLightConstantBuffer;
	Light	m_psLightConstatnBufferData;
	UINT8* m_pPLightSCbvDataBegin;
	
	SphereMesh m_sphereMesh;
	Model	m_model;

	// Synchronization objects.
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();

	void IMGuiUpdate();

	void LoadTexture(wchar_t* filename, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		ID3D12GraphicsCommandList* pCommandList, ID3D12Resource** texture, ID3D12Resource** textureUploadHeap);

	void LoadCubeTexture(wchar_t* filename, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		ID3D12GraphicsCommandList* pCommandList, ID3D12Resource** texture, ID3D12Resource** textureUploadHeap);
};
