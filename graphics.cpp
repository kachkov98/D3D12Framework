#include "stdafx.h"
#include "graphics.h"

#ifdef _DEBUG
inline void SetName (ID3D12Object* object, LPCWSTR name)
{
	object->SetName (name);
}
#else
inline void SetName (ID3D12Object*, LPCWSTR)
{
}
#endif

#define NAME_D3D12_OBJECT(x) SetName(x.Get(), L#x)

Graphics::Graphics () :
	is_resize (true)
{

}

Graphics::~Graphics ()
{

}

void Graphics::Init ()
{
	Log ("Initializing Direct3D 12...");
	//Get assets path
	{
		const size_t path_size = 512;
		WCHAR path[path_size];
		DWORD size = GetModuleFileName (nullptr, path, path_size);
		if (size == 0 || size == path_size)
			throw framework_err ("Can not get full path to the application");
		WCHAR *last_slash = wcsrchr (path, L'\\');
		if (last_slash)
			*(last_slash + 1) = L'\0';
		assets_path = path;
		Log ("Assets located at: %s", CW2A(assets_path.c_str ()).m_psz);
	}

	//init graphics
	for (size_t i = 0; i < frame_count; i++)
		fence_values[i] = 0;
	LoadPipeline ();
	LoadAssets ();
	Log ("Direct3D 12 initialized successfully");
}

void Graphics::Destroy ()
{
	WaitForGpu ();
	THROWIFFAILED (swap_chain->SetFullscreenState (FALSE, nullptr), "Can not set fullscreen state");
	CloseHandle (fence_event);
}

void Graphics::Update ()
{

}

void Graphics::Render ()
{
	//Record command list for current scene
	RecordCommandList ();

	//Execute the command list
	ID3D12CommandList *command_lists[] = { command_list.Get () };
	command_queue->ExecuteCommandLists (_countof (command_lists), command_lists);

	//Present the frame.
	THROWIFFAILED (swap_chain->Present (1, 0),
				   "Can not present frame");

	NextFrame ();
}

void Graphics::Resize (int window_width, int window_height)
{
	Log ("Resizing window to %dx%d", window_width, window_height);
	
	width = window_width;
	height = window_height;

	//flush current GPU programs
	WaitForGpu ();

	//release swap chain resources
	for (UINT n = 0; n < frame_count; n++)
	{
		render_targets[n].Reset ();
		fence_values[n] = fence_values[frame_index];
	}

	//resize swap chain
	DXGI_SWAP_CHAIN_DESC desc = {};
	swap_chain->GetDesc (&desc);
	THROWIFFAILED (swap_chain->ResizeBuffers (frame_count, width, height, desc.BufferDesc.Format, desc.Flags),
				   "Can not resize swap chain buffers");

	//reset frame index
	frame_index = swap_chain->GetCurrentBackBufferIndex ();

	is_resize = true;
}

