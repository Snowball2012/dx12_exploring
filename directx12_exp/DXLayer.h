#pragma once

#include <windows.h>

namespace DXLayer
{
	bool InitD3D(HWND window_handle, int width, int height, bool is_fullscreen); // initializes direct3d 12

	bool Update(); // update the game logic

	bool UpdatePipeline(); // update the direct3d pipeline (update command lists)

	bool Render(); // execute the command list

	void Cleanup(); // release com ojects and clean up memory

	bool WaitForPreviousFrame(); // wait until gpu is finished with command list

	extern HANDLE fence_event;	// a handle to an event when our fence is unlocked by the gpu
}