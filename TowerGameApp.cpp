#include "TowerGameApp.h"

// Define a simple structure for our RenderItems
struct RenderItem {
    XMFLOAT4X4 World = MathHelper::Identity4x4();
    MeshGeometry* Geo = nullptr;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
    int NumFramesDirty = 3; // For updating positions
};

void TowerGameApp::BuildTowerGeometry() {
    GeometryGenerator geoGen;
    // 1. Core Tower (Cylinder)
    GeometryGenerator::MeshData tower = geoGen.CreateCylinder(2.0f, 2.0f, 100.0f, 20, 20);
    // 2. Platforms (Boxes)
    GeometryGenerator::MeshData platform = geoGen.CreateBox(3.0f, 0.5f, 3.0f, 1);
    // 3. Lava (Grid)
    GeometryGenerator::MeshData lava = geoGen.CreateGrid(100.0f, 100.0f, 10, 10);
    
    // Merge into one MeshGeometry object (Luna Ch 7 style)...
}

void TowerGameApp::Update(const GameTimer& gt) {
    // MARK: First Person Camera (10%)
    if(GetAsyncKeyState('W') & 0x8000) mCamera.Walk(10.0f*gt.DeltaTime());
    if(GetAsyncKeyState('S') & 0x8000) mCamera.Walk(-10.0f*gt.DeltaTime());
    // ... Add A/D for Strafe ...

    mCamera.UpdateViewMatrix();

    // MARK: Automatic Movement - Rising Lava (15%)
    static float lavaY = -50.0f;
    lavaY += 0.5f * gt.DeltaTime(); // Lava rises over time
    
    // Update the World Matrix for the Lava RenderItem
    XMMATRIX lavaWorld = XMMatrixTranslation(0, lavaY, 0);
    XMStoreFloat4x4(&mLavaRitem->World, lavaWorld);
    mLavaRitem->NumFramesDirty = 3; // Tell GPU to update
}