// Default.hlsl
struct Light {
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

cbuffer cbPass : register(b0)
{
    float4x4 gViewProj;
    float4x4 gView;            
    float4x4 gProj;            
    float3 gEyePosW;
    float cbPerObjectPad1;     
    float4 gAmbientLight;      
    Light gLights[24];         
    float gTotalTime;          
};
// Compute light contribution
float3 ComputePointLight(Light L, float3 pos, float3 normal)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    
    // If outside the light's reach, return black
    if(d > L.FalloffEnd) return float3(0.0f, 0.0f, 0.0f);
    
    lightVec /= d; // normalize
    
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;
    
    // Linear attenuation
    float att = saturate((L.FalloffEnd - d) / (L.FalloffEnd - L.FalloffStart));
    return lightStrength * att;
}
cbuffer cbObject : register(b1)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    float3 gObjPad;
};
struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};
struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.TexC = vin.TexC;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Goal orb — pulsing transparent emissive sphere (MaterialIndex 2)
    if (gMaterialIndex == 2)
{
    // Very obvious pulse — swings from almost invisible to fully opaque
    float pulse = sin(gTotalTime * 5.0f) * 0.4f + 0.6f;

    // Emissive material for our wall lights
    if (gMaterialIndex == 3)
    {
        // Output pure color, unaffected by other lights
        return float4(1.0f, 1.0f, 0.4f, 1.0f); 
    }

    // Bright obvious rim — make the whole sphere shift color dramatically
    float3 N = normalize(pin.NormalW);
    float3 V = normalize(gEyePosW - pin.PosW);
    float rim = 1.0f - saturate(dot(N, V));

    // Swing between bright red and bright blue so the color change is unmissable
    float3 colorA = float3(1.0f, 0.1f, 0.1f); // red
    float3 colorB = float3(0.1f, 0.5f, 1.0f); // blue
    float3 orbColor = lerp(colorA, colorB, rim) * 3.0f;

    return float4(orbColor, pulse);
}

    float3 N = normalize(pin.NormalW);
    float3 V = normalize(gEyePosW - pin.PosW);

    if (dot(N, V) < 0.0f)
    {
        N = -N;
    }

    // 1. AMBIENT LIGHT
    float3 ambient = float3(0.2f, 0.2f, 0.3f);

    // 2. DIRECTIONAL LIGHT (sun shining down into the tower)
    float3 sunDir  = normalize(float3(0.577f, 0.577f, 0.577f));
    float  sunDiff = max(dot(N, sunDir), 0.0f);
    float3 sunColor = sunDiff * float3(0.6f, 0.6f, 0.5f);

    // 3. PLAYER POINT LIGHT (flashlight attached to camera)
    float3 lightVec = gEyePosW - pin.PosW;
    float  d        = length(lightVec);
    float  atten    = 1.0f / (1.0f + 0.1f * d + 0.01f * d * d);
    float  diff     = max(dot(N, normalize(lightVec)), 0.0f);
    float3 pointColor = diff * atten * float3(1.0f, 0.9f, 0.7f);

    // Base colour
    float3 baseColor = float3(0.5f, 0.5f, 0.5f);

    // Procedural checkerboard for platforms (MaterialIndex 1)
    if (gMaterialIndex == 1)
    {
        float2 uv     = pin.TexC * 5.0f;
        float checker = fmod(abs(floor(uv.x) + floor(uv.y)), 2.0f);
        baseColor     = lerp(float3(0.1f, 0.1f, 0.1f), float3(0.9f, 0.7f, 0.1f), checker);
    }

    // Dark blue carpet for the floor
    if (pin.PosW.y < 0.1f && gMaterialIndex == 0)
        baseColor = float3(0.2f, 0.2f, 0.4f);

    float3 finalLight = ambient + sunColor + (pointColor * 2.0f);

    for(int i = 0; i < 24; ++i)
    {
        finalLight += ComputePointLight(gLights[i], pin.PosW, N);
    }

    return float4(baseColor * finalLight, 1.0f);
}

// =================================================================
// GEOMETRY SHADER: Point to Quad (Billboarding)
// =================================================================
[maxvertexcount(4)]
void GS(point VertexOut gin[1], inout TriangleStream<VertexOut> triStream)
{
    // 1. Get the world position of our single point
    float3 centerPos = gin[0].PosW;

    // 2. Calculate axes so the quad always faces the camera
    float3 look  = normalize(gEyePosW - centerPos);
    float3 up    = float3(0.0f, 1.0f, 0.0f);
    float3 right = normalize(cross(up, look));
    up           = cross(look, right);

    // 3. Define the 4 corners of our new shape (2 units wide/high)
    float halfWidth = 1.0f;
    float halfHeight = 1.0f;
    
    float4 v[4];
    v[0] = float4(centerPos - halfWidth * right - halfHeight * up, 1.0f); // Bottom Left
    v[1] = float4(centerPos - halfWidth * right + halfHeight * up, 1.0f); // Top Left
    v[2] = float4(centerPos + halfWidth * right - halfHeight * up, 1.0f); // Bottom Right
    v[3] = float4(centerPos + halfWidth * right + halfHeight * up, 1.0f); // Top Right

    float2 uvs[4] = { float2(0.0f, 1.0f), float2(0.0f, 0.0f), float2(1.0f, 1.0f), float2(1.0f, 0.0f) };

    // 4. Output the new vertices to the Pixel Shader
    VertexOut gout;
    [unroll]
    for(int i = 0; i < 4; ++i)
    {
        gout.PosW    = v[i].xyz;
        gout.PosH    = mul(v[i], gViewProj); // Project to screen
        gout.NormalW = look;                 // Normal faces camera
        gout.TexC    = uvs[i];
        
        triStream.Append(gout);
    }
}


