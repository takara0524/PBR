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

#include "stdafx.h"
#include "PBRSandbox12.h"

#include <DirectXTex.h>
#include "DDSTextureLoader12.h"

// imgui
#include <imgui.h>
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

PBRSandbox12::PBRSandbox12(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_rtvDescriptorSize(0)
{
}

void PBRSandbox12::OnInit()
{
	LoadPipeline();
	LoadAssets();
}

// Load the rendering pipeline dependencies.
void PBRSandbox12::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
			));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
			));
	}

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
		));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 100;					// A descriptor for each of the 2 intermediate render targets.
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

		m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Describe and create a depth stencil target view (DSV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}
	}

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

	{
		// Depth Stencil Target
		CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

		D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			depthBufferFormat,
			m_width,
			m_height,
			1, 
			1
		);
		depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = depthBufferFormat;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(m_depthStencil.ReleaseAndGetAddressOf())
		));

		m_depthStencil->SetName(L"Depth stencil");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = depthBufferFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// Imgui Init
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	ImGui_ImplWin32_Init(Win32Application::GetHwnd());


	CD3DX12_CPU_DESCRIPTOR_HANDLE imguiCPUHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), IMGUI_HEAP_OFFSET, m_srvDescriptorSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE imguiGPUHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), IMGUI_HEAP_OFFSET, m_srvDescriptorSize);
	if (ImGui_ImplDX12_Init(m_device.Get(), FrameCount, DXGI_FORMAT_R8G8B8A8_UNORM, imguiCPUHandle, imguiGPUHandle))
	{
		ImGui::StyleColorsDark();

		ImGui_ImplDX12_InvalidateDeviceObjects();
		ImGui_ImplDX12_CreateDeviceObjects();
	}
}

// Load the sample assets.
void PBRSandbox12::LoadAssets()
{
	// Create an empty root signature.
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[6];
		CD3DX12_ROOT_PARAMETER1 rootParameters[3];

		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		rootParameters[2].InitAsDescriptorTable(4, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

		// Allow input layout and deny uneccessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC samplers[] = { sampler };
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1,samplers, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

#if 0
		ThrowIfFailed(D3DCompileFromFile(L"ModelShader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(L"ModelShader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
#else
		HRESULT hr = S_OK;
		ID3DBlob* errorBlob = nullptr;

		hr = D3DCompileFromFile(L"ModelShader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA((char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}

			ThrowIfFailed(hr);
		}

		hr = D3DCompileFromFile(L"ModelShader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA((char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}

			ThrowIfFailed(hr);
		}

#endif
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

	}

	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
//	ThrowIfFailed(m_commandList->Close());

	m_sphereMesh.Init(m_device.Get(), 64, 64);
	ThrowIfFailed(m_model.Load("helmet.vbo", m_device.Get()));

	//
	{
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vsConstantBuffer)));
		NAME_D3D12_OBJECT(m_vsConstantBuffer);

		CD3DX12_CPU_DESCRIPTOR_HANDLE cbCPUHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), SCENE_CONSTANT_BUFFER, m_srvDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_vsConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(SceneConstantBuffer) + 255) & ~255;
		m_device->CreateConstantBufferView(&cbvDesc, cbCPUHandle);

		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_vsConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pVSCbvDataBegin)));
		memcpy(m_pVSCbvDataBegin, &m_vsConstantBufferData, sizeof(m_vsConstantBufferData));
	}

	{
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_psConstantBuffer)));
		NAME_D3D12_OBJECT(m_psConstantBuffer);

		CD3DX12_CPU_DESCRIPTOR_HANDLE cbCPUHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), MATERIAL_CONSTANT_BUFFER, m_srvDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_psConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(PBRParameter) + 255) & ~255;
		m_device->CreateConstantBufferView(&cbvDesc, cbCPUHandle);

		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_psConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pPSCbvDataBegin)));
		memcpy(m_pPSCbvDataBegin, &m_psConstatnBufferData, sizeof(m_vsConstantBufferData));
	}

	{
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_psLightConstantBuffer)));

		NAME_D3D12_OBJECT(m_psLightConstantBuffer);

		CD3DX12_CPU_DESCRIPTOR_HANDLE cbCPUHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), LIGHT_CONSTANT_BUFFER, m_srvDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_psLightConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(PBRParameter) + 255) & ~255;
		m_device->CreateConstantBufferView(&cbvDesc, cbCPUHandle);

		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_psLightConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pPLightSCbvDataBegin)));
		memcpy(m_pPLightSCbvDataBegin, &m_psConstatnBufferData, sizeof(m_vsConstantBufferData));
	}

	// Create the GPU upload buffer.
	ID3D12Resource* pResouce = nullptr;
	ID3D12Resource* textureUploadHeap[4];
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle;

	srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), BASE_COLOR, m_srvDescriptorSize);
	LoadTexture(L"Default_albedo.dds", srvHandle, m_commandList.Get(), &pResouce, &textureUploadHeap[0]);
	m_baseColorTexture.Attach(pResouce);
	NAME_D3D12_OBJECT(m_baseColorTexture);

	srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), METALLIC_ROUGHNESS, m_srvDescriptorSize);
	LoadTexture(L"Default_metalRoughness.dds", srvHandle, m_commandList.Get(), &pResouce, &textureUploadHeap[1]);
	m_metallicRoughnessTexture.Attach(pResouce);
	NAME_D3D12_OBJECT(m_metallicRoughnessTexture);


	srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), CUBEMAP_RADDIANCE, m_srvDescriptorSize);
	LoadCubeTexture(L"Stonewall_Ref_radiance.dds", srvHandle, m_commandList.Get(), &pResouce, &textureUploadHeap[2]);
	m_radianceCube.Attach(pResouce);
	NAME_D3D12_OBJECT(m_radianceCube);

	srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), CUBEMAP_IRRADIANCE, m_srvDescriptorSize);
	LoadCubeTexture(L"Stonewall_Ref_irradiance.dds", srvHandle, m_commandList.Get(), &pResouce, &textureUploadHeap[3]);
	m_irradianceCube.Attach(pResouce);
	NAME_D3D12_OBJECT(m_irradianceCube);

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForPreviousFrame();
	}

	for (size_t i = 0; i < _countof(textureUploadHeap); i++)
	{
		textureUploadHeap[i]->Release();
	}
}


