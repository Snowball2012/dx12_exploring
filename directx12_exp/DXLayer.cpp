#include "DXLayer.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

namespace DXLayer
{
	static const int framebuffer_count = 3;
	// direct3d stuff
	ID3D12Device* device;											// direct3d device

	IDXGISwapChain3* swap_chain;									// swapchain used to switch between render targets

	ID3D12CommandQueue* command_queue;								// container for command lists

	ID3D12DescriptorHeap* rtv_descriptor_heap;						// a descriptor heap to hold resources like the render targets

	ID3D12Resource* render_targets[framebuffer_count];				// number of render targets equal to buffer count

	ID3D12CommandAllocator* command_allocator[framebuffer_count];	// we want enough allocators for each buffer * number of threads (we only have one thread)

	ID3D12GraphicsCommandList* command_list;						// a command list we can record commands into, then execute them to render the frame

	ID3D12Fence* fence[framebuffer_count];							// an object that is locked while our command list is being executed by the gpu. We need as many 
																	// as we have allocators (more if we want to know when the gpu is finished with an asset)

	HANDLE fence_event;

	UINT64 fence_value[framebuffer_count];							// this value is incremented each frame. each fence will have its own value

	int frame_index;												// current rtv we are on

	int rtv_descriptor_size;										// size of the rtv descriptor on the device (all front and back buffers will be the same size)

	// this will only call release if an object exists (prevents exceptions calling release on non existant objects)
	#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

	bool InitD3D(HWND window_handle, int width, int height, bool is_fullscreen)
	{
		HRESULT hr;

		// -- Create the Device -- //

		IDXGIFactory4* dxgi_factory;
		hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
		if (FAILED(hr))
		{
			return false;
		}

		IDXGIAdapter1* adapter; // adapters are the graphics card (this includes the embedded graphics on the motherboard)

		int adapter_index = 0; // we'll start looking for directx 12  compatible graphics devices starting at index 0

		bool adapter_found = false; // set this to true when a good one was found

		// find first hardware gpu that supports d3d 12
		while (dxgi_factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// we dont want a software device
				adapter_index++;
				continue;
			}

			// we want a device that is compatible with direct3d 12 (feature level 11 or higher)
			hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(hr))
			{
				adapter_found = true;
				break;
			}

