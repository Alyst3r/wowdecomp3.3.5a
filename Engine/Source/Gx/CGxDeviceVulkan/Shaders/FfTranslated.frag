// The fixed-function pixel stage in the translated interface: what a translated vertex module pairs
// with when the engine binds no pixel shader and the vertex record names no generated companion.
// D3D9's texture stage 0 over the texture and the vertex module's colour 0 (GxRs_ColorOp0 /
// GxRs_AlphaOp0 from the push constants, as Ff.frag), then the alpha test. Reads color0 and
// texcoord0 only, so it pairs with any vertex module that writes both.
#version 450
#extension GL_GOOGLE_include_directive : require

#include "Push.glsl"
#include "AlphaTest.glsl"

layout(location = 0) in vec4 color0;
layout(location = 2) in vec4 texcoord0;

layout(location = 0) out vec4 oC0;

layout(set = 0, binding = 0) uniform sampler2D s0;

vec3 Stage(uint op, vec3 t, vec3 c, float ca)
{
    vec3 r;
    if      (op == 0u) r = t * c;
    else if (op == 1u) r = 2.0 * t * c;
    else if (op == 2u) r = t + c;
    else if (op == 3u) r = c;
    else if (op == 4u) r = c * ca + t * (1.0 - ca);
    else               r = t * ca + c * (1.0 - ca);
    return clamp(r, 0.0, 1.0);
}

void main()
{
    vec4 cur = clamp(color0, 0.0, 1.0);
    if (pc.params.y > 0.5)
    {
        vec4 t = texture(s0, texcoord0.xy);
        uint colourOp = uint(pc.params.z + 0.5);
        uint alphaOp  = uint(pc.params.w + 0.5);
        cur.rgb = Stage(colourOp, t.rgb, color0.rgb, color0.a);
        cur.a   = Stage(alphaOp, t.aaa, color0.aaa, color0.a).x;
    }
    AlphaTest(cur.a);
    oC0 = cur;
}
