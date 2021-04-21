#include "Graphics.h"
#include <iostream>

const int gNumFrameResources = 3;

Graphics::Graphics(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
    // Estimate the scene bounding sphere manually since we know how the scene was constructed.
    // The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
    // the world space origin.  In general, you need to loop over every world space vertex
    // position and compute the bounding sphere.
    mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    mSceneBounds.Radius = sqrtf(10.0f*10.0f + 15.0f*15.0f);

}

Graphics::~Graphics()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}

bool Graphics::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
    // so we have to query this information.
    //mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

    mCamera.SetPosition(0.0f, 2.0f, -15.0f);

    mShadowMap = std::make_unique<ShadowMap>(
        md3dDevice.Get(), 1024, 1024);

    //fbxmesh = std::make_unique<FBXMesh>(fbx,md3dDevice.Get(), mCommandList.Get());


    LoadContents();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
    BuildSkullGeometry();
    
    //BuildTreeSpritesGeometry();
    //BuildLandGeometry();
    //BuildWavesGeometry();
    //BuildBoxGeometry();

    BuildMaterials();
    BuildRenderItems();
    BuildSkinnedRenderItems();
    BuildInstanceRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}

void Graphics::CreateRtvAndDsvDescriptorHeaps()
{
    // Add +6 RTV for cube render target.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = mSwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

    // Add +1 DSV for shadow map.
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 2;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void Graphics::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

    BoundingFrustum::CreateFromMatrix(mCamFrustum, mCamera.GetProj());
}

void Graphics::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, NULL, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    //AnimateMaterials(gt);
    UpdateInstanceData(gt);
    UpdateObjectCBs(gt);
    UpdateSkinnedCBs(gt);
    UpdateMaterialBuffer(gt);
    UpdateShadowTransform(gt);
    UpdateMainPassCB(gt);
    UpdateShadowPassCB(gt);
    //UpdateReflectedPassCB(gt);
   
    //UpdateWaves(gt);
}

void Graphics::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void Graphics::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Graphics::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        mCamera.Pitch(dy);
        mCamera.RotateY(dx);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void Graphics::OnKeyboardInput(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();

    if (GetAsyncKeyState('W') & 0x8000) mCamera.Walk(10.0f * dt);
    if (GetAsyncKeyState('S') & 0x8000) mCamera.Walk(-10.0f * dt);
    if (GetAsyncKeyState('D') & 0x8000) mCamera.Strafe(10.0f * dt);
    if (GetAsyncKeyState('A') & 0x8000) mCamera.Strafe(-10.0f * dt);

    if (GetAsyncKeyState(VK_UP) & 0x8000) mCamera.Walk(10.0f * dt);
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) mCamera.Walk(-10.0f * dt);
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) mCamera.Strafe(10.0f * dt);
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) mCamera.Strafe(-10.0f * dt);

    if (GetAsyncKeyState('1') & 0x0001)
    {
        if (!mFrustumCullingEnabled)
        {
            std::cout << "************\n Frustum Culling on \n************\n";
            mFrustumCullingEnabled = true;
        }
        else
        {
            std::cout << "************\n Frustum Culling off \n************\n";
            mFrustumCullingEnabled = false;
        }

    }
    mCamera.UpdateViewMatrix();

    if (GetAsyncKeyState('2') & 0x0001)
    {
        if (sunTurn)
        {
            std::cout << "************\n sun rotation off \n************\n";
            sunTurn = false;
        }
        else
        {
            std::cout << "************\n sun rotation on \n************\n";
            sunTurn = true;
        }

    }
    

    if (GetAsyncKeyState('3') & 0x0001)
    {
        if (debugFlag)
        {
            std::cout << "************\n shadow debug off \n************\n";
            debugFlag = false;
        }
        else
        {
            std::cout << "************\n shadow debug on \n************\n";
            debugFlag = true;
        }
    }
    if (GetAsyncKeyState('4') & 0x0001)
    {
        if (skull)
        {
            std::cout << "************\n skull off \n************\n";
            skull = false;
        }
        else
        {
            std::cout << "************\n skull on \n************\n";
            skull = true;
        }
    }
    if (GetAsyncKeyState('5') & 0x0001)
    {
        if (model)
        {
            std::cout << "************\n model off \n************\n";
            model = false;
        }
        else
        {
            std::cout << "************\n model on \n************\n";
            model = true;
        }
    }
    if (GetAsyncKeyState('6') & 0x0001)
    {
        if (object)
        {
            std::cout << "************\n object off \n************\n";
            object = false;
        }
        else
        {
            std::cout << "************\n object on \n************\n";
            object = true;
        }
    }




    if (GetAsyncKeyState('Q') & 0x8000) mLightRotationAngle -= 1 * dt;
    if (GetAsyncKeyState('E') & 0x8000) mLightRotationAngle += 1 * dt;

    if (sunTurn)
    {
        mLightRotationAngle -= 0.1 * dt;
    }


    XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
    for (int i = 0; i < 3; ++i)
    {
        XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
        lightDir = XMVector3TransformNormal(lightDir, R);
        XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
    }

    if(GetAsyncKeyState('R') & 0x8000)
    {
        UpdateSkinnedCBs(gt);
    }

    if (GetAsyncKeyState(VK_SPACE) & 0x0001)
    {
        animation_index++;
        if (animation_index >= animation_name.size())
        {
            animation_index = 0;
        }
        std::cout << animation_name[animation_index] << std::endl;
    }

    if (GetAsyncKeyState('J') & 0x0001)
    {
        animation_speed += 0.25f;
        std::cout << animation_speed << std::endl;
    }

    if (GetAsyncKeyState('K') & 0x0001)
    {
        animation_speed -= 0.25f;
        std::cout << animation_speed << std::endl;
    }
    

}

void Graphics::AnimateMaterials(const GameTimer& gt)
{
    // Scroll the water material texture coordinates.
    auto waterMat = mMaterials["water"].get();

    float& tu = waterMat->MatTransform(3, 0);
    float& tv = waterMat->MatTransform(3, 1);

    tu += 0.1f * gt.DeltaTime();
    tv += 0.02f * gt.DeltaTime();

    if (tu >= 1.0f)
        tu -= 1.0f;

    if (tv >= 1.0f)
        tv -= 1.0f;

    waterMat->MatTransform(3, 0) = tu;
    waterMat->MatTransform(3, 1) = tv;

    // Material has changed, so need to update cbuffer.
    waterMat->NumFramesDirty = gNumFrameResources;
}

void Graphics::UpdateObjectCBs(const GameTimer& gt)
{
    
    float animation_tick = animation_speed * gt.DeltaTime();
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        // Only update the cbuffer data if the constants have changed.  
        // This needs to be tracked per frame resource.
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->World);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
            objConstants.MaterialIndex = e->Mat->MatCBIndex;

            currObjectCB->CopyData(e->ObjCBIndex, objConstants);
            //Next FrameResource need to be updated too.
            e->NumFramesDirty--;
        }
    }
}

void Graphics::UpdateMaterialBuffer(const GameTimer& gt)
{
    auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialData matData;
            matData.DiffuseAlbedo = mat->DiffuseAlbedo;
            matData.FresnelR0 = mat->FresnelR0;
            matData.Roughness = mat->Roughness;
            XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
            matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
            matData.NormalMapIndex = mat->NormalSrvHeapIndex;
            matData.SpecularMapIndex = mat->SpecularSrvHeapIndex;


            currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}

void Graphics::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMVECTOR viewVector = XMMatrixDeterminant(view);
    XMMATRIX invView = XMMatrixInverse(&viewVector, view);
    XMVECTOR projVector = XMMatrixDeterminant(proj);
    XMMATRIX invProj = XMMatrixInverse(&projVector, proj);
    XMVECTOR viewProjVector = XMMatrixDeterminant(viewProj);
    XMMATRIX invViewProj = XMMatrixInverse(&viewProjVector, viewProj);

    XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    
    XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
    
    mMainPassCB.EyePosW = mCamera.GetPosition3f();
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];
    mMainPassCB.Lights[0].Strength = { 0.9f, 0.8f, 0.7f };
    mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
    mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
    mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);

}

void Graphics::UpdateReflectedPassCB(const GameTimer& gt)
{
    mReflectedPassCB = mMainPassCB;

    XMVECTOR mirrorPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX R = XMMatrixReflect(mirrorPlane);

    for (int i = 0; i < 3; i++)
    {
        XMVECTOR lightDir = XMLoadFloat3(&mMainPassCB.Lights[i].Direction);
        XMVECTOR reflectedLightDir = XMVector3TransformNormal(lightDir, R);
        XMStoreFloat3(&mReflectedPassCB.Lights[i].Direction, reflectedLightDir);
    }

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(1, mReflectedPassCB);
}

