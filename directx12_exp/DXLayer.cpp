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

	UINT64 fence_value[framebuffer_count];							// this value is incremented each frame. each fence will have its own value

	int frame_index;												// current rtv we are on

	int rtv_descriptor_size;										// size of the rtv descriptor on the device (all front and back buffers will be the same size)

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

		return true;
	}

	void Update()
	{
	}

	void UpdatePipeline()
	{
	}

	void Render()
	{
	}

	void Cleanup()
	{
	}
	
	void WaitForPreviousFrame()
	{
	}
};