void Graphics::LoadPipeline ()
{
	//enable debug layer
	#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED (D3D12GetDebugInterface (IID_PPV_ARGS (&debugController))))
		debugController->EnableDebugLayer ();
	else
		throw framework_err ("Can not enable DirectX debug layer");
	Log ("Directx debug layer initialized successfully");
	#endif
	
	ComPtr<IDXGIFactory4> factory = nullptr;
	ComPtr<IDXGIAdapter1> adapter = nullptr;
	//get hardware adapter
	{
		THROWIFFAILED (CreateDXGIFactory1 (IID_PPV_ARGS (&factory)), "Can not create dxgi factory");
		//print all devices
		char adapter_info[128];
		Log ("Detecting graphics adapters...");
		for (UINT adapterIndex = 0; factory.Get ()->EnumAdapters1 (adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; adapterIndex++)
		{
			DXGI_ADAPTER_DESC1 adapterDesc;
			adapter->GetDesc1 (&adapterDesc);

			wcstombs_s (NULL, adapter_info, 128, adapterDesc.Description, 128);
			Log ("Adapter %u:", adapterIndex);
			Log ("\t%s", adapter_info);
			Log ("\tDedicated memory: %u MB", adapterDesc.DedicatedVideoMemory / 1024 / 1024);
			Log ("\tShared memory: %u MB", adapterDesc.SharedSystemMemory / 1024 / 1024);
			if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				Log ("\tSoftware adapter");
			else
				Log ("\tHardware adapter");

			//try to select current adapter
			if (device == nullptr && !(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
			{
				if (FAILED (D3D12CreateDevice (adapter.Get (), D3D_FEATURE_LEVEL_11_0, __uuidof (ID3D12Device), &device)))
					device = nullptr;
				else
					Log ("\tThis card was selected for Direct3D 12");
			}
		}
		if (device == nullptr)
			throw framework_err ("Can not select video card with 11_0 hardware support");
	}

	//create command queue
	{
		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		THROWIFFAILED (device->CreateCommandQueue (&queue_desc, IID_PPV_ARGS (&command_queue)),
					   "Can not create command queue");
		Log ("Command queue created successfully");
	}

	//create swapchain
	{
		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
		swap_chain_desc.BufferCount = frame_count;
		swap_chain_desc.Width = width;
		swap_chain_desc.Height = height;
		swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> temp_swap_chain;
		THROWIFFAILED (factory->CreateSwapChainForHwnd (command_queue.Get (),
														hWnd,
														&swap_chain_desc,
														nullptr,
														nullptr,
														&temp_swap_chain),
					   "Can not create swap chain");
		THROWIFFAILED (temp_swap_chain.As (&swap_chain), "Can not create swap chain");
		frame_index = swap_chain->GetCurrentBackBufferIndex ();
		Log ("Swap chain created successfully");
	}

	//create descriptor heaps
	//create render target view descriptor heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
		descriptor_heap_desc.NumDescriptors = frame_count;
		descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		THROWIFFAILED (device->CreateDescriptorHeap (&descriptor_heap_desc, IID_PPV_ARGS (&rtv_heap)),
					   "Can not create render target view descriptor heap");
		rtv_descriptor_size = device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		Log ("Render target view descriptor heap created successfully");
	}

	//create command allocators
	{
		for (int i = 0; i < frame_count; i++)
			THROWIFFAILED (device->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT,
														   IID_PPV_ARGS (&command_allocators[i])),
						   "Can not create command allocator");
		Log ("Command allocators for %u frames created successfully", frame_count);
	}

	Log ("Direct3D 12 pipeline initialized successfully");
}

void Graphics::LoadAssets ()
{
	try
	{
		//create an empty root signature
		{
			D3D12_ROOT_SIGNATURE_DESC root_signature_desc;

			root_signature_desc.NumParameters = 0;
			root_signature_desc.pParameters = nullptr;
			root_signature_desc.NumStaticSamplers = 0;
			root_signature_desc.pStaticSamplers = nullptr;
			root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			THROWIFFAILED (D3D12SerializeRootSignature (&root_signature_desc,
														D3D_ROOT_SIGNATURE_VERSION_1,
														&signature,
														&error),
						   "Can not serialize root signature");
			THROWIFFAILED (device->CreateRootSignature (0,
														signature->GetBufferPointer (),
														signature->GetBufferSize (),
														IID_PPV_ARGS (&root_signature)),
						   "Can not create root signature");
			Log ("root signature created successfully");
		}

		//create pipeline state object
		{
			//compile shaders
			ComPtr<ID3DBlob> vertex_shader;
			ComPtr<ID3DBlob> pixel_shader;
			ComPtr<ID3DBlob> error;
			
			#ifdef _DEBUG
			UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
			#else
			UINT compile_flags = 0;
			#endif

			THROWIFFAILED (D3DCompileFromFile (GetAssetPath (L"shaders.hlsl").c_str(),
											   nullptr,
											   nullptr,
											   "VSMain",
											   "vs_5_0",
											   compile_flags,
											   0,
											   &vertex_shader,
											   &error),
						   "Can not compile vertex shader");
			Log ("Vertex shader compiled successfully");

			THROWIFFAILED (D3DCompileFromFile (GetAssetPath (L"shaders.hlsl").c_str(),
											   nullptr,
											   nullptr,
											   "PSMain",
											   "ps_5_0",
											   compile_flags,
											   0,
											   &pixel_shader,
											   &error),
						   "Can not compile pixel shader");
			Log ("Pixel shader compiled successfully");

			//Describe vertex input layout
			D3D12_INPUT_ELEMENT_DESC input_element_desc[]=
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
			};

			//Describe and create pipeline state object; a little bit complicated)
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = { input_element_desc, _countof (input_element_desc) };
				pso_desc.pRootSignature = root_signature.Get ();
				//vertex shader
				pso_desc.VS.pShaderBytecode = vertex_shader.Get ()->GetBufferPointer ();
				pso_desc.VS.BytecodeLength = vertex_shader.Get ()->GetBufferSize ();
				//pixel shader
				pso_desc.PS.pShaderBytecode = pixel_shader.Get ()->GetBufferPointer ();
				pso_desc.PS.BytecodeLength = pixel_shader.Get ()->GetBufferSize ();
				//rasterization state
				pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
				pso_desc.RasterizerState.FrontCounterClockwise = FALSE;
				pso_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
				pso_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
				pso_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
				pso_desc.RasterizerState.DepthClipEnable = TRUE;
				pso_desc.RasterizerState.MultisampleEnable = FALSE;
				pso_desc.RasterizerState.AntialiasedLineEnable = FALSE;
				pso_desc.RasterizerState.ForcedSampleCount = 0;
				pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
				//blend state
				pso_desc.BlendState.AlphaToCoverageEnable = FALSE;
				pso_desc.BlendState.IndependentBlendEnable = FALSE;
				const D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_desc =
				{
					FALSE,FALSE,
					D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
					D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
					D3D12_LOGIC_OP_NOOP,
					D3D12_COLOR_WRITE_ENABLE_ALL,
				};
				for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
					pso_desc.BlendState.RenderTarget[i] = default_render_target_blend_desc;

				pso_desc.DepthStencilState.DepthEnable = FALSE;
				pso_desc.DepthStencilState.StencilEnable = FALSE;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				pso_desc.SampleDesc.Count = 1;

				THROWIFFAILED (device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)),
							   "Can not create pipeline state object");
				NAME_D3D12_OBJECT (pipeline_state);
				Log ("Pipeline state object created successfully");
			}
			
		}

		//create command list
		{
			THROWIFFAILED (device->CreateCommandList (0,
													  D3D12_COMMAND_LIST_TYPE_DIRECT,
													  command_allocators[frame_index].Get (),
													  nullptr,
													  IID_PPV_ARGS (&command_list)),
						   "Can not create command list");

			//optionally record something
			CreateFrameBuffers ();
			CreateVertexBuffers ();

			//close command list
			THROWIFFAILED (command_list->Close (), "Can not close command list");
			Log ("Command buffer created successfully");
		}

		//create frame and vertex buffers by executing a command list
		ID3D12CommandList *command_lists[] = { command_list.Get () };
		command_queue->ExecuteCommandLists (_countof (command_lists), command_lists);
		Log ("Command list executed successfully");

		//Create synchronization objects
		{
			THROWIFFAILED (device->CreateFence (0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&fence)),
						   "Can not create fence");
			fence_values[frame_index]++;
			fence_event = CreateEvent (nullptr, FALSE, FALSE, nullptr);
			if (fence_event == nullptr)
				THROWIFFAILED (HRESULT_FROM_WIN32 (GetLastError ()), "Can not create fence event");

			WaitForGpu ();

			Log ("Fence created successfully");
		}
	}
	catch (framework_err err)
	{
		throw framework_err ("Can not load assets");
	}
	Log ("Assets loaded successfully");
}

