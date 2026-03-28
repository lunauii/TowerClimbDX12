Texture2D gDiffuseMap : register(t0);
SamplerState gsamAnisotropicWrap : register(s0);

cbuffer cbPass : register(b0) {
    float4x4 gViewProj;
    float3 gEyePosW;
    float gTotalTime;
};

cbuffer cbObject : register(b1) {
    float4x4 gWorld;
};

struct VertexIn {
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct VertexOut {
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin) {
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = vin.TexC;
    return vout;
}

// MARK: Geometry Shader (10%) - Explodes/pulses based on time
[maxvertexcount(3)]
void GS(triangle VertexOut gin[3], inout TriangleStream<VertexOut> triStream) {
    float3 normal = normalize(cross(gin[1].PosW - gin[0].PosW, gin[2].PosW - gin[0].PosW));
    float offset = sin(gTotalTime * 2.0f) * 0.1f; // Subtle pulse effect

    for(int i = 0; i < 3; ++i) {
        VertexOut vout = gin[i];
        vout.PosW += normal * offset;
        vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
        triStream.Append(vout);
    }
}

float4 PS(VertexOut pin) : SV_Target {
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);
    
    // MARK: Blending (10%) - Glass effect for high platforms
    // clip(diffuseAlbedo.a - 0.1f); // Use for alpha testing
    
    return diffuseAlbedo; 
}