void PBRSandbox12::LoadTexture(wchar_t* filename, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12GraphicsCommandList* pCommandList, ID3D12Resource** texture, ID3D12Resource** textureUploadHeap)
{
	// Load texture from DDS.
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresouceData;
	ThrowIfFailed(LoadDDSTextureFromFile(m_device.Get(), filename, texture, ddsData, subresouceData));
	D3D12_RESOURCE_DESC textureDesc = (*texture)->GetDesc();

	CD3DX12_HEAP_PROPERTIES heapProperty;
	D3D12_HEAP_FLAGS flags;

	(*texture)->GetHeapProperties(&heapProperty, &flags);
	const UINT subresoucesize
		= static_cast<UINT>(subresouceData.size());
	const UINT64 uploadBufferSize
		= GetRequiredIntermediateSize(*texture, 0, subresoucesize);

	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(textureUploadHeap)));

	UpdateSubresources(m_commandList.Get(), *texture, *textureUploadHeap, 0, 0, subresoucesize, &subresouceData[0]);
	pCommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(*texture,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
	m_device->CreateShaderResourceView(*texture,
		&srvDesc,
		cpuHandle);
}


void PBRSandbox12::LoadCubeTexture(wchar_t* filename, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12GraphicsCommandList* pCommandList, ID3D12Resource** texture, ID3D12Resource** textureUploadHeap)
{
	bool isCubeMap = true;
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresouceData;
	ThrowIfFailed(LoadDDSTextureFromFile(m_device.Get(), filename, texture, ddsData, subresouceData, 0Ui64, nullptr, &isCubeMap));
	D3D12_RESOURCE_DESC textureDesc = (*texture)->GetDesc();

	CD3DX12_HEAP_PROPERTIES heapProperty;
	D3D12_HEAP_FLAGS flags;

	(*texture)->GetHeapProperties(&heapProperty, &flags);
	const UINT subresoucesize
		= static_cast<UINT>(subresouceData.size());
	const UINT64 uploadBufferSize
		= GetRequiredIntermediateSize(*texture, 0, subresoucesize);

	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(textureUploadHeap)));

	UpdateSubresources(m_commandList.Get(), *texture, *textureUploadHeap, 0, 0, subresoucesize, &subresouceData[0]);
	pCommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(*texture,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = textureDesc.MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	m_device->CreateShaderResourceView(*texture,
		&srvDesc,
		cpuHandle);
}