			adapter_index++;
		}

		if (!adapter_found)
		{
			return false;
		}

		// Create the device
		hr = D3D12CreateDevice(
			adapter,
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&device)
		);

		if (FAILED(hr))
		{
			return false;
		}

		// -- Create the Command Queue -- //

		D3D12_COMMAND_QUEUE_DESC cq_desc = {}; // we will be using all the default values

		hr = device->CreateCommandQueue(&cq_desc, IID_PPV_ARGS(&command_queue)); // create the command queue
		
		if (FAILED(hr))
		{
			return false;
		}

		// -- Create the Swap Chain (double/tripple buffering) -- //

		DXGI_MODE_DESC back_buffer_desc = {}; // this is to describe our display mode
		back_buffer_desc.Width = width; // buffer width
		back_buffer_desc.Height = height; // buffer height
		back_buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the buffer (rgba 32 bits, 8 bits for each chanel)

															// describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
		DXGI_SAMPLE_DESC sample_desc = {};
		sample_desc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)

							  // Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
		swap_chain_desc.BufferCount = framebuffer_count; // number of buffers we have
		swap_chain_desc.BufferDesc = back_buffer_desc; // our back buffer description
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
		swap_chain_desc.OutputWindow = window_handle; // handle to our window
		swap_chain_desc.SampleDesc = sample_desc; // our multi-sampling description
		swap_chain_desc.Windowed = !is_fullscreen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps

		IDXGISwapChain* temp_swap_chain;

		dxgi_factory->CreateSwapChain(
			command_queue, // the queue will be flushed once the swap chain is created
			&swap_chain_desc, // give it the swap chain description we created above
			&temp_swap_chain // store the created swap chain in a temp IDXGISwapChain interface
		);

		swap_chain = static_cast<IDXGISwapChain3*>(temp_swap_chain);

		frame_index = swap_chain->GetCurrentBackBufferIndex();

		// -- Create the Back Buffers (render target views) Descriptor Heap -- //

		// describe an rtv descriptor heap and create
		D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
		rtv_heap_desc.NumDescriptors = framebuffer_count; // number of descriptors for this heap.
		rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // this heap is a render target view heap

		// This heap will not be directly referenced by the shaders (not shader visible), as this will store the output from the pipeline
		// otherwise we would set the heap's flag to D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_descriptor_heap));
		if (FAILED(hr))
			return false;

		// get the size of a descriptor in this heap (this is a rtv heap, so only rtv descriptors should be stored in it.
		// descriptor sizes may vary from device to device, which is why there is no set size and we must ask the 
		// device to give us the size. we will use this size to increment a descriptor handle offset
		rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
		// but we cannot literally use it like a c++ pointer.
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each buffer (double buffering is two buffers, tripple buffering is 3).
		for (int i = 0; i < framebuffer_count; i++)
		{
			// first we get the n'th buffer in the swap chain and store it in the n'th
			// position of our ID3D12Resource array
			hr = swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i]));
			if (FAILED(hr))
				return false;

			// the we "create" a render target view which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
			device->CreateRenderTargetView(render_targets[i], nullptr, rtv_handle);

			// we increment the rtv handle by the rtv descriptor size we got above
			rtv_handle.Offset(1, rtv_descriptor_size);
		}

		// -- Create the Command Allocators -- //

		for (int i = 0; i < framebuffer_count; i++)
		{
			hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator[i]));
			if (FAILED(hr))
				return false;
		}

		// -- Create the command list with the first allocator -- //
		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator[0], NULL, IID_PPV_ARGS(&command_list));
		if (FAILED(hr))
			return false;

		// command lists are created in the recording state. our main loop will set it up for recording again so close it now
		command_list->Close();

		// -- Create a Fence & Fence Event -- //

		// create the fences
		for (int i = 0; i < framebuffer_count; i++)
		{
			hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
			if (FAILED(hr))
				return false;

			fence_value[i] = 0; // set the initial fence value to 0
		}

		// create a handle to a fence event
		fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!fence_event)
			return false;

		return true;
	}

	bool Update()
	{
		return true;
	}

	bool UpdatePipeline()
	{
		HRESULT hr;

		// We have to wait for the gpu to finish with the command allocator before we reset it
		if (!WaitForPreviousFrame())
			return false;

		// we can only reset an allocator once the gpu is done with it
		// resetting an allocator frees the memory that the command list was stored in
		hr = command_allocator[frame_index]->Reset();
		if (FAILED(hr))
			return false;

		// reset the command list. by resetting the command list we are putting it into
		// a recording state so we can start recording commands into the command allocator.
		// the command allocator that we reference here may have multiple command lists
		// associated with it, but only one can be recording at any time. Make sure
		// that any other command lists associated to this command allocator are in
		// the closed state (not recording).
		// Here you will pass an initial pipeline state object as the second parameter,
		// but in this tutorial we are only clearing the rtv, and do not actually need
		// anything but an initial default pipeline, which is what we get by setting
		// the second parameter to NULL
		hr = command_list->Reset(command_allocator[frame_index], nullptr);
		if (FAILED(hr))
			return false;

		// here we start recording commands into the commandList (which all the commands will be stored in the commandAllocator)

		// transition the "frameIndex" render target from the present state to the render target state so the command list draws to it starting from here
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(render_targets[frame_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		// here we again get the handle to our current render target view so we can set it as the render target in the output merger stage of the pipeline
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), frame_index, rtv_descriptor_size);

		// set the render target for the output merger stage (the output of the pipeline)
		command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

		// Clear the render target by using the ClearRenderTargetView command
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		command_list->ClearRenderTargetView(rtv_handle, clearColor, 0, nullptr);

		// transition the "frameIndex" render target from the render target state to the present state. If the debug layer is enabled, you will receive a
		// warning if present is called on the render target when it's not in the present state
		command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(render_targets[frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		hr = command_list->Close();
		if (FAILED(hr))
			return false;

		return true;
	}

	bool Render()
	{
		HRESULT hr;

		if (!UpdatePipeline())
			return false;

		// create an array of command lists (only one command list here)
		ID3D12CommandList* command_lists[] = { command_list };

		// execute the array of command lists
		command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

		// this command goes in at the end of our command queue. we will know when our command queue 
		// has finished because the fence value will be set to "fenceValue" from the GPU since the command
		// queue is being executed on the GPU
		hr = command_queue->Signal(fence[frame_index], fence_value[frame_index]);
		if (FAILED(hr))
			return false;

		// present the current backbuffer
		hr = swap_chain->Present(0, 0);
		if (FAILED(hr))
			return false;

		return true;
	}

	void Cleanup()
	{
		// wait for the gpu to finish all frames
		for (int i = 0; i < framebuffer_count; ++i)
		{
			frame_index = i;
			WaitForPreviousFrame(); // cleanup, don't care about errors
		}

		// get swapchain out of full screen before exiting
		BOOL fs = false;
		if (swap_chain->GetFullscreenState(&fs, NULL))
			swap_chain->SetFullscreenState(false, NULL);

		SAFE_RELEASE(device);
		SAFE_RELEASE(swap_chain);
		SAFE_RELEASE(command_queue);
		SAFE_RELEASE(rtv_descriptor_heap);
		SAFE_RELEASE(command_list);

		for (int i = 0; i < framebuffer_count; ++i)
		{
			SAFE_RELEASE(render_targets[i]);
			SAFE_RELEASE(command_allocator[i]);
			SAFE_RELEASE(fence[i]);
		};
	}
	
	bool WaitForPreviousFrame()
	{
		HRESULT hr;

		// swap the current rtv buffer index so we draw on the correct buffer
		frame_index = swap_chain->GetCurrentBackBufferIndex();

		// if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
		// the command queue since it has not reached the "commandQueue->Signal(fence, fenceValue)" command
		if (fence[frame_index]->GetCompletedValue() < fence_value[frame_index])
		{
			// we have the fence create an event which is signaled once the fence's current value is "fenceValue"
			hr = fence[frame_index]->SetEventOnCompletion(fence_value[frame_index], fence_event);
			if (FAILED(hr))
				return false;

			// We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
			// has reached "fenceValue", we know the command queue has finished executing
			WaitForSingleObject(fence_event, INFINITE);
		}

		// increment fenceValue for next frame
		fence_value[frame_index]++;
		return true;
	}
};