void Graphics::CreateFrameBuffers ()
{
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MaxDepth = 1.0f;

	scissor_rect.right = static_cast<LONG>(width);
	scissor_rect.bottom = static_cast<LONG>(height);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart ();

	//create render target for each frame
	for (UINT n = 0; n < frame_count; n++)
	{
		THROWIFFAILED (swap_chain->GetBuffer (n, IID_PPV_ARGS (&render_targets[n])),
					   "Can not get render target buffer");
		device->CreateRenderTargetView (render_targets[n].Get (), nullptr, rtv_handle);
		rtv_handle.ptr += 1 * rtv_descriptor_size;  //offset on 1 descriptor

		//set name for render targets
		WCHAR name[25];
		if (swprintf_s (name, L"render_targets[%u]", n) > 0)
			SetName (render_targets[n].Get (), name);
	}

	is_resize = false;

	Log ("Framebuffers created successfully");
}

void Graphics::CreateVertexBuffers ()
{
	Vertex triangle_vertices[] =
	{
		{ { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, -0.5f, 0.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } }
	};

	const UINT vertex_buffer_size = sizeof (triangle_vertices);

	D3D12_RESOURCE_DESC vertex_buffer_desc;
	vertex_buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertex_buffer_desc.Alignment = 0;
	vertex_buffer_desc.Width = vertex_buffer_size;
	vertex_buffer_desc.Height = 1;
	vertex_buffer_desc.DepthOrArraySize = 1;
	vertex_buffer_desc.MipLevels = 1;
	vertex_buffer_desc.Format = DXGI_FORMAT_UNKNOWN;
	vertex_buffer_desc.SampleDesc.Count = 1;
	vertex_buffer_desc.SampleDesc.Quality = 0;
	vertex_buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	vertex_buffer_desc.Flags = D3D12_RESOURCE_FLAG_NONE;


	D3D12_HEAP_PROPERTIES heap_properties;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 1;
	heap_properties.VisibleNodeMask = 1;

	heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	THROWIFFAILED (device->CreateCommittedResource (&heap_properties,
													D3D12_HEAP_FLAG_NONE,
													&vertex_buffer_desc,
													D3D12_RESOURCE_STATE_COPY_DEST,
													nullptr,
													IID_PPV_ARGS (&vertex_buffer)),
				   "Can not create vertex buffer resource");

	heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	THROWIFFAILED (device->CreateCommittedResource (&heap_properties,
													D3D12_HEAP_FLAG_NONE,
													&vertex_buffer_desc,
													D3D12_RESOURCE_STATE_GENERIC_READ,
													nullptr,
													IID_PPV_ARGS (&vertex_buffer_upload)),
				   "Can not create vertex buffer resource for upload");

	NAME_D3D12_OBJECT (vertex_buffer);

	//copy data to the upload heap and than copy to the vertex buffer
	UINT8 *vertex_data_begin;
	D3D12_RANGE read_range = { 0, 0 };
	THROWIFFAILED (vertex_buffer_upload->Map (0, &read_range, reinterpret_cast<void**>(&vertex_data_begin)),
				   "Can not get CPU pointer to vertex_buffer_upload");
	memcpy (vertex_data_begin, triangle_vertices, vertex_buffer_size);
	vertex_buffer_upload->Unmap (0, nullptr);

	command_list->CopyBufferRegion (vertex_buffer.Get (), 0, vertex_buffer_upload.Get (), 0, vertex_buffer_size);
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = vertex_buffer.Get ();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	
	command_list->ResourceBarrier (1, &barrier);

	//init vertex buffer view
	vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress ();
	vertex_buffer_view.StrideInBytes = sizeof (Vertex);
	vertex_buffer_view.SizeInBytes = vertex_buffer_size;

	Log ("Vertex buffers created successfully");
}