//void Graphics::UpdateWaves(const GameTimer& gt)
//{
//    // Every quarter second, generate a random wave.
//    static float t_base = 0.0f;
//    if ((mTimer.TotalTime() - t_base) >= 0.25f)
//    {
//        t_base += 0.25f;
//
//        int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
//        int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);
//
//        float r = MathHelper::RandF(0.2f, 0.5f);
//
//        mWaves->Disturb(i, j, r);
//    }
//
//    // Update the wave simulation.
//    mWaves->Update(gt.DeltaTime());
//
//    // Update the wave vertex buffer with the new solution.
//    auto currWavesVB = mCurrFrameResource->WavesVB.get();
//    for (int i = 0; i < mWaves->VertexCount(); ++i)
//    {
//        Vertex v;
//
//        v.Pos = mWaves->Position(i);
//        v.Normal = mWaves->Normal(i);
//
//        // Derive tex-coords from position by 
//        // mapping [-w/2,w/2] --> [0,1]
//        v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
//        v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();
//
//        currWavesVB->CopyData(i, v);
//    }
//
//    // Set the dynamic VB of the wave renderitem to the current frame VB.
//    mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
//}

void Graphics::UpdateInstanceData(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView();
    XMVECTOR viewVector = XMMatrixDeterminant(view);
    XMMATRIX invView = XMMatrixInverse(&viewVector, view);

    auto currInstanceBuffer = mCurrFrameResource->InstanceBuffer.get();
    for (auto& e : mAllInstanceRitems)
    {
        const auto& instanceData = e->Instances;

        int visibleInstanceCount = 0;

        for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
        {
            XMMATRIX world = XMLoadFloat4x4(&instanceData[i].World);
            XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);
            XMVECTOR worldVector = XMMatrixDeterminant(world);
            XMMATRIX invWorld = XMMatrixInverse(&worldVector, world);

            // View space to the object's local space.
            XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

            // Transform the camera frustum from view space to the object's local space.
            BoundingFrustum localSpaceFrustum;
            mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

            // Perform the box/frustum intersection test in local space.
            if ((localSpaceFrustum.Contains(e->Bounds) != DirectX::DISJOINT) || (mFrustumCullingEnabled == false))
            {
                InstanceData data;
                XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
                XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
                data.MaterialIndex = instanceData[i].MaterialIndex;

                // Write the instance data to structured buffer for the visible objects.
                currInstanceBuffer->CopyData(visibleInstanceCount++, data);
            }
        }

        e->InstanceCount = visibleInstanceCount;

        std::wostringstream outs;
        outs.precision(6);
        outs << L"Demo" <<
            L"    " << e->InstanceCount <<
            L" objects visible out of " << e->Instances.size();
        mMainWndCaption = outs.str();
    }
}

void Graphics::UpdateShadowTransform(const GameTimer& gt)
{
    // Only the first "main" light casts a shadow.
    XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
    XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
    XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
    XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

    XMStoreFloat3(&mLightPosW, lightPos);

    // Transform bounding sphere to light space.
    XMFLOAT3 sphereCenterLS;
    XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

    // Ortho frustum in light space encloses scene.
    float l = sphereCenterLS.x - mSceneBounds.Radius;
    float b = sphereCenterLS.y - mSceneBounds.Radius;
    float n = sphereCenterLS.z - mSceneBounds.Radius;
    float r = sphereCenterLS.x + mSceneBounds.Radius;
    float t = sphereCenterLS.y + mSceneBounds.Radius;
    float f = sphereCenterLS.z + mSceneBounds.Radius;

    mLightNearZ = n;
    mLightFarZ = f;
    XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

    // Transform NDC space [-1,+1]^2 to texture space [0,1]^2
    XMMATRIX T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);

    XMMATRIX S = lightView * lightProj * T;
    XMStoreFloat4x4(&mLightView, lightView);
    XMStoreFloat4x4(&mLightProj, lightProj);
    XMStoreFloat4x4(&mShadowTransform, S);
}

void Graphics::UpdateShadowPassCB(const GameTimer& gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mLightView);
    XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMVECTOR viewVector = XMMatrixDeterminant(view);
    XMMATRIX invView = XMMatrixInverse(&viewVector, view);
    XMVECTOR projVector = XMMatrixDeterminant(proj);
    XMMATRIX invProj = XMMatrixInverse(&projVector, proj);
    XMVECTOR viewProjVector = XMMatrixDeterminant(viewProj);
    XMMATRIX invViewProj = XMMatrixInverse(&viewProjVector, viewProj);

    UINT w = mShadowMap->Width();
    UINT h = mShadowMap->Height();

    XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mShadowPassCB.EyePosW = mLightPosW;
    mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
    mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
    mShadowPassCB.NearZ = mLightNearZ;
    mShadowPassCB.FarZ = mLightFarZ;

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(1, mShadowPassCB);
}

void Graphics::UpdateSkinnedCBs(const GameTimer& gt)
{
    float animation_tick = animation_speed * gt.DeltaTime();
    auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();
    SkinnedConstants skinnedConstants;

        if (extra_animations[animation_name[animation_index]].size() > 0)
        {
            int frame = extra_animations[animation_name[animation_index]].animation_tick /
                extra_animations[animation_name[animation_index]].sampling_time;
            if (frame > extra_animations[animation_name[animation_index]].size() - 1)
            {
                frame = 0;
                extra_animations[animation_name[animation_index]].animation_tick = 0;
            }
            std::vector<Bone>& skeletal = extra_animations[animation_name[animation_index]].at(frame);
            size_t number_of_bones = skeletal.size();
            _ASSERT_EXPR(number_of_bones < MAX_BONES, L"'the number_of_bones' exceeds MAX_BONES.");
            for (size_t i = 0; i < number_of_bones; i++)
            {
                XMStoreFloat4x4(&skinnedConstants.BoneTransforms[i], XMLoadFloat4x4(&skeletal.at(i).transform));
            }
            extra_animations[animation_name[animation_index]].animation_tick += animation_tick;
        }

    currSkinnedCB->CopyData(0, skinnedConstants);
}

void Graphics::LoadContents()
{
    LoadFBX(fbx);
    std::vector<std::wstring> motionNames =
    {
        L"../Models/armada.fbx",
        L"../Models/au to role.fbx",
        L"../Models/bencao.fbx",
        L"../Models/capoeira (2).fbx",
        L"../Models/chapa giratoria 2.fbx",
        L"../Models/chapa-giratoria.fbx",
        L"../Models/martelo do chau sem mao.fbx",
        L"../Models/meia lua de compasso.fbx",
        L"../Models/chapaeu de couro.fbx",
    };

    for (int i = 0; i < (int)motionNames.size(); ++i)
    {
        LoadFBX(motionNames[i]);
    }


    std::vector<std::string> texNames =
    {
        "bricks",
        "stone",
        "grass",
        "ice",
        "bricksDiffuseMap",
        "bricksNormalMap",
        "tileDiffuseMap",
        "tileNormalMap",
        "defaultDiffuseMap",
        "defaultNormalMap",
        "skyCubeMap"
    };

    std::vector<std::wstring> texFilenames =
    {
        L"../Textures/bricks.dds",
        L"../Textures/stone.dds",
        L"../Textures/grass.dds",
        L"../Textures/ice.dds",
        L"../Textures/bricks2.dds",
        L"../Textures/bricks2_nmap.dds",
        L"../Textures/tile.dds",
        L"../Textures/tile_nmap.dds",
        L"../Textures/white1x1.dds",
        L"../Textures/default_nmap.dds",
        L"../Textures/snowcube1024.dds",
        
    };

    int modelTexBegin = texNames.size();

    for (auto& mesh : meshes)
    {
        for (auto& subset : mesh.subsets)
        {
            if (subset.material.DiffuseMapName != "")
            {
                std::wstring diffuse = AnsiToWString(subset.material.DiffuseMapName);
                std::string name = subset.material.DiffuseMapName.substr(subset.material.DiffuseMapName.find_last_of("/\\") + 1,
                    subset.material.DiffuseMapName.find_last_of("."));

                texFilenames.push_back(L"../Models/" + diffuse);
                texNames.push_back(name);
                
            }
            if (subset.material.NormalMapName != "")
            {
                std::wstring normal = AnsiToWString(subset.material.NormalMapName);
                std::string name = subset.material.NormalMapName.substr(subset.material.NormalMapName.find_last_of("/\\") + 1,
                    subset.material.NormalMapName.find_last_of("."));

                texFilenames.push_back(L"../Models/" + normal); 
                texNames.push_back(name);
            }
            if (subset.material.SpecularName != "")
            {
                std::wstring specular = AnsiToWString(subset.material.SpecularName);
                std::string name = subset.material.SpecularName.substr(subset.material.SpecularName.find_last_of("/\\") + 1,
                    subset.material.SpecularName.find_last_of("."));

                texFilenames.push_back(L"../Models/" + specular);
                texNames.push_back(name);
            }
        }
    }

    for (int i = 0; i < (int)texNames.size(); ++i)
    {
        // Don't create duplicates.
        if (mTextures.find(texNames[i]) == std::end(mTextures))
        {
            LoadTextures(texFilenames[i], texNames[i]);

            if (i >= modelTexBegin)
            {
                mModelTextureNames.push_back(texNames[i]);
            }
        }
    }
    for (auto i = extra_animations.begin(); i != extra_animations.end(); ++i)
    {
        animation_name.push_back(i->second.name);
    }
}

