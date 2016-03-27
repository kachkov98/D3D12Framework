#pragma once
#include "stdafx.h"
#include "errors.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Graphics
{
public:
	int width, height;
	HWND hWnd;

	Graphics ();
	~Graphics ();
	void Init ();
	void Destroy ();
	void Update ();
	void Render ();
	void Resize (int window_width, int window_height);
private:
	void LoadPipeline ();
	void LoadAssets ();
	void CreateFrameBuffers ();
	void CreateVertexBuffers ();
	void RecordCommandList ();
	void WaitForGpu ();
	void NextFrame ();
	std::wstring GetAssetPath (LPCWSTR name);

	std::wstring assets_path;

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT4 color;
	};

	static const UINT frame_count = 2;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissor_rect;
	bool is_resize;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> command_queue;
	ComPtr<IDXGISwapChain3> swap_chain;
	ComPtr<ID3D12DescriptorHeap> rtv_heap;
	UINT rtv_descriptor_size;
	ComPtr<ID3D12CommandAllocator> command_allocators[frame_count];
	ComPtr<ID3D12GraphicsCommandList> command_list;
	ComPtr<ID3D12RootSignature> root_signature;
	ComPtr<ID3D12PipelineState> pipeline_state;
	ComPtr<ID3D12Resource> render_targets[frame_count];

	ComPtr<ID3D12Resource> vertex_buffer;
	ComPtr<ID3D12Resource> vertex_buffer_upload;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;

	//for synchronization
	UINT frame_index;
	HANDLE fence_event;
	ComPtr<ID3D12Fence> fence;
	UINT64 fence_values[frame_count];
};