// Update frame-based values.
void PBRSandbox12::OnUpdate()
{
	static float fY = 0.0f;

	// Initialize the world matrices
	XMMATRIX world = XMMatrixIdentity();

	world = XMMatrixRotationY(fY += 0.01f);

	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 16.0f/9.0f, 0.1f, 100);
	XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0, 0, 3.0f,0.0f), XMVectorSet(0,0,0, 0), XMVectorSet(0,1,0, 0));
		
	m_vsConstantBufferData.mWVP = world * view * proj;
	m_vsConstantBufferData.mWVP = XMMatrixTranspose(m_vsConstantBufferData.mWVP);
	m_vsConstantBufferData.mWorld = XMMatrixTranspose(world);
	memcpy(m_pVSCbvDataBegin, &m_vsConstantBufferData, sizeof(m_vsConstantBufferData));

	m_psConstatnBufferData.view[0] = 0.0f;
	m_psConstatnBufferData.view[1] = 0.0f;
	m_psConstatnBufferData.view[2] = 3.0f;
	memcpy(m_pPSCbvDataBegin, &m_psConstatnBufferData, sizeof(m_psConstatnBufferData));

	memcpy(m_pPLightSCbvDataBegin, &m_psLightConstatnBufferData, sizeof(m_psLightConstatnBufferData));
}

// Render the scene.
void PBRSandbox12::OnRender()
{
	IMGuiUpdate();

	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void PBRSandbox12::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CloseHandle(m_fenceEvent);
}

//
void PBRSandbox12::IMGuiUpdate()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	static bool isWindowed = true;
	static int radioButton = 0;
	ImGui::Begin("Material Window", &isWindowed);
	
//	ImGui::SliderFloat4("BaseColor",m_psConstatnBufferData.baseColor, 0.0, 1.f);
	ImGui::ColorEdit3("BaseColor", m_psConstatnBufferData.baseColor);
	m_psConstatnBufferData.baseColor[3] = 1.0f;
	ImGui::SliderFloat("Metallic", &m_psConstatnBufferData.MetallicRougnessReflectance[0], 0.0f, 1.0f);
	ImGui::SliderFloat("Roughness", &m_psConstatnBufferData.MetallicRougnessReflectance[1], 0.001f, 1.0f);
	ImGui::SliderFloat("Reflectance", &m_psConstatnBufferData.MetallicRougnessReflectance[2], 0.0f, 1.0f);
	ImGui::ColorEdit3("Ambient color", m_psConstatnBufferData.ambientColor);

	ImGui::End();

	ImGui::Begin("Light Window", &isWindowed);

	ImGui::SliderFloat3("Direction", m_psLightConstatnBufferData.direction, -1.f, 1.f);
	ImGui::SliderFloat("Intensity", &m_psLightConstatnBufferData.intensity, 0.0f, 10.0f);
	ImGui::ColorEdit3("LightColor", m_psLightConstatnBufferData.lightColor);

	ImGui::End();

}

void PBRSandbox12::PopulateCommandList()
{
	ThrowIfFailed(m_commandAllocator->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_dsvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	CD3DX12_GPU_DESCRIPTOR_HANDLE cbSceneGPUHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), SCENE_CONSTANT_BUFFER, m_srvDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(0, cbSceneGPUHandle);
	
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbMaterialGPUHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), MATERIAL_CONSTANT_BUFFER, m_srvDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(1, cbMaterialGPUHandle);

	CD3DX12_GPU_DESCRIPTOR_HANDLE radianceHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), BASE_COLOR, m_srvDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(2, radianceHandle);
	

//	m_sphereMesh.DrawMesh(m_commandList.Get());

	m_model.DrawModel(m_commandList.Get());

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

void PBRSandbox12::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
