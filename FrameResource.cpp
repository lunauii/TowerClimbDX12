#include "FrameResource.h"

// The constructor: This runs when the game creates a new "Frame" of data (usually 3 frames)
FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
{
    // 1. Initialize the Command Allocator
    // Each frame needs its own allocator so the CPU can start working on the 
    // next frame while the GPU is still drawing the previous one.
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    // 2. Initialize the Constant Buffers
    // PassCB: Stores global data (Camera View, Projection, Lighting)
    // ObjectCB: Stores data for every object (The Tower, the Platforms)
    // The 'true' parameter tells the UploadBuffer that this is a Constant Buffer (handles 256-byte alignment)
    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
}

// The destructor: This MUST exist to fix your "unresolved external symbol" error.
// Even if it's empty, the compiler needs to see this function to finish the build.
FrameResource::~FrameResource()
{
}