void Graphics::LoadTextures(const std::wstring filename, std::string texName)
{
    auto tex = std::make_unique<Texture>();
    int texType;

    tex->Filename = filename;
    tex->Name = texName;

    std::unique_ptr<uint8_t[]> texData;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);

    if (filename.rfind(L"dds") != std::wstring::npos)
    {

        std::vector<D3D12_SUBRESOURCE_DATA> subresources;

        ThrowIfFailed(LoadDDSTextureFromFile(
            md3dDevice.Get(),
            tex->Filename.c_str(),
            tex->Resource.ReleaseAndGetAddressOf(),
            texData,
            subresources));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex->Resource.Get(), 0,
            static_cast<UINT>(subresources.size()));

        // Create the GPU upload buffer.

        auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &uploadHeap,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(tex->UploadHeap.GetAddressOf())));

        UpdateSubresources(mCommandList.Get(), tex->Resource.Get(), tex->UploadHeap.Get(),
            0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    }
    else if (filename.rfind(L"tga") != std::wstring::npos)
    {
        auto scratchImage = std::make_unique<ScratchImage>();
        auto metadata = std::make_unique<TexMetadata>();
        HRESULT hr = LoadFromTGAFile(filename.c_str(), metadata.get(), *scratchImage);
        if (FAILED(hr));

        D3D12_RESOURCE_DESC desc = {};
        switch (metadata->dimension)
        {
        case TEX_DIMENSION_TEXTURE1D:
            desc = CD3DX12_RESOURCE_DESC::Tex1D(
                metadata->format,
                static_cast<UINT64>(metadata->width),
                static_cast<UINT16>(metadata->arraySize));
            break;
        case TEX_DIMENSION_TEXTURE2D:
            desc = CD3DX12_RESOURCE_DESC::Tex2D(
                metadata->format,
                static_cast<UINT64>(metadata->width),
                static_cast<UINT>(metadata->height),
                static_cast<UINT16>(metadata->arraySize));
            break;
        case TEX_DIMENSION_TEXTURE3D:
            desc = CD3DX12_RESOURCE_DESC::Tex3D(
                metadata->format,
                static_cast<UINT64>(metadata->width),
                static_cast<UINT>(metadata->height),
                static_cast<UINT16>(metadata->depth));
            break;
        default:
            throw std::exception("Invalid texture dimension.");
            break;
        }
        
        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &defaultHeap,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(tex->Resource.ReleaseAndGetAddressOf())));

        std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage->GetImageCount());

        auto img = scratchImage->GetImages();

        for (int i = 0; i < scratchImage->GetImageCount(); ++i)
        {
            auto& subresource = subresources[i];
            subresource.RowPitch = img[i].rowPitch;
            subresource.SlicePitch = img[i].slicePitch;
            subresource.pData = img[i].pixels;
        }
        
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex->Resource.Get(), 0,
            static_cast<uint32_t>(subresources.size()));
        desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &uploadHeap,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(tex->UploadHeap.GetAddressOf())));

        UpdateSubresources(mCommandList.Get(), tex->Resource.Get(), tex->UploadHeap.Get(),
            0, 0, static_cast<uint32_t>(subresources.size()), subresources.data());
    }
    else
    {
        D3D12_SUBRESOURCE_DATA subresources = {};
        

        ThrowIfFailed(LoadWICTextureFromFile(
            md3dDevice.Get(),
            tex->Filename.c_str(),
            tex->Resource.ReleaseAndGetAddressOf(),
            texData,
            subresources));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex->Resource.Get(), 0, 1);

        // Create the GPU upload buffer.
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(tex->UploadHeap.GetAddressOf())));

        UpdateSubresources(mCommandList.Get(), tex->Resource.Get(), tex->UploadHeap.Get(),
            0, 0, 1, &subresources);
    }

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &barrier);

    mTextures[tex->Name] = std::move(tex);
}

void Graphics::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable[2];
    
    // skybox
    texTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

    // texture
    texTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, mTextures.size(), 2, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[7];

    // instance 
    slotRootParameter[0].InitAsShaderResourceView(0, 1);
    // constant buffer
    slotRootParameter[1].InitAsConstantBufferView(0);
    // skinned buffer
    slotRootParameter[2].InitAsConstantBufferView(1);
    // pass buffer
    slotRootParameter[3].InitAsConstantBufferView(2);
    // material
    slotRootParameter[4].InitAsShaderResourceView(1, 1);
    // sky box
    slotRootParameter[5].InitAsDescriptorTable(1, &texTable[0], D3D12_SHADER_VISIBILITY_PIXEL);
    // tex
    slotRootParameter[6].InitAsDescriptorTable(1, &texTable[1], D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();

    auto size = sizeof(slotRootParameter);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(7, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void Graphics::BuildDescriptorHeaps()
{
    //
    // Create the SRV heap.
    //
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = mTextures.size() + 3;// shadow map, cube, null tex
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    //
    // Fill out the heap with actual descriptors.
    //
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    std::vector<ComPtr<ID3D12Resource>> tex2DList =
    {
        

        mTextures["bricksDiffuseMap"]->Resource,
        mTextures["bricksNormalMap"]->Resource,
        mTextures["tileDiffuseMap"]->Resource,
        mTextures["tileNormalMap"]->Resource,
        mTextures["defaultDiffuseMap"]->Resource,
        mTextures["defaultNormalMap"]->Resource,
        mTextures["bricks"]->Resource,
        mTextures["stone"]->Resource,
        mTextures["grass"]->Resource,
        mTextures["ice"]->Resource,
    };

    mModelSrvHeapStart = (UINT)tex2DList.size();

    for (UINT i = 0; i < (UINT)mModelTextureNames.size(); ++i)
    {
        auto texResource = mTextures[mModelTextureNames[i]]->Resource;
        assert(texResource != nullptr);
        tex2DList.push_back(texResource);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    for (UINT i = 0; i < (UINT)tex2DList.size(); ++i)
    {
        srvDesc.Format = tex2DList[i]->GetDesc().Format;
        //srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
        srvDesc.Texture2D.MipLevels = 1;
        md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);

        // next descriptor
        hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
    }

    auto skyTex = mTextures["skyCubeMap"]->Resource;

    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = skyTex->GetDesc().MipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    srvDesc.Format = skyTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(skyTex.Get(), &srvDesc, hDescriptor);

    mSkyTexHeapIndex = (UINT)tex2DList.size();

    mShadowMapHeapIndex = mSkyTexHeapIndex + 1;
    mNullCubeSrvIndex = mShadowMapHeapIndex + 1;
    mNullTexSrvIndex = mNullCubeSrvIndex + 1;

    auto srvCpuStart = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    auto srvGpuStart = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

    auto nullSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mNullCubeSrvIndex, mCbvSrvUavDescriptorSize);
    mNullSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mNullCubeSrvIndex, mCbvSrvUavDescriptorSize);

    md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
    nullSrv.Offset(1, mCbvSrvUavDescriptorSize);

    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

    mShadowMap->BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
        CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
        CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, mDsvDescriptorSize));
}

