#include "TowerGameApp.h"
#include "Common/GeometryGenerator.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    try {
        TowerGameApp theApp(hInstance);
        if (!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch (DxException& e) {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

TowerGameApp::TowerGameApp(HINSTANCE hInstance) : D3DApp(hInstance) {
    mMainWndCaption = L"DX12 Tower Climber - Inside the Spire";
}

bool TowerGameApp::Initialize() {
    if (!D3DApp::Initialize()) return false;

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // STARTING POSITION: Inside the tower, on the floor
    mCamera.SetPosition(0.0f, 5.0f, -30.0f);

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildTowerGeometry();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();

    return true;
}

void TowerGameApp::OnResize() {
    D3DApp::OnResize();
    mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 2000.0f);
}

void TowerGameApp::Update(const GameTimer& gt) {
    // 1. Frame Sync
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % 3;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    float dt = gt.DeltaTime();

    // 2. FPS MOVEMENT (WASD)
    float speed = 50.0f * dt;
    if (GetAsyncKeyState('W') & 0x8000) mCamera.Walk(speed);
    if (GetAsyncKeyState('S') & 0x8000) mCamera.Walk(-speed);
    if (GetAsyncKeyState('A') & 0x8000) mCamera.Strafe(-speed);
    if (GetAsyncKeyState('D') & 0x8000) mCamera.Strafe(speed);

    // 3. JUMPING & GRAVITY
    if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && !mIsJumping) {
        mVerticalVelocity = 60.0f; // Jump force
        mIsJumping = true;
    }
    mVerticalVelocity -= 150.0f * dt; // Gravity

    // 4. PHYSICS & COLLISION
    XMFLOAT3 pos = mCamera.GetPosition3f();
    float nextY = pos.y + (mVerticalVelocity * dt); // FIX: nextY declared here

    for (auto& ri : mAllRitems) {
        if (ri->ObjCBIndex < 2) continue; // Skip walls (0) and floor (1)

        // Calculate platform's current animated rotation (Automatic Movement Mark)
        float time = gt.TotalTime();
        float angle = 0.4f * time * (ri->ObjCBIndex % 2 == 0 ? 1 : -1);
        XMMATRIX baseWorld = XMLoadFloat4x4(&ri->World);
        XMMATRIX animatedWorld = baseWorld * XMMatrixRotationY(angle);

        // Get platform's center in world space
        float px = animatedWorld.r[3].m128_f32[0];
        float py = animatedWorld.r[3].m128_f32[1];
        float pz = animatedWorld.r[3].m128_f32[2];

        // Collision check (AABB)
        if (pos.x > px - 8.0f && pos.x < px + 8.0f &&
            pos.z > pz - 8.0f && pos.z < pz + 8.0f) {

            // If falling and eye-level is about to drop through the platform top
            if (mVerticalVelocity <= 0 && pos.y >= py + 4.5f && nextY <= py + 5.0f) {
                nextY = py + 5.0f; // Eye level height
                mVerticalVelocity = 0.0f;
                mIsJumping = false;
                break;
            }
        }
    }

    // 5. STATIC FLOOR COLLISION (Y=5 is ground eye-level)
    if (nextY < 5.0f) {
        nextY = 5.0f;
        mVerticalVelocity = 0.0f;
        mIsJumping = false;
    }

    mCamera.SetPosition(pos.x, nextY, pos.z);
    mCamera.UpdateViewMatrix();

    // 6. UPDATE CONSTANT BUFFERS
    PassConstants passCB;
    XMMATRIX viewProj = mCamera.GetView() * mCamera.GetProj();
    XMStoreFloat4x4(&passCB.ViewProj, XMMatrixTranspose(viewProj));
    passCB.EyePosW = mCamera.GetPosition3f();
    passCB.TotalTime = gt.TotalTime();
    mCurrFrameResource->PassCB->CopyData(0, passCB);
}

