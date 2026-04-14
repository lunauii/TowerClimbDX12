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
    if (GetAsyncKeyState('E') & 0x8000) mVerticalVelocity = 80.0f;  // Fly up
    if (GetAsyncKeyState('Q') & 0x8000) mVerticalVelocity = -80.0f; // Fly down

    // 3. JUMPING & GRAVITY
    if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && !mIsJumping) {
        mVerticalVelocity = 60.0f; // Jump force
        mIsJumping = true;
    }
    mVerticalVelocity -= 150.0f * dt; // Gravity

    // 4. PHYSICS & COLLISION
    XMFLOAT3 pos = mCamera.GetPosition3f();
    float nextY = pos.y + (mVerticalVelocity * dt);

    for (auto& ri : mOpaqueRitems) {
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

    // WIN CONDITION: Check if player reached the orb
    if (!mHasWon)
    {
        XMFLOAT3 orbPos = XMFLOAT3(0.0f, 1100.0f, 0.0f); // Match orb translation
        float dx = pos.x - orbPos.x;
        float dy = pos.y - orbPos.y;
        float dz = pos.z - orbPos.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        if (distSq < 50.0f * 50.0f) // Within 50 units of orb centre
        {
            mHasWon = true;
            MessageBox(mhMainWnd, L"You reached the orb! YOU WIN!", L"VICTORY!", MB_OK);
        }
    }
    mCamera.SetPosition(pos.x, nextY, pos.z);
    mCamera.UpdateViewMatrix();

    // 6. UPDATE CONSTANT BUFFERS
    PassConstants passCB;
    XMMATRIX viewProj = mCamera.GetView() * mCamera.GetProj();
    XMStoreFloat4x4(&passCB.ViewProj, XMMatrixTranspose(viewProj));
    passCB.EyePosW = mCamera.GetPosition3f();
    passCB.TotalTime = gt.TotalTime();

    float lightHeights[3] = { 50.0f, 550.0f, 1050.0f };
    int lightIndex = 0;

    for (int ring = 0; ring < 3; ++ring) {
        for (int i = 0; i < 8; ++i) {
            float angle = i * (MathHelper::Pi / 4.0f);

            passCB.Lights[lightIndex].Position = { 145.0f * cos(angle), lightHeights[ring], 145.0f * sin(angle) };
            passCB.Lights[lightIndex].Strength = { 0.8f, 0.8f, 0.2f };
            passCB.Lights[lightIndex].FalloffStart = 20.0f;
            passCB.Lights[lightIndex].FalloffEnd = 120.0f;

            lightIndex++;
        }
    }

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

    // --- PASS 1: Draw all opaque objects ---
    for (auto& ri : mOpaqueRitems) {
        mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

        XMMATRIX modelWorld = XMLoadFloat4x4(&ri->World);
        if (ri->ObjCBIndex >= 2 && ri->ObjCBIndex <= 91) {
            float angle = 0.4f * gt.TotalTime() * (ri->ObjCBIndex % 2 == 0 ? 1 : -1);
            modelWorld = modelWorld * XMMatrixRotationY(angle);
        }

        ObjectConstants objCB;
        XMStoreFloat4x4(&objCB.World, XMMatrixTranspose(modelWorld));
        
        // 0=Wall/Floor, 1=Platforms, 3=Wall Lights
        if (ri->ObjCBIndex >= 93) objCB.MaterialIndex = 3;
        else if (ri->ObjCBIndex >= 2) objCB.MaterialIndex = 1;
        else objCB.MaterialIndex = 0;

        mCurrFrameResource->ObjectCB->CopyData(ri->ObjCBIndex, objCB);

        auto addr = mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress() + (ri->ObjCBIndex * objCBByteSize);
        mCommandList->SetGraphicsRootConstantBufferView(1, addr);
        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

    // --- PASS 2: Draw transparent orb last, with blending PSO ---
    mCommandList->SetPipelineState(mTransparentPSO.Get());
    {
        auto& ri = mOrbRitem;
        mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

        ObjectConstants objCB;
        XMStoreFloat4x4(&objCB.World, XMMatrixTranspose(XMLoadFloat4x4(&ri->World)));
        objCB.MaterialIndex = 2; // Orb material — handled in shader
        mCurrFrameResource->ObjectCB->CopyData(ri->ObjCBIndex, objCB);

        auto addr = mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress() + (ri->ObjCBIndex * objCBByteSize);
        mCommandList->SetGraphicsRootConstantBufferView(1, addr);
        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

    // --- PASS 3: Draw Geometry Shader Sparks ---
    mCommandList->SetPipelineState(mSparkPSO.Get());
    for (auto& ri : mSparkRitems) {
        mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType); // Will set D3D_PRIMITIVE_TOPOLOGY_POINTLIST

        ObjectConstants objCB;
        XMStoreFloat4x4(&objCB.World, XMMatrixTranspose(XMLoadFloat4x4(&ri->World)));
        objCB.MaterialIndex = 2; // Share the pulsing color of the Orb!

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
    // NEW: Sphere for the goal orb at the top of the tower
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(20.0f, 32, 32);
    GeometryGenerator::MeshData lightBox = geoGen.CreateBox(4.0f, 4.0f, 1.0f, 0);

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

    SubmeshGeometry boxSub, cylSub, gridSub, sphereSub, lightBoxSub;
    addMesh(box, boxSub);
    addMesh(cylinder, cylSub);
    addMesh(grid, gridSub);
    addMesh(sphere, sphereSub);
    addMesh(lightBox, lightBoxSub);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "towerGeo";
    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), (UINT)vertices.size() * sizeof(Vertex), geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), (UINT)indices.size() * sizeof(uint32_t), geo->IndexBufferUploader);
    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = (UINT)vertices.size() * sizeof(Vertex);
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = (UINT)indices.size() * sizeof(uint32_t);
    geo->DrawArgs["box"] = boxSub;
    geo->DrawArgs["cylinder"] = cylSub;
    geo->DrawArgs["grid"] = gridSub;
    geo->DrawArgs["sphere"] = sphereSub;
    geo->DrawArgs["lightBox"] = lightBoxSub;
    mGeometries[geo->Name] = std::move(geo);

    // 1. Tower Walls
    auto walls = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&walls->World, XMMatrixTranslation(0, 450, 0));
    walls->ObjCBIndex = 0; walls->Geo = mGeometries["towerGeo"].get();
    walls->IndexCount = cylSub.IndexCount; walls->StartIndexLocation = cylSub.StartIndexLocation; walls->BaseVertexLocation = cylSub.BaseVertexLocation;
    mOpaqueRitems.push_back(walls.get());
    mAllRitems.push_back(std::move(walls));

    // 2. Floor
    auto ground = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&ground->World, XMMatrixTranslation(0, 0, 0));
    ground->ObjCBIndex = 1; ground->Geo = mGeometries["towerGeo"].get();
    ground->IndexCount = gridSub.IndexCount; ground->StartIndexLocation = gridSub.StartIndexLocation; ground->BaseVertexLocation = gridSub.BaseVertexLocation;
    mOpaqueRitems.push_back(ground.get());
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
        mOpaqueRitems.push_back(ri.get());
        mAllRitems.push_back(std::move(ri));
    }

    // 4. Goal Orb — transparent sphere at the top of the tower (ObjCBIndex = 92)
    auto orb = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&orb->World, XMMatrixTranslation(0.0f, 1100.0f, 0.0f));
    orb->ObjCBIndex = 92; orb->Geo = mGeometries["towerGeo"].get();
    orb->IndexCount = sphereSub.IndexCount; orb->StartIndexLocation = sphereSub.StartIndexLocation; orb->BaseVertexLocation = sphereSub.BaseVertexLocation;
    mOrbRitem = orb.get(); // Keep raw pointer for Draw()
    mAllRitems.push_back(std::move(orb));

    // 5. Wall Light Fixtures (ObjCBIndex 93 through 116)
    float lightHeights[3] = { 50.0f, 550.0f, 1050.0f }; // Floor, Midpoint, Ceiling
    int lightIndex = 0;

    for (int ring = 0; ring < 3; ++ring) {
        for (int i = 0; i < 8; ++i) {
            auto ri = std::make_unique<RenderItem>();
            float angle = i * (MathHelper::Pi / 4.0f);
            float r = 148.0f;
            float y = lightHeights[ring];

            XMMATRIX world = XMMatrixRotationY(-angle) * XMMatrixTranslation(r * cos(angle), y, r * sin(angle));
            XMStoreFloat4x4(&ri->World, world);

            ri->ObjCBIndex = 93 + lightIndex;
            ri->Geo = mGeometries["towerGeo"].get();
            ri->IndexCount = lightBoxSub.IndexCount;
            ri->StartIndexLocation = lightBoxSub.StartIndexLocation;
            ri->BaseVertexLocation = lightBoxSub.BaseVertexLocation;
            mOpaqueRitems.push_back(ri.get());
            mAllRitems.push_back(std::move(ri));

            lightIndex++; // Increment so each box gets a unique ObjCBIndex
        }
    }

    // 6. Energy Sparks (ObjCBIndex 117)
    // We only need to define Positions. The GS will build the actual geometry.
    std::vector<Vertex> sparkVertices;
    std::vector<std::uint32_t> sparkIndices;

    for (int i = 0; i < 50; ++i) {
        Vertex v;
        // Random point within a 60 unit radius around the orb
        v.Pos.x = (MathHelper::RandF() * 120.0f) - 60.0f;
        v.Pos.y = 1100.0f + ((MathHelper::RandF() * 40.0f) - 20.0f);
        v.Pos.z = (MathHelper::RandF() * 120.0f) - 60.0f;

        v.Normal = { 0, 1, 0 }; // Ignored by GS
        v.TexC = { 0, 0 };      // Ignored by GS

        sparkVertices.push_back(v);
        sparkIndices.push_back(i);
    }

    // Build the MeshGeometry for the points
    auto sparkGeo = std::make_unique<MeshGeometry>();
    sparkGeo->Name = "sparkGeo";
    sparkGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), sparkVertices.data(), (UINT)sparkVertices.size() * sizeof(Vertex), sparkGeo->VertexBufferUploader);
    sparkGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), sparkIndices.data(), (UINT)sparkIndices.size() * sizeof(uint32_t), sparkGeo->IndexBufferUploader);
    sparkGeo->VertexByteStride = sizeof(Vertex);
    sparkGeo->VertexBufferByteSize = (UINT)sparkVertices.size() * sizeof(Vertex);
    sparkGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    sparkGeo->IndexBufferByteSize = (UINT)sparkIndices.size() * sizeof(uint32_t);

    SubmeshGeometry sparkSub;
    sparkSub.IndexCount = (UINT)sparkIndices.size();
    sparkSub.StartIndexLocation = 0;
    sparkSub.BaseVertexLocation = 0;
    sparkGeo->DrawArgs["sparks"] = sparkSub;

    mGeometries[sparkGeo->Name] = std::move(sparkGeo);

    auto sparks = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&sparks->World, XMMatrixIdentity());
    sparks->ObjCBIndex = 117; // Give it the next available index
    sparks->Geo = mGeometries["sparkGeo"].get();

    // CRITICAL: Tell the API these are POINTS, not triangles
    sparks->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

    sparks->IndexCount = sparkSub.IndexCount;
    sparks->StartIndexLocation = sparkSub.StartIndexLocation;
    sparks->BaseVertexLocation = sparkSub.BaseVertexLocation;

    mSparkRitems.push_back(sparks.get());
    mAllRitems.push_back(std::move(sparks));
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
    mgsByteCode = d3dUtil::CompileShader(L"shaders\\Default.hlsl", nullptr, "GS", "gs_5_0");
    mInputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void TowerGameApp::BuildPSOs() {
    // --- Base PSO descriptor (shared settings) ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), mvsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), mpsByteCode->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;

    // Opaque PSO (unchanged)
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mOpaquePSO)));

    // --- NEW: Transparent PSO — copy opaque desc, then enable alpha blending ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentDesc = psoDesc;

    D3D12_RENDER_TARGET_BLEND_DESC blendDesc;
    blendDesc.BlendEnable = TRUE;
    blendDesc.LogicOpEnable = FALSE;
    blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;       // Multiply source by its alpha
    blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;   // Multiply dest by (1 - alpha)
    blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transparentDesc.BlendState.RenderTarget[0] = blendDesc;

    // Keep depth testing ON so orb is occluded by geometry in front of it,
    // but disable depth writes so it doesn't block objects behind it
    transparentDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // --- Spark Particle PSO ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC sparkDesc = transparentDesc; // Copy the alpha blending settings
    sparkDesc.GS = { reinterpret_cast<BYTE*>(mgsByteCode->GetBufferPointer()), mgsByteCode->GetBufferSize() };
    sparkDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentDesc, IID_PPV_ARGS(&mTransparentPSO)));
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&sparkDesc, IID_PPV_ARGS(&mSparkPSO)));
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