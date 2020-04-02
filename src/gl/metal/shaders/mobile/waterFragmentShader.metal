#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct type_u_cameraPlaneParams
{
    float s_CameraNearPlane;
    float s_CameraFarPlane;
    float u_clipZNear;
    float u_clipZFar;
};

struct type_u_EveryFrameFrag
{
    float4 u_specularDir;
    float4x4 u_eyeNormalMatrix;
    float4x4 u_inverseViewMatrix;
};

struct main0_out
{
    float4 out_var_SV_Target [[color(0)]];
    float gl_FragDepth [[depth(any)]];
};

struct main0_in
{
    float2 in_var_TEXCOORD0 [[user(locn0)]];
    float2 in_var_TEXCOORD1 [[user(locn1)]];
    float4 in_var_COLOR0 [[user(locn2)]];
    float4 in_var_COLOR1 [[user(locn3)]];
    float2 in_var_TEXCOORD2 [[user(locn4)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant type_u_cameraPlaneParams& u_cameraPlaneParams [[buffer(0)]], constant type_u_EveryFrameFrag& u_EveryFrameFrag [[buffer(1)]], texture2d<float> normalMapTexture [[texture(0)]], texture2d<float> skyboxTexture [[texture(1)]], sampler normalMapSampler [[sampler(0)]], sampler skyboxSampler [[sampler(1)]])
{
    main0_out out = {};
    float3 _85 = normalize(((normalMapTexture.sample(normalMapSampler, in.in_var_TEXCOORD0).xyz * float3(2.0)) - float3(1.0)) + ((normalMapTexture.sample(normalMapSampler, in.in_var_TEXCOORD1).xyz * float3(2.0)) - float3(1.0)));
    float3 _87 = normalize(in.in_var_COLOR0.xyz);
    float3 _103 = normalize((u_EveryFrameFrag.u_eyeNormalMatrix * float4(_85, 0.0)).xyz);
    float3 _105 = normalize(reflect(_87, _103));
    float3 _129 = normalize((u_EveryFrameFrag.u_inverseViewMatrix * float4(_105, 0.0)).xyz);
    out.out_var_SV_Target = float4(mix(skyboxTexture.sample(skyboxSampler, (float2(atan2(_129.x, _129.y) + 3.1415927410125732421875, acos(_129.z)) * float2(0.15915493667125701904296875, 0.3183098733425140380859375))).xyz, in.in_var_COLOR1.xyz * mix(float3(1.0, 1.0, 0.60000002384185791015625), float3(0.3499999940395355224609375), float3(pow(fast::max(0.0, _85.z), 5.0))), float3(((dot(_103, _87) * (-0.5)) + 0.5) * 0.75)) + float3(pow(abs(dot(_105, normalize((u_EveryFrameFrag.u_eyeNormalMatrix * float4(normalize(u_EveryFrameFrag.u_specularDir.xyz), 0.0)).xyz))), 50.0) * 0.5), 1.0);
    out.gl_FragDepth = log2(in.in_var_TEXCOORD2.x) * (1.0 / log2(u_cameraPlaneParams.s_CameraFarPlane + 1.0));
    return out;
}

