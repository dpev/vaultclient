cbuffer u_cameraPlaneParams
{
  float s_CameraNearPlane;
  float s_CameraFarPlane;
  float u_clipZNear;
  float u_clipZFar;
};

struct VS_INPUT
{
  float3 pos : POSITION;
  float3 normal : NORMAL;
  float2 uv  : TEXCOORD0;
  //float4x4 instanceModel : TEXCOORD1;
  uint instanceId : SV_InstanceID;
};

struct PS_INPUT
{
  float4 pos : SV_POSITION;
  float2 uv  : TEXCOORD0;
  float3 normal : NORMAL;
  float4 colour : COLOR0;
  float2 fLogDepth : TEXCOORD1;
  float2 objectInfo : TEXCOORD2;
};

struct InstanceData
{
  float4x4 worldViewProjectionMatrix;
  float4x4 worldMatrix;
  float4 colour;
  float4 objectInfo; // id.x, isSelectable.y, (unused).zw
};

cbuffer u_EveryObject : register(b0)
{
  InstanceData u_instanceData[409];
};

PS_INPUT main(VS_INPUT input)
{
  PS_INPUT output;

  // making the assumption that the model matrix won't contain non-uniform scale
  float3 worldNormal = normalize(mul(u_instanceData[input.instanceId].worldMatrix, float4(input.normal, 0.0)).xyz);

  output.pos = mul(u_instanceData[input.instanceId].worldViewProjectionMatrix, float4(input.pos, 1.0));
  output.uv = input.uv;
  output.normal = worldNormal;
  output.colour = u_instanceData[input.instanceId].colour;// * input.colour;
  output.fLogDepth.x = 1.0 + output.pos.w;
  output.objectInfo = u_instanceData[input.instanceId].objectInfo.xy;

  return output;
}
