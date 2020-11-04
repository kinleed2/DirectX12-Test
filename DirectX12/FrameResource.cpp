#include "FrameResource.h"

//FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT waveVertCount)
//{
//    ThrowIfFailed(device->CreateCommandAllocator(
//        D3D12_COMMAND_LIST_TYPE_DIRECT,
//        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
//
//    //  FrameCB = std::make_unique<UploadBuffer<FrameConstants>>(device, 1, true);
//    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
//    MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
//    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
//
//    WavesVB = std::make_unique<UploadBuffer<Vertex>>(device, waveVertCount, false);
//}

//Instance
FrameResource::FrameResource(
    ID3D12Device* device, 
    UINT passCount, 
    UINT objectCount, 
    UINT maxInstanceCount, 
    UINT materialCount, 
    UINT skinnedObjectCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
    InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, maxInstanceCount, false);
    SkinnedCB = std::make_unique<UploadBuffer<SkinnedConstants>>(device, skinnedObjectCount, true);

}

//FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT skinnedObjectCount, UINT materialCount)
//{
//    ThrowIfFailed(device->CreateCommandAllocator(
//        D3D12_COMMAND_LIST_TYPE_DIRECT,
//        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
//
//    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
//    SsaoCB = std::make_unique<UploadBuffer<SsaoConstants>>(device, 1, true);
//    MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
//    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
//    SkinnedCB = std::make_unique<UploadBuffer<SkinnedConstants>>(device, skinnedObjectCount, true);
//}

FrameResource::~FrameResource()
{

}