#include "Common/d3dApp.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/Camera.h"
#include "FrameResource.h"

using namespace DirectX;

class TowerGameApp : public D3DApp
{
public:
    TowerGameApp(HINSTANCE hInstance);
    virtual bool Initialize() override;
private:
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

    // Game Logic
    void BuildTowerGeometry();
    void BuildShadersAndInputLayout();
    void BuildRenderItems();
    
    Camera mCamera; // First Person Camera (10% marks)
    float mLavaHeight = -10.0f; // Rising Lava (5% Timing marks)
    
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    RenderItem* mLavaRitem = nullptr; // For automatic movement (15% marks)
};