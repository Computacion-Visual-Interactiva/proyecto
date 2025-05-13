Texture2D g_SkyTex;
SamplerState g_SkyTex_sampler;

cbuffer CB
{
    float4x4 g_ViewProjInv; // inverse View‑Projection, row‑major
};

struct VSOut
{
    float3 Dir : TEXCOORD0;
    float4 Pos : SV_Position;
};

VSOut VSMain(uint vId : SV_VertexID)
{
    // Full‑screen triangle (no vertex buffer)
    float2 uv = float2((vId << 1) & 2, vId & 2); // (‑1|+1,‑1|+1)
    float4 clip = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    float4 wpos = mul(g_ViewProjInv, clip); // back‑project to world
    float3 dir = normalize(wpos.xyz / wpos.w); // view ray

    VSOut outp;
    outp.Dir = dir;
    outp.Pos = clip;
    return outp; // passthrough to PS
}

float4 PSMain(VSOut i) : SV_Target
{
    // view‑direction → equirectangular lookup -----------------------------
    float2 uv;
    uv.x = 0.5 + atan2(i.Dir.z, i.Dir.x) / (2 * 3.14159265);
    uv.y = 0.5 - asin(i.Dir.y) / 3.14159265;
    return g_SkyTex.Sample(g_SkyTex_sampler, uv);
}
