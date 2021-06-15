#include "00constantbuffer.h"
#include "../../sharedsignature/sharedvertex.h"

HlslTransform transform;
struct PsInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

PsInput vs_main(HlslVertexIn vin)
{
    PsInput result;
    result.position = mul(transform.m_world_from_model, float4(vin.m_position, 1.0f));
    result.color = vin.m_normal;
    result.texcoord = vin.m_texcoord;
    return result;
}

ConstantBuffer<Example00ConstantBuffer> gu : register(b0);
Texture2D g_texture[] : register(t0);
SamplerState g_sampler : register(s0);

float4 fs_main(PsInput ps_input) : SV_TARGET
{
    //return gu.f4;
    //return float4(ps_input.color, 1.0f);
    //return float4(ps_input.texcoord, 0.0f, 0.0f);
    return g_texture[0].Sample(g_sampler, ps_input.texcoord).rgba;
}