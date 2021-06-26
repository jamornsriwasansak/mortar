struct PsInput
{
    float4 position : SV_POSITION;
    float2 texpos : TEXCOORD0;
};

PsInput VsMain(uint vid : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(1.0f, -1.0f),
        float2(1.0f, 3.0f),
        float2(-3.0f, -1.0f)
    };
    PsInput output;
    output.position = float4(positions[vid], 0.0f, 1.0);
    output.texpos = (positions[vid] + float2(1.0f, 1.0f)) * 0.5f;
    return output;
}

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 FsMain(PsInput input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.texpos.xy);
}