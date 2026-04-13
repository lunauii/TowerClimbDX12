#pragma once
#include "Common/d3dApp.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/Camera.h"
#include "FrameResource.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <wrl.h>
using Microsoft::WRL::ComPtr;
using namespace DirectX;
struct RenderItem {
    XMFLOAT4X4 World = MathHelper::Identity4x4();
    MeshGeometry* Geo = nullptr;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
    int ObjCBIndex = -1;
};
class TowerGameApp : public D3DApp {
public:
    TowerGameApp(HINSTANCE hInstance);
    virtual bool Initialize() override;
private:
    POINT mLastMousePos;
    float mPitch = 0.0f;
    float mYaw = 0.0f;
    // Pseudo-Physics
    float mVerticalVelocity = 0.0f;
    bool mIsJumping = false;
    bool mHasWon = false;   
    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    void BuildTowerGeometry();
    void BuildShadersAndInputLayout();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildRootSignature();
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;
    ComPtr<ID3D12PipelineState> mTransparentPSO = nullptr;   
    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    std::vector<RenderItem*> mOpaqueRitems;                 
    RenderItem* mOrbRitem = nullptr;                         
    RenderItem* mLavaRitem = nullptr;
    Camera mCamera;
};