void Graphics::BuildShadersAndInputLayout()
{
    const D3D_SHADER_MACRO defines[] =
    {
        "FOG", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "ALPHA_TEST", "1",
        NULL, NULL
    };


    const D3D_SHADER_MACRO skinnedDefines[] =
    {
        "SKINNED", "1",
        NULL, NULL
    };

    
    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");

    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");
    mShaders["skinnedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "PS", "ps_5_1");

    //mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
    //mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
    //mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mShaders["InstanceStandardVS"] = d3dUtil::CompileShader(L"Shaders\\InstanceDefault.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["InstanceOpaquePS"] = d3dUtil::CompileShader(L"Shaders\\InstanceDefault.hlsl", nullptr, "PS", "ps_5_1");

    mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

    mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shaders\\Shadow.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skinnedShadowVS"] = d3dUtil::CompileShader(L"Shaders\\Shadow.hlsl", skinnedDefines, "VS", "vs_5_1");


    mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"Shaders\\Shadow.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["shadowAlphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Shadow.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mShaders["debugVS"] = d3dUtil::CompileShader(L"Shaders\\ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["debugPS"] = d3dUtil::CompileShader(L"Shaders\\ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");


    mStdInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

    };

    mTreeSpriteInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    mSkinnedInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void Graphics::BuildTreeSpritesGeometry()
{
    struct TreeSpriteVertex
    {
        XMFLOAT3 Pos;
        XMFLOAT2 Size;
    };

    static const int treeCount = 16;
    std::array<TreeSpriteVertex, 16> vertices;
    for (UINT i = 0; i < treeCount; ++i)
    {
        float x = MathHelper::RandF(-45.0f, 45.0f);
        float z = MathHelper::RandF(-45.0f, 45.0f);
        float y = GetHillsHeight(x, z);

        // Move tree slightly above land height.
        y += 8.0f;

        vertices[i].Pos = XMFLOAT3(x, y, z);
        vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
    }

    std::array<std::uint16_t, 16> indices =
    {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "treeSpritesGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(TreeSpriteVertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["points"] = submesh;

    mGeometries["treeSpritesGeo"] = std::move(geo);
}

void Graphics::BuildLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(260.0f, 260.0f, 100, 100);

    //
    // Extract the vertex elements we are interested and apply the height function to
    // each vertex.  In addition, color the vertices based on their height so we have
    // sandy looking beaches, grassy low hills, and snow mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for (size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.x, p.z);
        vertices[i].TexC = grid.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "landGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries["landGeo"] = std::move(geo);
}

void Graphics::BuildWavesGeometry()
{
    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
    assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for (int i = 0; i < m - 1; ++i)
    {
        for (int j = 0; j < n - 1; ++j)
        {
            indices[k] = i * n + j;
            indices[k + 1] = i * n + j + 1;
            indices[k + 2] = (i + 1) * n + j;

            indices[k + 3] = (i + 1) * n + j;
            indices[k + 4] = i * n + j + 1;
            indices[k + 5] = (i + 1) * n + j + 1;

            k += 6; // next quad
        }
    }

    UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
    UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "waterGeo";

    // Set dynamically.
    geo->VertexBufferCPU = nullptr;
    geo->VertexBufferGPU = nullptr;

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries["waterGeo"] = std::move(geo);
}

void Graphics::BuildBoxGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);

    std::vector<Vertex> vertices(box.Vertices.size());
    for (size_t i = 0; i < box.Vertices.size(); ++i)
    {
        auto& p = box.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Normal = box.Vertices[i].Normal;
        vertices[i].TexC = box.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = box.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["box"] = submesh;

    mGeometries["boxGeo"] = std::move(geo);
}

void Graphics::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
    GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

    //
    // We are concatenating all the geometry into one big vertex/index buffer.  So
    // define the regions in the buffer each submesh covers.
    //

    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    UINT boxVertexOffset = 0;
    UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
    UINT quadVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();

    // Cache the starting index for each object in the concatenated index buffer.
    UINT boxIndexOffset = 0;
    UINT gridIndexOffset = (UINT)box.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
    UINT quadIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    SubmeshGeometry quadSubmesh;
    quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
    quadSubmesh.StartIndexLocation = quadIndexOffset;
    quadSubmesh.BaseVertexLocation = quadVertexOffset;

    //
    // Extract the vertex elements we are interested in and pack the
    // vertices of all the meshes into one vertex buffer.
    //

    auto totalVertexCount =
        box.Vertices.size() +
        grid.Vertices.size() +
        sphere.Vertices.size() +
        cylinder.Vertices.size() +
        quad.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].TexC = box.Vertices[i].TexC;
        vertices[k].TangentU = box.Vertices[i].TangentU;
    }

    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grid.Vertices[i].Position;
        vertices[k].Normal = grid.Vertices[i].Normal;
        vertices[k].TexC = grid.Vertices[i].TexC;
        vertices[k].TangentU = grid.Vertices[i].TangentU;
    }

    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Normal = sphere.Vertices[i].Normal;
        vertices[k].TexC = sphere.Vertices[i].TexC;
        vertices[k].TangentU = sphere.Vertices[i].TangentU;
    }

    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Normal = cylinder.Vertices[i].Normal;
        vertices[k].TexC = cylinder.Vertices[i].TexC;
        vertices[k].TangentU = cylinder.Vertices[i].TangentU;
    }

    for (int i = 0; i < quad.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = quad.Vertices[i].Position;
        vertices[k].Normal = quad.Vertices[i].Normal;
        vertices[k].TexC = quad.Vertices[i].TexC;
        vertices[k].TangentU = quad.Vertices[i].TangentU;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
    indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["grid"] = gridSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;
    geo->DrawArgs["quad"] = quadSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void Graphics::BuildSkullGeometry()
{
    std::ifstream fin("../Models/skull.txt");

    if (!fin)
    {
        MessageBoxW(0,L"Models/skull.txt not found.", 0, 0);
        return;
    }

    UINT vcount = 0;
    UINT tcount = 0;
    std::string ignore;

    fin >> ignore >> vcount;
    fin >> ignore >> tcount;
    fin >> ignore >> ignore >> ignore >> ignore;

    XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
    XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

    XMVECTOR vMin = XMLoadFloat3(&vMinf3);
    XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    std::vector<Vertex> vertices(vcount);
    for (UINT i = 0; i < vcount; ++i)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

        XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

        // Project point onto unit sphere and generate spherical texture coordinates.
        XMFLOAT3 spherePos;
        XMStoreFloat3(&spherePos, XMVector3Normalize(P));

        float theta = atan2f(spherePos.z, spherePos.x);

        // Put in [0, 2pi].
        if (theta < 0.0f)
            theta += XM_2PI;

        float phi = acosf(spherePos.y);

        float u = theta / (2.0f * XM_PI);
        float v = phi / XM_PI;

        vertices[i].TexC = { u, v };

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }

    BoundingBox bounds;
    XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;

    std::vector<std::int32_t> indices(3 * tcount);
    for (UINT i = 0; i < tcount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }

    fin.close();

    //
    // Pack the indices of all the meshes into one index buffer.
    //

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skullGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    submesh.Bounds = bounds;

    geo->DrawArgs["skull"] = submesh;

    mGeometries[geo->Name] = std::move(geo);
}

void Graphics::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    //
    // PSO for opaque objects.
    //
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mStdInputLayout.data(), (UINT)mStdInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = 1;
    opaquePsoDesc.SampleDesc.Quality = 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

    //
    // PSO for skinned pass.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
    skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedOpaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
        mShaders["skinnedVS"]->GetBufferSize()
    };
    skinnedOpaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedPS"]->GetBufferPointer()),
        mShaders["skinnedPS"]->GetBufferSize()
    };

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));

    //
    // PSO for shadow map pass.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePsoDesc;
    smapPsoDesc.RasterizerState.DepthBias = 100000;
    smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    smapPsoDesc.pRootSignature = mRootSignature.Get();
    smapPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
        mShaders["shadowVS"]->GetBufferSize()
    };
    smapPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
        mShaders["shadowOpaquePS"]->GetBufferSize()
    };

    // Shadow map pass does not have a render target.
    smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    smapPsoDesc.NumRenderTargets = 0;   
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedSmapPsoDesc = smapPsoDesc;

    skinnedSmapPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedSmapPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedShadowVS"]->GetBufferPointer()),
        mShaders["skinnedShadowVS"]->GetBufferSize()
    };
    skinnedSmapPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
        mShaders["shadowOpaquePS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedSmapPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedShadow_opaque"])));

    //
    // PSO for debug layer.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;
    debugPsoDesc.pRootSignature = mRootSignature.Get();
    debugPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
        mShaders["debugVS"]->GetBufferSize()
    };
    debugPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
        mShaders["debugPS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

    // PSO for Instance objects
    D3D12_GRAPHICS_PIPELINE_STATE_DESC InstanceOpaquePsoDesc = opaquePsoDesc;
    InstanceOpaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["InstanceStandardVS"]->GetBufferPointer()),
        mShaders["InstanceStandardVS"]->GetBufferSize()
    };
    InstanceOpaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["InstanceOpaquePS"]->GetBufferPointer()),
        mShaders["InstanceOpaquePS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&InstanceOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["instance"])));

    //
    // PSO for sky.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

    // The camera is inside the sky sphere, so just turn off culling.
    skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // Make sure the depth function is LESS_EQUAL and not just LESS.  
    // Otherwise, the normalized depth values at z = 1 (NDC) will 
    // fail the depth test if the depth buffer was cleared to 1.
    skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    skyPsoDesc.pRootSignature = mRootSignature.Get();
    skyPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
        mShaders["skyVS"]->GetBufferSize()
    };
    skyPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
        mShaders["skyPS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

}

