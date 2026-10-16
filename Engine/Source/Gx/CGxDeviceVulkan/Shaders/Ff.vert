// The fixed-function vertex program: the position through the device's World * View * ProjNative,
// the vertex colour and the first texture coordinate forwarded as the engine's fixed-function
// pipeline would (no lighting, no fog, no texgen).
#version 450
#extension GL_GOOGLE_include_directive : require

#include "Push.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;      // D3DCOLOR, fetched as r, g, b, a; opaque white when the vertex has none
layout(location = 2) in vec2 inTexCoord;   // texcoord set 0; (0, 0) when the vertex has none

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;

void main()
{
    gl_Position = ToVulkanClip(pc.xform * vec4(inPos, 1.0));
    vColor      = inColor;
    vTexCoord   = inTexCoord;
}