void TowerGameApp::Draw(const GameTimer& gt) {
    auto alloc = mCurrFrameResource->CmdListAlloc.Get();
    ThrowIfFailed(alloc->Reset());
    ThrowIfFailed(mCommandList->Reset(alloc, mOpaquePSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    auto b1 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &b1);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = DepthStencilView();
    mCommandList->ClearRenderTargetView(rtv, Colors::Black, 0, nullptr);
    mCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    mCommandList->SetGraphicsRootConstantBufferView(0, mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress());

    UINT objCBByteSize = (sizeof(ObjectConstants) + 255) & ~255;

    for (auto& ri : mAllRitems) {
        mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

        // Apply automatic rotation for Draw call
        XMMATRIX modelWorld = XMLoadFloat4x4(&ri->World);
        if (ri->ObjCBIndex >= 2) {
            float angle = 0.4f * gt.TotalTime() * (ri->ObjCBIndex % 2 == 0 ? 1 : -1);
            modelWorld = modelWorld * XMMatrixRotationY(angle);
        }

        ObjectConstants objCB;
        XMStoreFloat4x4(&objCB.World, XMMatrixTranspose(modelWorld));
        mCurrFrameResource->ObjectCB->CopyData(ri->ObjCBIndex, objCB);

        auto addr = mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress() + (ri->ObjCBIndex * objCBByteSize);
        mCommandList->SetGraphicsRootConstantBufferView(1, addr);

        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

    auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &b2);

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
    mCurrFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void TowerGameApp::BuildTowerGeometry() {
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(12.0f, 1.0f, 12.0f, 3);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(150.0f, 150.0f, 1000.0f, 60, 60);
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(400.0f, 400.0f, 20, 20);

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    auto addMesh = [&](GeometryGenerator::MeshData& data, SubmeshGeometry& sub) {
        sub.IndexCount = (UINT)data.Indices32.size();
        sub.StartIndexLocation = (UINT)indices.size();
        sub.BaseVertexLocation = (UINT)vertices.size();
        for (auto& v : data.Vertices) {
            Vertex nv; nv.Pos = v.Position; nv.Normal = v.Normal; nv.TexC = v.TexC;
            vertices.push_back(nv);
        }
        for (auto& i : data.Indices32) indices.push_back(i);
        };

    SubmeshGeometry boxSub, cylSub, gridSub;
    addMesh(box, boxSub); addMesh(cylinder, cylSub); addMesh(grid, gridSub);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "towerGeo";
    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), (UINT)vertices.size() * sizeof(Vertex), geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), (UINT)indices.size() * sizeof(uint32_t), geo->IndexBufferUploader);
    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = (UINT)vertices.size() * sizeof(Vertex);
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = (UINT)indices.size() * sizeof(uint32_t);
    geo->DrawArgs["box"] = boxSub; geo->DrawArgs["cylinder"] = cylSub; geo->DrawArgs["grid"] = gridSub;
    mGeometries[geo->Name] = std::move(geo);

    // 1. Tower Walls
    auto walls = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&walls->World, XMMatrixTranslation(0, 450, 0));
    walls->ObjCBIndex = 0; walls->Geo = mGeometries["towerGeo"].get();
    walls->IndexCount = cylSub.IndexCount; walls->StartIndexLocation = cylSub.StartIndexLocation; walls->BaseVertexLocation = cylSub.BaseVertexLocation;
    mAllRitems.push_back(std::move(walls));

    // 2. Floor
    auto ground = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&ground->World, XMMatrixTranslation(0, 0, 0));
    ground->ObjCBIndex = 1; ground->Geo = mGeometries["towerGeo"].get();
    ground->IndexCount = gridSub.IndexCount; ground->StartIndexLocation = gridSub.StartIndexLocation; ground->BaseVertexLocation = gridSub.BaseVertexLocation;
    mAllRitems.push_back(std::move(ground));

    // 3. Platforms
    for (int i = 0; i < 90; ++i) {
        auto ri = std::make_unique<RenderItem>();
        float r = 30.0f + MathHelper::RandF() * 100.0f;
        float theta = MathHelper::RandF() * 2.0f * MathHelper::Pi;
        float y = 12.0f + (i * 12.0f);
        XMStoreFloat4x4(&ri->World, XMMatrixTranslation(r * cos(theta), y, r * sin(theta)));
        ri->ObjCBIndex = i + 2; ri->Geo = mGeometries["towerGeo"].get();
        ri->IndexCount = boxSub.IndexCount; ri->StartIndexLocation = boxSub.StartIndexLocation; ri->BaseVertexLocation = boxSub.BaseVertexLocation;
        mAllRitems.push_back(std::move(ri));
    }
}

void TowerGameApp::BuildRootSignature() {
    CD3DX12_ROOT_PARAMETER slot[2];
    slot[0].InitAsConstantBufferView(0);
    slot[1].InitAsConstantBufferView(1);
    CD3DX12_ROOT_SIGNATURE_DESC rsDesc(2, slot, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serialize, error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialize, &error));
    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serialize->GetBufferPointer(), serialize->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void TowerGameApp::BuildShadersAndInputLayout() {
    mvsByteCode = d3dUtil::CompileShader(L"shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
    mInputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void TowerGameApp::BuildPSOs() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), mvsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), mpsByteCode->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Allow seeing interior walls
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mOpaquePSO)));
}

void TowerGameApp::BuildFrameResources() {
    for (int i = 0; i < 3; ++i) mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, 200));
}

void TowerGameApp::OnMouseDown(WPARAM btnState, int x, int y) { mLastMousePos.x = x; mLastMousePos.y = y; SetCapture(mhMainWnd); }
void TowerGameApp::OnMouseUp(WPARAM btnState, int x, int y) { ReleaseCapture(); }
void TowerGameApp::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_LBUTTON) != 0) {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        mCamera.Pitch(dy);
        mCamera.RotateY(dx);
    }
    mLastMousePos.x = x; mLastMousePos.y = y;
}