void Graphics::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back
        (
            std::make_unique<FrameResource>
            (
                md3dDevice.Get(), 2, (UINT)mAllRitems.size(), mInstanceCount, 
                (UINT)mMaterials.size(), mRitemLayer[(int)RenderLayer::SkinnedOpaque].size()
            )
        );
    }
}

void Graphics::BuildMaterials()
{
    UINT matCBIndex = 0;

    auto bricks0 = std::make_unique<Material>();
    bricks0->Name = "bricks0";
    bricks0->MatCBIndex = matCBIndex++;
    bricks0->DiffuseSrvHeapIndex = 0;
    bricks0->NormalSrvHeapIndex = 1;
    bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bricks0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    bricks0->Roughness = 0.3f;

    auto tile0 = std::make_unique<Material>();
    tile0->Name = "tile0";
    tile0->MatCBIndex = matCBIndex++;
    tile0->DiffuseSrvHeapIndex = 2;
    tile0->NormalSrvHeapIndex = 3;
    tile0->DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
    tile0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
    tile0->Roughness = 0.1f;

    auto mirror0 = std::make_unique<Material>();
    mirror0->Name = "mirror0";
    mirror0->MatCBIndex = matCBIndex++;
    mirror0->DiffuseSrvHeapIndex = 4;
    mirror0->NormalSrvHeapIndex = 5;
    mirror0->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mirror0->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
    mirror0->Roughness = 0.1f;

    auto bricks1 = std::make_unique<Material>();
    bricks1->Name = "bricks1";
    bricks1->MatCBIndex = matCBIndex++;
    bricks1->DiffuseSrvHeapIndex = 6;
    bricks1->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bricks1->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    bricks1->Roughness = 0.1f;

    auto stone0 = std::make_unique<Material>();
    stone0->Name = "stone0";
    stone0->MatCBIndex = matCBIndex++;
    stone0->DiffuseSrvHeapIndex = 7;
    stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;

    auto grass0 = std::make_unique<Material>();
    grass0->Name = "grass0";
    grass0->MatCBIndex = matCBIndex++;
    grass0->DiffuseSrvHeapIndex = 8;
    grass0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    grass0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    grass0->Roughness = 0.2f;

    auto ice0 = std::make_unique<Material>();
    ice0->Name = "ice0";
    ice0->MatCBIndex = matCBIndex++;
    ice0->DiffuseSrvHeapIndex = 9;
    ice0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    ice0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    ice0->Roughness = 0.0f;

    mMaterials["bricks0"] = std::move(bricks0);
    mMaterials["tile0"] = std::move(tile0);
    mMaterials["mirror0"] = std::move(mirror0);
    mMaterials["bricks1"] = std::move(bricks1);
    mMaterials["stone0"] = std::move(stone0);
    mMaterials["grass0"] = std::move(grass0);
    mMaterials["ice0"] = std::move(ice0);


    UINT srvHeapIndex = mModelSrvHeapStart;

    for (auto& mesh : meshes)
    {
        for (auto& subset : mesh.subsets)
        {
            if (mMaterials.find(subset.material.Name) != mMaterials.end()) continue;

            auto mat = std::make_unique<Material>();
            mat->Name = subset.material.Name;
            mat->MatCBIndex = matCBIndex++;
            mat->DiffuseSrvHeapIndex = srvHeapIndex++;
            mat->NormalSrvHeapIndex = srvHeapIndex++;
            mat->SpecularSrvHeapIndex = srvHeapIndex++;
            mat->DiffuseAlbedo = subset.material.DiffuseAlbedo;
            mat->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
            mat->Roughness = 1.0f;

            mMaterials[mat->Name] = std::move(mat);

        }
    }


    auto sky = std::make_unique<Material>();
    sky->Name = "sky";
    sky->MatCBIndex = matCBIndex++;
    sky->DiffuseSrvHeapIndex = mSkyTexHeapIndex;
    sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    sky->Roughness = 1.0f;

    mMaterials["sky"] = std::move(sky);
}

void Graphics::BuildRenderItems()
{
    UINT objCBIndex = 0;

    auto skyRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = objCBIndex++;
    skyRitem->Mat = mMaterials["sky"].get();
    skyRitem->Geo = mGeometries["shapeGeo"].get();
    skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
    skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
    skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

    mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
    mAllRitems.push_back(std::move(skyRitem));

    auto quadRitem = std::make_unique<RenderItem>();
    quadRitem->World = MathHelper::Identity4x4();
    quadRitem->TexTransform = MathHelper::Identity4x4();
    quadRitem->ObjCBIndex = objCBIndex++;
    quadRitem->Mat = mMaterials["bricks0"].get();
    quadRitem->Geo = mGeometries["shapeGeo"].get();
    quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
    quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    mRitemLayer[(int)RenderLayer::Debug].push_back(quadRitem.get());
    mAllRitems.push_back(std::move(quadRitem));

    auto boxRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 1.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
    XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 0.5f, 1.0f));
    boxRitem->ObjCBIndex = objCBIndex++;
    boxRitem->Mat = mMaterials["bricks0"].get();
    boxRitem->Geo = mGeometries["shapeGeo"].get();
    boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

    mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
    mAllRitems.push_back(std::move(boxRitem));

    auto globeRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&globeRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 2.0f, 0.0f));
    XMStoreFloat4x4(&globeRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
    globeRitem->ObjCBIndex = objCBIndex++;
    globeRitem->Mat = mMaterials["mirror0"].get();
    globeRitem->Geo = mGeometries["shapeGeo"].get();
    globeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    globeRitem->IndexCount = globeRitem->Geo->DrawArgs["sphere"].IndexCount;
    globeRitem->StartIndexLocation = globeRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
    globeRitem->BaseVertexLocation = globeRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

    mRitemLayer[(int)RenderLayer::Opaque].push_back(globeRitem.get());
    mAllRitems.push_back(std::move(globeRitem));

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
    gridRitem->ObjCBIndex = objCBIndex++;
    gridRitem->Mat = mMaterials["tile0"].get();
    gridRitem->Geo = mGeometries["shapeGeo"].get();
    gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
    mAllRitems.push_back(std::move(gridRitem));

    XMMATRIX brickTexTransform = XMMatrixScaling(1.5f, 2.0f, 1.0f);
    
    for (int i = 0; i < 5; ++i)
    {
        auto leftCylRitem = std::make_unique<RenderItem>();
        auto rightCylRitem = std::make_unique<RenderItem>();
        auto leftSphereRitem = std::make_unique<RenderItem>();
        auto rightSphereRitem = std::make_unique<RenderItem>();

        XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
        XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

        XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
        XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

        XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
        XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
        leftCylRitem->ObjCBIndex = objCBIndex++;
        leftCylRitem->Mat = mMaterials["bricks0"].get();
        leftCylRitem->Geo = mGeometries["shapeGeo"].get();
        leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
        leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

        XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
        XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
        rightCylRitem->ObjCBIndex = objCBIndex++;
        rightCylRitem->Mat = mMaterials["bricks0"].get();
        rightCylRitem->Geo = mGeometries["shapeGeo"].get();
        rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
        rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

        XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
        leftSphereRitem->TexTransform = MathHelper::Identity4x4();
        leftSphereRitem->ObjCBIndex = objCBIndex++;
        leftSphereRitem->Mat = mMaterials["mirror0"].get();
        leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
        leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
        leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
        leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

        XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
        rightSphereRitem->TexTransform = MathHelper::Identity4x4();
        rightSphereRitem->ObjCBIndex = objCBIndex++;
        rightSphereRitem->Mat = mMaterials["mirror0"].get();
        rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
        rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
        rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
        rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

        mRitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
        mRitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
        mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
        mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

        mAllRitems.push_back(std::move(leftCylRitem));
        mAllRitems.push_back(std::move(rightCylRitem));
        mAllRitems.push_back(std::move(leftSphereRitem));
        mAllRitems.push_back(std::move(rightSphereRitem));
    }
}

