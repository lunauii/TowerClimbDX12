🏰 TowerClimbDX12: Group Project
📝 Project Description

Tower Climber is a 3D first-person platforming environment developed using DirectX 12, Win32, and C++. The environment consists of a massive interior tower spire containing procedurally generated floating platforms.

The game features a physics-based movement system including gravity and jumping mechanics. The platforms rotate automatically around the tower's center to provide a dynamic gameplay challenge. The visual style is achieved through custom HLSL shaders implementing ambient lighting, directional "sun" lighting, and a dynamic point light that follows the player's position.
🚀 Getting Started
Prerequisites

    Visual Studio 2022 (or 2026 Preview).

    C++ Desktop Development Workload installed via Visual Studio Installer.

    Windows 10/11 SDK.

Installation & Launch

    Clone the Repository:
    git clone https://github.com/[your-username]/TowerClimbDX12.git

    Open the Solution:
    Navigate to the project folder and double-click TowerClimbDX12.sln.

    Retargeting (If Prompted):
    If Visual Studio asks to "Retarget Projects," click OK to match your local Windows SDK version.

    Build & Run:
    Set the configuration to Debug and the platform to x64. Press F5 to start.

🛠 Project Configuration

The following settings are pre-configured in the .vcxproj file. If you create a new project or experience configuration loss, ensure these settings are active:
Compiler (C/C++)

    General -> Additional Include Directories:
    $(ProjectDir);$(ProjectDir)Common;

    Language -> C++ Language Standard:
    ISO C++17 Standard (/std:c++17) or higher.

    Language -> Conformance Mode:
    No. (Required to allow the Frank Luna framework style without l-value address errors).

Linker

    System -> SubSystem:
    Windows (/SUBSYSTEM:WINDOWS)

    Input -> Additional Dependencies:
    d3d12.lib;dxgi.lib;d3dcompiler.lib;

Shaders (HLSL)

    Item Type:
    Shader files (e.g., Default.hlsl) are set to "Does not participate in build" because the engine compiles them at runtime via d3dUtil::CompileShader.

🎮 Controls

    W / A / S / D: Walk and Strafe.

    Spacebar: Jump.

    Left Mouse Click + Drag: Look around (Rotate camera Pitch and Yaw).

    Escape: Exit the application.

📂 Project Structure

    TowerGameApp.cpp / .h: Main entry point and game logic. Handles the update loop, physics, and object management.

    FrameResource.cpp / .h: Manages the synchronization between the CPU and GPU (Fences/Constant Buffers).

    /shaders: Contains Default.hlsl, handling the lighting model and interior visibility (Culling: None).

    /Common: The framework library providing helper classes for DirectX 12 initialization and geometry generation.

    /textures: Storage for DDS texture files.

    /models: Storage for imported 3D assets (OBJ/FBX).

👥 Team Workflow

    The Project Map: The .sln and .vcxproj files are the "map" of the project. If you add a new file, ensure you commit the updated .vcxproj so your teammates can see the file in their Solution Explorer.

    Relative Paths: Always use $(ProjectDir) for paths to ensure the project builds correctly across different machines and drive letters.

📚 Citations & Libraries

    Framework: Built using the d3dApp framework from Introduction to 3D Game Programming with DirectX 12 by Frank Luna.

    API: Microsoft DirectX 12 and DirectXMath library.

    Asset Loading: DDSTextureLoader for loading DirectDraw Surface textures.
