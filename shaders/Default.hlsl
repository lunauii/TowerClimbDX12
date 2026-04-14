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