void Graphics::BuildSkinnedRenderItems()
{
    int objCBIndex = mAllRitems.size();
    int skinnedIndex = 0;

    for (auto& mesh : meshes)
    {
        int subsetIndex = 0;
        for (auto& subset : mesh.subsets)
        {
            auto model = std::make_unique<RenderItem>();

            Matrix modelScale = XMMatrixScaling(0.01f, 0.01f, 0.01f);
            Matrix modelRot = XMMatrixRotationX(-XM_PIDIV2);
            Matrix modelOffset = XMMatrixTranslation(0.0f, 0.0f, -10.0f);

            model->World = modelScale * modelOffset;

            model->ObjCBIndex = objCBIndex++;
            if (subset.material.Name != "")
            {
                model->Mat = mMaterials[subset.material.Name].get();
            }
            else
            {
                model->Mat = mMaterials["mirror0"].get();
            }
 
            model->Geo = mGeometries[mesh.name].get();
            model->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

            model->IndexCount = model->Geo->DrawArgs[subset.name].IndexCount;
            model->StartIndexLocation = model->Geo->DrawArgs[subset.name].StartIndexLocation;
            model->BaseVertexLocation = 0;

            // All render items for this solider.m3d instance share
            // the same skinned model instance.
            model->SkinnedCBIndex = 0;
            model->SkinnedFlag = true;

            mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(model.get());
            mAllRitems.push_back(std::move(model));
        }
    }
}

void Graphics::BuildInstanceRenderItems()
{
    auto skullRitem = std::make_unique<RenderItem>();
    skullRitem->World = MathHelper::Identity4x4();
    skullRitem->TexTransform = MathHelper::Identity4x4();
    skullRitem->ObjCBIndex = 0;
    skullRitem->Mat = mMaterials["ice0"].get();
    skullRitem->Geo = mGeometries["skullGeo"].get();
    skullRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullRitem->InstanceCount = 0;
    skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
    skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
    skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
    skullRitem->Bounds = skullRitem->Geo->DrawArgs["skull"].Bounds;

    // Generate instance data.
    const int n = 5;
    mInstanceCount = n * n * n;
    skullRitem->Instances.resize(mInstanceCount);

    float width = 200.0f;
    float height = 200.0f;
    float depth = 200.0f;

    float x = -0.5f * width;
    float y = -0.5f * height;
    float z = -0.5f * depth;
    float dx = width / (n - 1);
    float dy = height / (n - 1);
    float dz = depth / (n - 1);
    for (int k = 0; k < n; ++k)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                int index = k * n * n + i * n + j;
                // Position instanced along a 3D grid.
                skullRitem->Instances[index].World = XMFLOAT4X4(
                    1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    x + j * dx, y + i * dy, z + k * dz, 1.0f);

                XMStoreFloat4x4(&skullRitem->Instances[index].TexTransform, XMMatrixScaling(2.0f, 2.0f, 1.0f));
                //skullRitem->Instances[index].MaterialIndex = index % (mMaterials.size() - mModelMaterials.size());
                skullRitem->Instances[index].MaterialIndex = (rand() % 4) + 3;

            }
        }
    }
    mRitemLayer[(int)RenderLayer::InstanceOpaque].push_back(skullRitem.get());
    mAllInstanceRitems.push_back(std::move(skullRitem));

}

void Graphics::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));


    auto objectCB = mCurrFrameResource->ObjectCB->Resource();
    auto skinnedCB = mCurrFrameResource->SkinnedCB->Resource();


    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];
        auto vertexBufferView = ri->Geo->VertexBufferView();
        auto indexBufferView = ri->Geo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
        cmdList->IASetIndexBuffer(&indexBufferView);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;

        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);

        if (ri->SkinnedFlag)
        {
            D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->SkinnedCBIndex * skinnedCBByteSize;
            cmdList->SetGraphicsRootConstantBufferView(2, skinnedCBAddress);
        }
        else
        {
            cmdList->SetGraphicsRootConstantBufferView(2, 0);
        }

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

}

void Graphics::DrawInstanceRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        auto vertexBufferView = ri->Geo->VertexBufferView();
        auto indexBufferView = ri->Geo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
        cmdList->IASetIndexBuffer(&indexBufferView);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        // Set the instance buffer to use for this render-item.  For structured buffers, we can bypass 
        // the heap and set as a root descriptor.
        auto instanceBuffer = mCurrFrameResource->InstanceBuffer->Resource();
        cmdList->SetGraphicsRootShaderResourceView(0, instanceBuffer->GetGPUVirtualAddress());

        cmdList->DrawIndexedInstanced(ri->IndexCount, ri->InstanceCount, ri->StartIndexLocation, ri->BaseVertexLocation, 0);

    }
}

void Graphics::DrawSceneToShadowMap()
{
    auto viewport = mShadowMap->Viewport();
    auto scissorRect = mShadowMap->ScissorRect();
    mCommandList->RSSetViewports(1, &viewport);
    mCommandList->RSSetScissorRects(1, &scissorRect);

    // Change to DEPTH_WRITE.
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &barrier);

    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Set null render target because we are only going to draw to
    // depth buffer.  Setting a null render target will disable color writes.
    // Note the active PSO also must specify a render target count of 0.
    auto shadowMapDsv = mShadowMap->Dsv();
    mCommandList->OMSetRenderTargets(0, nullptr, false, &shadowMapDsv);

    // Bind the pass constant buffer for the shadow map pass.
    auto passCB = mCurrFrameResource->PassCB->Resource();
    D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
    mCommandList->SetGraphicsRootConstantBufferView(3, passCBAddress);

    mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    mCommandList->SetPipelineState(mPSOs["skinnedShadow_opaque"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

    // Change back to GENERIC_READ so we can read the texture in a shader.
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
    mCommandList->ResourceBarrier(1, &barrier);
}

void Graphics::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
    // set as a root descriptor.
    auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
    mCommandList->SetGraphicsRootShaderResourceView(4, matBuffer->GetGPUVirtualAddress());

    // Bind null SRV for shadow map pass.
    mCommandList->SetGraphicsRootDescriptorTable(5, mNullSrv);

    // Bind all the textures used in this scene.  Observe
    // that we only have to specify the first descriptor in the table.  
    // The root signature knows how many descriptors are expected in the table.
    mCommandList->SetGraphicsRootDescriptorTable(6, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    DrawSceneToShadowMap();

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    auto currentBackBufferView = CurrentBackBufferView();
    auto depthStencilView = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);

    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(3, passCB->GetGPUVirtualAddress());

    // Bind the sky cube map.  For our demos, we just use one "world" cube map representing the environment
    // from far away, so all objects will use the same cube map and we only need to set it once per-frame.  
    // If we wanted to use "local" cube maps, we would have to change them per-object, or dynamically
    // index into an array of cube maps.
    CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    skyTexDescriptor.Offset(mSkyTexHeapIndex, mCbvSrvUavDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(5, skyTexDescriptor);

    if (object)
    {
        mCommandList->SetPipelineState(mPSOs["opaque"].Get());
        DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
    }


    if (model)
    {
        mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
        DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);
    }

    if (debugFlag)
    {
        mCommandList->SetPipelineState(mPSOs["debug"].Get());
        DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Debug]);
    }

    if (skull)
    {
        mCommandList->SetPipelineState(mPSOs["instance"].Get());
        DrawInstanceRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::InstanceOpaque]);
    }
    mCommandList->SetPipelineState(mPSOs["sky"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

    // Indicate a state transition on the resource usage.
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier);

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % mSwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> Graphics::GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC shadow(
        6, // shaderRegister
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
        0.0f,                               // mipLODBias
        16,                                 // maxAnisotropy
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

    return 
    {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp,
        shadow
    };
}

float Graphics::GetHillsHeight(float x, float z)const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 Graphics::GetHillsNormal(float x, float z)const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(
        -0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
        1.0f,
        -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}

void Graphics::Fetch_bone_influences(const FbxMesh* fbx_mesh, std::vector<bone_influences_per_control_point>& influences)
{
    const int number_of_control_points = fbx_mesh->GetControlPointsCount();
    influences.resize(number_of_control_points);
    const int number_of_deformers = fbx_mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int index_of_deformer = 0; index_of_deformer < number_of_deformers; ++index_of_deformer)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(fbx_mesh->GetDeformer(index_of_deformer, FbxDeformer::eSkin));
        const int number_of_clusters = skin->GetClusterCount();

        for (int index_of_cluster = 0; index_of_cluster < number_of_clusters; ++index_of_cluster)
        {
            FbxCluster* cluster = skin->GetCluster(index_of_cluster);
            const int number_of_control_point_indices = cluster->GetControlPointIndicesCount();
            const int* array_of_control_point_indices = cluster->GetControlPointIndices();
            const double* array_of_control_point_weights = cluster->GetControlPointWeights();

            for (int i = 0; i < number_of_control_point_indices; ++i)
            {
                bone_influences_per_control_point& influences_per_control_point
                    = influences.at(array_of_control_point_indices[i]);
                bone_influence influence;

                influence.index = index_of_cluster;
                influence.weight = static_cast<float>(array_of_control_point_weights[i]);
                influences_per_control_point.push_back(influence);
            }
        }
    }
}