void Graphics::RecordCommandList ()
{
	//reset command allocator for current frame
	THROWIFFAILED (command_allocators[frame_index]->Reset (), "Can not reset command allocator");
	THROWIFFAILED (command_list->Reset (command_allocators[frame_index].Get (), pipeline_state.Get ()),
				   "Can not reset command list");

	//add commands to resize buffers
	if (is_resize)
		CreateFrameBuffers ();

	//set states
	command_list->SetGraphicsRootSignature (root_signature.Get ());

	command_list->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->RSSetViewports (1, &viewport);
	command_list->RSSetScissorRects (1, &scissor_rect);

	//set back buffer as render target
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = render_targets[frame_index].Get ();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	command_list->ResourceBarrier (1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
	rtv_handle.ptr = rtv_heap->GetCPUDescriptorHandleForHeapStart ().ptr + frame_index * rtv_descriptor_size;
	command_list->OMSetRenderTargets (1, &rtv_handle, FALSE, nullptr);

	//record commands
	const float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	command_list->ClearRenderTargetView (rtv_handle, clear_color, 0, nullptr);
	command_list->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->IASetVertexBuffers (0, 1, &vertex_buffer_view);

	//draw our triangle
	command_list->DrawInstanced (3, 1, 0, 0);

	//indicate that the back buffer will now be used to present
	barrier.Transition.pResource = render_targets[frame_index].Get ();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	command_list->ResourceBarrier (1, &barrier);

	THROWIFFAILED (command_list->Close (), "Can not close command list");
}

void Graphics::WaitForGpu ()
{
	THROWIFFAILED (command_queue->Signal (fence.Get (), fence_values[frame_index]), "Can not shedule a signal command");

	THROWIFFAILED (fence->SetEventOnCompletion (fence_values[frame_index], fence_event), "Can not set event");
	WaitForSingleObjectEx (fence_event, INFINITE, FALSE);

	fence_values[frame_index]++;
}

void Graphics::NextFrame ()
{
	const UINT64 current_fence_value = fence_values[frame_index];
	THROWIFFAILED (command_queue->Signal (fence.Get (), current_fence_value), "Can not shedule a signal command");

	//update the frame index
	frame_index = swap_chain->GetCurrentBackBufferIndex ();

	//wait until the next frame is ready to be rendered
	if (fence->GetCompletedValue () < fence_values[frame_index])
	{
		THROWIFFAILED (fence->SetEventOnCompletion (fence_values[frame_index], fence_event), "Can not set event");
		WaitForSingleObjectEx (fence_event, INFINITE, FALSE);
	}

	//set the fence value for the next frame
	fence_values[frame_index] = current_fence_value + 1;
}

std::wstring Graphics::GetAssetPath (LPCWSTR name)
{
	return assets_path + name;
}