#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct InstanceData
{
    float4x4 worldViewProjectionMatrix;
    float4x4 worldMatrix;
    float4 colour;
    float4 objectInfo;
};

struct type_u_EveryObject
{
    InstanceData u_instanceData[409];
};

constant float2 _42 = {};

struct main0_out
{
    float2 out_var_TEXCOORD0 [[user(locn0)]];
    float3 out_var_NORMAL [[user(locn1)]];
    float4 out_var_COLOR0 [[user(locn2)]];
    float2 out_var_TEXCOORD1 [[user(locn3)]];
    float2 out_var_TEXCOORD2 [[user(locn4)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 in_var_POSITION [[attribute(0)]];
    float3 in_var_NORMAL [[attribute(1)]];
    float2 in_var_TEXCOORD0 [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant type_u_EveryObject& u_EveryObject [[buffer(0)]], uint gl_InstanceIndex [[instance_id]])
{
    main0_out out = {};
    float4 _63 = u_EveryObject.u_instanceData[gl_InstanceIndex].worldViewProjectionMatrix * float4(in.in_var_POSITION, 1.0);
    float2 _68 = _42;
    _68.x = 1.0 + _63.w;
    out.gl_Position = _63;
    out.out_var_TEXCOORD0 = in.in_var_TEXCOORD0;
    out.out_var_NORMAL = normalize((u_EveryObject.u_instanceData[gl_InstanceIndex].worldMatrix * float4(in.in_var_NORMAL, 0.0)).xyz);
    out.out_var_COLOR0 = u_EveryObject.u_instanceData[gl_InstanceIndex].colour;
    out.out_var_TEXCOORD1 = _68;
    out.out_var_TEXCOORD2 = u_EveryObject.u_instanceData[gl_InstanceIndex].objectInfo.xy;
    return out;
}