void Graphics::Fetch_bone_matrices(const FbxMesh* fbx_mesh)
{
    const int number_of_deformers = fbx_mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int index_of_deformer = 0; index_of_deformer < number_of_deformers; ++index_of_deformer)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(fbx_mesh->GetDeformer(index_of_deformer, FbxDeformer::eSkin));

        const int number_of_cluster = skin->GetClusterCount();

        for (int index_of_cluster = 0; index_of_cluster < number_of_cluster; ++index_of_cluster)
        {

            FbxCluster* cluster = skin->GetCluster(index_of_cluster);

            // this matrix trnasforms coordinates of the initial pose from mesh space to global space
            FbxAMatrix reference_global_init_position;
            cluster->GetTransformMatrix(reference_global_init_position);
            mReference_global_init_position[index_of_cluster] = reference_global_init_position;

            // this matrix trnasforms coordinates of the initial pose from bone_node space to global space
            FbxAMatrix cluster_global_init_position;
            cluster->GetTransformLinkMatrix(cluster_global_init_position);
            mCluster_global_init_position[index_of_cluster] = cluster_global_init_position;

        }

    }
}

void Graphics::LoadFBX(const std::wstring filename)
{
    std::string _filename = WstringToString(filename);

    FbxManager* manager = FbxManager::Create();

    manager->SetIOSettings(FbxIOSettings::Create(manager, IOSROOT));

    FbxImporter* importer = FbxImporter::Create(manager, "");

    bool importStatus = false;

    importStatus = importer->Initialize(_filename.c_str(), -1, manager->GetIOSettings());
    _ASSERT_EXPR(importStatus, importer->GetStatus().GetErrorString());

    FbxScene* scene = FbxScene::Create(manager, "");

    importStatus = importer->Import(scene);
    _ASSERT_EXPR(importStatus, importer->GetStatus().GetErrorString());

    fbxsdk::FbxGeometryConverter gemoetryConverter(manager);
    gemoetryConverter.Triangulate(scene, /*replace*/true);

    std::vector <FbxNode*> fetchedMeshes;
    std::vector <FbxNode*> boneNodes;
    std::function<void(FbxNode*)> traverse = [&](FbxNode* node)
    {
        if (node)
        {
            FbxNodeAttribute* fbx_node_attribute = node->GetNodeAttribute();
            
            if (fbx_node_attribute)
            {
                switch (fbx_node_attribute->GetAttributeType())
                {
                case FbxNodeAttribute::eMesh:
                    fetchedMeshes.push_back(node);
                    break;
                case FbxNodeAttribute::eSkeleton:
                    boneNodes.push_back(node);

                    break;
                }
            }
          
            for (int i = 0; i < node->GetChildCount(); i++)
            {
                traverse(node->GetChild(i));
            }
        }
      
    };
    traverse(scene->GetRootNode());

    if (fetchedMeshes.size() > 0)
    {
        meshes.resize(fetchedMeshes.size());
        for (int i = 0; i < fetchedMeshes.size(); i++)
        {
            FbxMesh* fbxMesh = fetchedMeshes.at(i)->GetMesh();
            Mesh& mesh = meshes.at(i);
            const int numberOfMaterials = fbxMesh->GetNode()->GetMaterialCount();
            mesh.subsets.resize(numberOfMaterials);//UNIT18

            mesh.name = fbxMesh->GetName();

            //load bone_node influence per mesh
            std::vector<bone_influences_per_control_point> bone_influences;
            Fetch_bone_influences(fbxMesh, bone_influences);

            FbxTime::EMode time_mode = fbxMesh->GetScene()->GetGlobalSettings().GetTimeMode();
            FbxTime frame_time;
            frame_time.SetTime(0, 0, 0, 1, 0, time_mode);
            
            Fetch_bone_matrices(fbxMesh);

            FbxAMatrix global_transform = fbxMesh->GetNode()->EvaluateGlobalTransform(0);

            for (int row = 0; row < 4; row++)
            {
                for (int column = 0; column < 4; column++)
                {
                    mesh.global_transform.m[row][column] = static_cast<float>(global_transform[row][column]);
                }
            }

            for (int index_of_material = 0; index_of_material < numberOfMaterials; index_of_material++)
            {
                const FbxSurfaceMaterial* surface_material = fbxMesh->GetNode()->GetMaterial(index_of_material);
                //load diffuseMap
                const FbxProperty diffuseMap = surface_material->FindProperty(FbxSurfaceMaterial::sDiffuse);
                const FbxProperty factor = surface_material->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
                Subset& subset = mesh.subsets.at(index_of_material);

                subset.material.Name = surface_material->GetName();

                if (diffuseMap.IsValid() && factor.IsValid())
                {
                    FbxDouble3 color = diffuseMap.Get<FbxDouble3>();
                    double f = factor.Get<FbxDouble>();
                    subset.material.DiffuseAlbedo.x = static_cast<float>(color[0] * f);
                    subset.material.DiffuseAlbedo.y = static_cast<float>(color[1] * f);
                    subset.material.DiffuseAlbedo.z = static_cast<float>(color[2] * f);
                    subset.material.DiffuseAlbedo.w = 1.0f;
                }

                //load normalMap
                if (diffuseMap.IsValid())
                {
                    const int number_of_textures = diffuseMap.GetSrcObjectCount<FbxFileTexture>();
                    if (number_of_textures)
                    {
                        const FbxFileTexture* file_texture = diffuseMap.GetSrcObject<FbxFileTexture>();
                        if (file_texture)
                        {
                            subset.material.DiffuseMapName = file_texture->GetRelativeFileName();
                        }
                    }
                }
                const FbxProperty normalMap = surface_material->FindProperty(FbxSurfaceMaterial::sNormalMap);
                if (normalMap.IsValid())
                {
                    const int number_of_textures = normalMap.GetSrcObjectCount<FbxFileTexture>();
                    if (number_of_textures)
                    {
                        const FbxFileTexture* file_texture = normalMap.GetSrcObject<FbxFileTexture>();
                        if (file_texture)
                        {
                            subset.material.NormalMapName = file_texture->GetRelativeFileName();
                        }
                    }
                }
                const FbxProperty specularMap = surface_material->FindProperty(FbxSurfaceMaterial::sSpecular);
                if (specularMap.IsValid())
                {
                    const int number_of_textures = specularMap.GetSrcObjectCount<FbxFileTexture>();
                    if (number_of_textures)
                    {
                        const FbxFileTexture* file_texture = specularMap.GetSrcObject<FbxFileTexture>();
                        if (file_texture)
                        {
                            subset.material.SpecularName = file_texture->GetRelativeFileName();
                        }
                    }
                }
            }
            // Count the polygon count of each material
            if (numberOfMaterials > 0)
            {
                // Count the faces of each material
                const int number_of_polygons = fbxMesh->GetPolygonCount();
                for (int index_of_polygon = 0; index_of_polygon < number_of_polygons; ++index_of_polygon)
                {
                    const u_int material_index = fbxMesh->GetElementMaterial()->GetIndexArray().GetAt(index_of_polygon);
                    mesh.subsets.at(material_index).index_count += 3;
                }
                // Record the offset (how many vertex)
                int offset = 0;
                for (Subset& subset : mesh.subsets)
                {
                    subset.index_start = offset;
                    offset += subset.index_count;
                    // This will be used as counter in the following procedures, reset to zero
                    subset.index_count = 0;
                }
            }

            std::vector<SkinnedVertex> vertices;
            std::vector<uint16_t> indices;
            u_int vertex_count = 0;

            //Tangent
            FbxGeometryElementTangent* element = fbxMesh->CreateElementTangent();
            //  保存形式の取得
            FbxGeometryElement::EMappingMode mapmode = element->GetMappingMode();
            FbxGeometryElement::EReferenceMode refmode = element->GetReferenceMode();

            const FbxVector4* array_of_control_points = fbxMesh->GetControlPoints();
            const int number_of_polygons = fbxMesh->GetPolygonCount();
            indices.resize(number_of_polygons * 3);
            for (int index_of_polygon = 0; index_of_polygon < number_of_polygons; index_of_polygon++)
            {

                // The material for current face.
                int index_of_material = 0;
                if (numberOfMaterials > 0)
                {
                    index_of_material = fbxMesh->GetElementMaterial()->GetIndexArray().GetAt(index_of_polygon);
                }
                // Where should I save the vertex attribute index, according to the material
                Subset& subset = mesh.subsets.at(index_of_material);
                const int index_offset = subset.index_start + subset.index_count;

                for (int index_of_vertex = 0; index_of_vertex < 3; index_of_vertex++)
                {
                    SkinnedVertex vertex;
                    const int index_of_control_point = fbxMesh->GetPolygonVertex(index_of_polygon, index_of_vertex);
                    vertex.Pos.x = static_cast<float>(array_of_control_points[index_of_control_point][0]);
                    vertex.Pos.y = static_cast<float>(array_of_control_points[index_of_control_point][1]);
                    vertex.Pos.z = static_cast<float>(array_of_control_points[index_of_control_point][2]);

                    FbxVector4 normal;
                    fbxMesh->GetPolygonVertexNormal(index_of_polygon, index_of_vertex, normal);
                    vertex.Normal.x = static_cast<float>(normal[0]);
                    vertex.Normal.y = static_cast<float>(normal[1]);
                    vertex.Normal.z = static_cast<float>(normal[2]);

                    fbxsdk::FbxStringList uv_names;
                    fbxMesh->GetUVSetNames(uv_names);

                    if (uv_names.GetCount() > 0)
                    {
                        FbxVector2 uv;
                        bool unmapped_uv;
                        fbxMesh->GetPolygonVertexUV(index_of_polygon, index_of_vertex, uv_names[0], uv, unmapped_uv);
                        vertex.TexC.x = static_cast<float>(uv[0]);
                        vertex.TexC.y = 1.0f - static_cast<float>(uv[1]);
                    }
                    //    ポリゴン頂点に対するインデックス参照形式のみ対応
                    if (mapmode == FbxGeometryElement::eByPolygonVertex)
                    {
                        if (refmode == FbxGeometryElement::eIndexToDirect)
                        {
                            FbxLayerElementArrayTemplate<int>* index = &element->GetIndexArray();
                            // FbxColor取得
                            FbxVector4 v = element->GetDirectArray().GetAt(index->GetAt(index_of_control_point));
                            // DWORD型のカラー作成        
                            vertex.TangentU.x = (float)v[0];
                            vertex.TangentU.y = (float)v[1];
                            vertex.TangentU.z = (float)v[2];
                        }
                    }
                    else
                    {
                        vertex.TangentU.x = 0;
                        vertex.TangentU.y = 0;
                        vertex.TangentU.z = 0;
                    }

                    bone_influences_per_control_point influences_per_control_point = bone_influences.at(index_of_control_point);

                    for (size_t bone_index = 0; bone_index < influences_per_control_point.size(); ++bone_index)
                    {

                        if (bone_index < MAX_BONE_INFLUENCES)
                        {
                            vertex.BoneIndices[bone_index] = influences_per_control_point.at(bone_index).index;
                            vertex.BoneWeights[bone_index] = influences_per_control_point.at(bone_index).weight;
                        }
                    }

                    vertices.push_back(vertex);

                    indices.at(index_offset + index_of_vertex) = static_cast<uint16_t>(vertex_count);


                    vertex_count += 1;
                }
                subset.index_count += 3;
            }

            auto geo = std::make_unique<MeshGeometry>();
            geo->Name = mesh.name;

            int submeshIndex = 0;
            for (auto& subset : mesh.subsets)
            {
                subset.name = mesh.name + std::to_string(submeshIndex);

                SubmeshGeometry submesh;
                submesh.IndexCount = subset.index_count;
                submesh.StartIndexLocation = subset.index_start;
                submesh.BaseVertexLocation = 0;
                submesh.Bounds = BoundingBox();

                geo->DrawArgs[subset.name] = submesh;
                submeshIndex++;

            }

            const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
            const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

            ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
            CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

            ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
            CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

            geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
                mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

            geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
                mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

            geo->VertexByteStride = sizeof(SkinnedVertex);
            geo->VertexBufferByteSize = vbByteSize;
            geo->IndexFormat = DXGI_FORMAT_R16_UINT;
            geo->IndexBufferByteSize = ibByteSize;

            mGeometries[geo->Name] = std::move(geo);

        }
    }
    if (boneNodes.size() > 0)
    {
        scene->SetName(_filename.c_str());
        Fetch_bone_animations(boneNodes, extra_animations);
    }
    manager->Destroy();
    
}