struct GS_TORCH_IN
{
    float4 PosW  : POSITION;  // World-space point (from VS passthrough)
    float3 NormW : NORMAL;
    float2 TexC  : TEXCOORD;
};

struct GS_TORCH_OUT
{
    float4 PosH  : SV_POSITION;
    float2 TexC  : TEXCOORD0;
    float  Alpha : TEXCOORD1;  // Per-vertex fade based on distance
};

// --- Passthrough VS for torch points ---
GS_TORCH_IN VS_Torch(VertIn vin)
{
    GS_TORCH_IN vout;
    // Transform to world space only (GS will project)
    float4x4 world = gWorld; // your ObjectCB world matrix
    vout.PosW  = mul(float4(vin.PosL, 1.0f), world);
    vout.NormW = vin.NormalL;
    vout.TexC  = vin.TexC;
    return vout;
}

[maxvertexcount(4)]
void GS_Torch(
    point GS_TORCH_IN gin[1],
    inout TriangleStream<GS_TORCH_OUT> triStream)
{
    // --- Billboard axes from the view matrix ---
    // Right and Up in world space from the camera (stored in ViewProj columns)
    float3 right = float3(gViewProj[0][0], gViewProj[1][0], gViewProj[2][0]);
    float3 up    = float3(0.0f, 1.0f, 0.0f); // Keep torches upright

    right = normalize(right);

    // --- Distance-based scale ---
    float3 toPoint   = gin[0].PosW.xyz - gEyePosW;
    float  dist      = length(toPoint);

    // Base size = 12 units. Falls off: half size at 200 units, tiny at 600+
    float scale = 12.0f / (1.0f + dist * 0.008f);
    scale = clamp(scale, 0.5f, 12.0f);

    // Alpha also fades with distance
    float alpha = saturate(1.0f - (dist - 50.0f) / 500.0f);

    // --- Build the 4 quad corners ---
    // Offset upward slightly so the base sits at the point origin
    float3 center = gin[0].PosW.xyz + up * (scale * 0.5f);

    float3 corners[4];
    corners[0] = center - right * scale * 0.5f - up * scale * 0.5f; // BL
    corners[1] = center - right * scale * 0.5f + up * scale * 0.5f; // TL
    corners[2] = center + right * scale * 0.5f - up * scale * 0.5f; // BR
    corners[3] = center + right * scale * 0.5f + up * scale * 0.5f; // TR

    float2 uvs[4] = {
        float2(0.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 1.0f),
        float2(1.0f, 0.0f)
    };

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        GS_TORCH_OUT gout;
        gout.PosH  = mul(float4(corners[i], 1.0f), gViewProj);
        gout.TexC  = uvs[i];
        gout.Alpha = alpha;
        triStream.Append(gout);
    }
    triStream.RestartStrip();
}

// --- Pixel shader for torch ---
float4 PS_Torch(GS_TORCH_OUT pin) : SV_Target
{
    float2 uv = pin.TexC;

    // Animated flame: based on TotalTime from PassCB
    float t = gTotalTime;

    // Flicker: rapid low-amplitude oscillation
    float flicker = 0.85f + 0.15f * sin(t * 17.3f + uv.x * 5.0f)
                          + 0.08f * sin(t * 31.7f);

    // Shape: brighter at center-bottom, fades at top and edges
    float xFade   = 1.0f - abs(uv.x - 0.5f) * 2.0f;       // 0 at edges, 1 at center
    float yFade   = 1.0f - uv.y;                            // 1 at bottom, 0 at top
    float shape   = pow(xFade, 1.5f) * pow(yFade, 0.6f);

    // Discard pixels that are too transparent (cutout the shape)
    float baseAlpha = shape * flicker;
    clip(baseAlpha - 0.05f);  // Hard clip so background is invisible

    // Torch colour gradient: white core -> orange -> red at tips
    float3 innerCol = float3(1.0f,  0.95f, 0.6f);   // Bright white-yellow core
    float3 midCol   = float3(1.0f,  0.45f, 0.05f);  // Orange
    float3 outerCol = float3(0.8f,  0.1f,  0.02f);  // Deep red

    // Blend based on height (uv.y) and distance from center (xFade)
    float3 col = lerp(outerCol, midCol,   saturate(yFade * 2.0f));
    col        = lerp(col,      innerCol, saturate(shape * flicker));

    // Pixel-art look: quantize slightly to mimic Minecraft block style
    // Snap color to ~8 levels per channel
    col = floor(col * 8.0f + 0.5f) / 8.0f;

    float finalAlpha = baseAlpha * pin.Alpha;

    return float4(col, finalAlpha);
}