#version 300 es

struct InstanceData
{
    mat4 worldViewProjectionMatrix;
    mat4 worldMatrix;
    vec4 colour;
    vec4 objectInfo;
};

layout(std140) uniform type_u_EveryObject
{
    layout(row_major) InstanceData u_instanceData[409];
} u_EveryObject;

layout(location = 0) in vec3 in_var_POSITION;
layout(location = 1) in vec3 in_var_NORMAL;
layout(location = 2) in vec2 in_var_TEXCOORD0;
uniform int SPIRV_Cross_BaseInstance;
out vec2 varying_TEXCOORD0;
out vec3 varying_NORMAL;
out vec4 varying_COLOR0;
out vec2 varying_TEXCOORD1;
out vec2 varying_TEXCOORD2;

vec2 _42;

void main()
{
    vec4 _63 = vec4(in_var_POSITION, 1.0) * u_EveryObject.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].worldViewProjectionMatrix;
    vec2 _68 = _42;
    _68.x = 1.0 + _63.w;
    gl_Position = _63;
    varying_TEXCOORD0 = in_var_TEXCOORD0;
    varying_NORMAL = normalize((vec4(in_var_NORMAL, 0.0) * u_EveryObject.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].worldMatrix).xyz);
    varying_COLOR0 = u_EveryObject.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].colour;
    varying_TEXCOORD1 = _68;
    varying_TEXCOORD2 = u_EveryObject.u_instanceData[uint((gl_InstanceID + SPIRV_Cross_BaseInstance))].objectInfo.xy;
}