void Graphics::Fetch_bone_animations(std::vector<FbxNode*> bone_nodes, std::map<std::string, Skeletal_animation>& skeletal_animations, u_int sampling_rate)
{
    // Get the list of all the animation stack.
    FbxArray<FbxString*> array_of_animation_stack_names;
    bone_nodes[0]->GetScene()->FillAnimStackNameArray(array_of_animation_stack_names);

    // Get the number of animations.
    int number_of_animations = array_of_animation_stack_names.Size();
    if (number_of_animations > 0)
    {
        for (size_t index_of_animation = 0; index_of_animation < number_of_animations; ++index_of_animation)
        {
            Skeletal_animation skeletal_animation;
            // Get the FbxTime per animation's frame.
            FbxTime::EMode time_mode = bone_nodes[0]->GetScene()->GetGlobalSettings().GetTimeMode();
            FbxTime frame_time;
            frame_time.SetTime(0, 0, 0, 1, 0, time_mode);
            sampling_rate = sampling_rate > 0 ? sampling_rate : frame_time.GetFrameRate(time_mode);
            float sampling_time = 1.0f / sampling_rate;
            skeletal_animation.sampling_time = sampling_time;
            skeletal_animation.animation_tick = 0.0f;
            FbxString* animation_stack_name = array_of_animation_stack_names.GetAt(index_of_animation);

            skeletal_animation.name = bone_nodes[0]->GetScene()->GetName();
            
            FbxAnimStack* current_animation_stack
                = bone_nodes[0]->GetScene()->FindMember<FbxAnimStack>(animation_stack_name->Buffer());
            bone_nodes[0]->GetScene()->SetCurrentAnimationStack(current_animation_stack);
            FbxTakeInfo* take_info = bone_nodes[0]->GetScene()->GetTakeInfo(animation_stack_name->Buffer());
            FbxTime start_time = take_info->mLocalTimeSpan.GetStart();
            FbxTime end_time = take_info->mLocalTimeSpan.GetStop();
            FbxTime sampling_step;
            sampling_step.SetTime(0, 0, 1, 0, 0, time_mode);
            sampling_step = static_cast<FbxLongLong>(sampling_step.Get() * sampling_time);
            for (FbxTime current_time = start_time; current_time < end_time; current_time += sampling_step)
            {
                Skeletal skeletal;
                skeletal.resize(bone_nodes.size());
                for (size_t i = 0; i < bone_nodes.size(); i++)
                {
                    FbxSkeleton* fbx_skeletal = bone_nodes[i]->GetSkeleton();

                    const FbxAMatrix& transform = bone_nodes[i]->EvaluateGlobalTransform(current_time) *
                        mCluster_global_init_position[i].Inverse() * mReference_global_init_position[i];
                    Bone& bone = skeletal.at(i);
                    // convert FbxAMatrix(transform) to XMDLOAT4X4(bone_node.transform)
                    for (int i = 0; i < 4; ++i)
                    {
                        for (int j = 0; j < 4; ++j)
                        {
                            bone.transform.m[i][j] = transform[i][j];
                        }
                    }
                }
                skeletal_animation.push_back(skeletal);
            }
            skeletal_animations[skeletal_animation.name] = skeletal_animation;

        }
        for (int i = 0; i < number_of_animations; i++)
        {
            delete array_of_animation_stack_names[i];
        }
    }
}

