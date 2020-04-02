#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 out_var_SV_Target [[color(0)]];
};

struct main0_in
{
    float4 in_var_COLOR0 [[user(locn0)]];
    float2 in_var_TEXCOORD0 [[user(locn1)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> TextureTexture [[texture(0)]], sampler TextureSampler [[sampler(0)]])
{
    main0_out out = {};
    out.out_var_SV_Target = in.in_var_COLOR0 * TextureTexture.sample(TextureSampler, in.in_var_TEXCOORD0);
    return out;
}

