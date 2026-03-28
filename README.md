# 🏰 TowerClimbDX12

## 📝 Project Description

**Tower Climber** is a 3D first-person platforming environment developed using **DirectX 12**, **Win32**, and **C++**. The environment consists of a massive interior tower spire containing procedurally generated floating platforms.

The game features a physics-based movement system including gravity and jumping mechanics. The platforms rotate automatically around the tower's center to provide a dynamic gameplay challenge. The visual style is achieved through custom HLSL shaders implementing ambient lighting, directional "sun" lighting, and a dynamic point light that follows the player's position.

---

## 🚀 Getting Started

### Prerequisites

* **Visual Studio 2022** (or 2026 Preview)
* **C++ Desktop Development Workload** (installed via VS Installer)
* **Windows 10/11 SDK**

### Installation & Launch

1. **Clone the Repository:**
   `git clone https://github.com/[your-username]/TowerClimbDX12.git`

2. **Open the Solution:**
   Navigate to the project folder and double-click `TowerClimbDX12.sln`.

3. **Retargeting (If Prompted):**
   If Visual Studio asks to "Retarget Projects," click **OK** to match your local Windows SDK version.

4. **Build & Run:**
   Set the configuration to **Debug** and the platform to **x64**. Press **F5** to start.

---

## 🛠 Project Configuration

These settings are saved in the `.vcxproj` file. If you need to verify them manually, check **Project Properties**:

* **Include Directories:** `$(ProjectDir);$(ProjectDir)Common;`
* **C++ Standard:** `ISO C++17 Standard (/std:c++17)` or higher.
* **Conformance Mode:** Set to **No**. (Required for the Frank Luna framework).
* **SubSystem:** `Windows (/SUBSYSTEM:WINDOWS)`
* **Linker Dependencies:** `d3d12.lib;dxgi.lib;d3dcompiler.lib;`
* **Shader Setting:** `Default.hlsl` must be set to **"Does not participate in build"** in its file properties.

---

## 🎮 Controls

* **W / A / S / D:** Walk and Strafe
* **Spacebar:** Jump
* **Left Mouse (Hold + Drag):** Look around (Pitch/Yaw)
* **Escape:** Exit application

---

## 📂 Project Structure

* **TowerGameApp.cpp / .h**: Main entry point, update loop, and physics.
* **FrameResource.cpp / .h**: CPU/GPU synchronization and constant buffers.
* **shaders/Default.hlsl**: GPU lighting and vertex logic.
* **Common/**: Frank Luna framework helper classes.

---

## 📚 Citations & Libraries

* **Framework:** Built using the `d3dApp` framework from *Introduction to 3D Game Programming with DirectX 12* by Frank Luna.
* **API:** Microsoft DirectX 12 and DirectXMath.
* **Asset Loading:** `DDSTextureLoader` for DDS textures.
