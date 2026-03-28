// Default.hlsl
cbuffer cbPass : register(b0)
{
    float4x4 gViewProj;
    float3 gEyePosW;
    float gTotalTime;
};

cbuffer cbObject : register(b1)
{
    float4x4 gWorld;
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
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 N = normalize(pin.NormalW);
    float3 V = normalize(gEyePosW - pin.PosW);

    // 1. AMBIENT LIGHT (Fixes the "pitch black" problem)
    float3 ambient = float3(0.2f, 0.2f, 0.3f);

    // 2. DIRECTIONAL LIGHT (The Sun - shines down into the tower)
    float3 sunDir = normalize(float3(0.577f, 0.577f, 0.577f));
    float sunDiff = max(dot(N, sunDir), 0.0f);
    float3 sunColor = sunDiff * float3(0.6f, 0.6f, 0.5f);

    // 3. PLAYER POINT LIGHT (The Flashlight)
    float3 lightVec = gEyePosW - pin.PosW;
    float d = length(lightVec);
    float atten = 1.0f / (1.0f + 0.1f * d + 0.01f * d * d);
    float diff = max(dot(N, normalize(lightVec)), 0.0f);
    float3 pointColor = diff * atten * float3(1.0f, 0.9f, 0.7f);

    // Final color logic
    float3 baseColor = float3(0.5f, 0.5f, 0.5f); // Grey stone
    
    // Make the floor (grid) a different color (Dark Blue Carpet)
    if (pin.PosW.y < 0.1f)
        baseColor = float3(0.2f, 0.2f, 0.4f);

    float3 finalLight = ambient + sunColor + (pointColor * 2.0f);
    return float4(baseColor * finalLight, 1.0f);
}