// The fixed-function fragment program: D3D9's texture stage 0 as the engine configures it
// (GxRs_ColorOp0 / GxRs_AlphaOp0 over the texture and the vertex colour), then the alpha test.
// Without a texture at stage 0 the stage is disabled and the vertex colour passes through.
#version 450
#extension GL_GOOGLE_include_directive : require

#include "Push.glsl"
#include "AlphaTest.glsl"

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D tex0;

// One texture stage op (CGxDeviceD3d's s_texOp / s_texArgs tables) on one channel set. `t` is the
// texture, `c` the current colour (the diffuse at stage 0), `ca` the current alpha, which is also
// the diffuse alpha at stage 0.
//
//   0 Mod                MODULATE          texture * current
//   1 Mod2x              MODULATE2X        2 * texture * current
//   2 Add                ADD               texture + current
//   3 PassThru           SELECTARG2        current (the texture is ignored)
//   4 Decal              BLENDCURRENTALPHA current * ca + texture * (1 - ca)
//   5 Fade               BLENDDIFFUSEALPHA texture * da + current * (1 - da), da the diffuse alpha
//
// Every result saturates, as a D3D stage's does.
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
    vec4 cur = vColor;
    if (pc.params.y > 0.5)
    {
        vec4 t = texture(tex0, vTexCoord);
        uint colourOp = uint(pc.params.z + 0.5);
        uint alphaOp  = uint(pc.params.w + 0.5);
        cur.rgb = Stage(colourOp, t.rgb, vColor.rgb, vColor.a);
        cur.a   = Stage(alphaOp, t.aaa, vColor.aaa, vColor.a).x;
    }
    AlphaTest(cur.a);
    outColor = cur;
}
