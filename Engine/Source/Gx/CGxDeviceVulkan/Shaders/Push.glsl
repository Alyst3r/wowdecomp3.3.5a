// The push constants every pipeline of the Vulkan backend shares: 80 bytes, pushed whole by each draw.
//
//   xform   the vertex transform. For the fixed-function vertex program the engine's row-major
//           World * View * ProjNative, read as a column-major mat4 (so xform * v is the row-vector
//           product v * M). For the UI vertex program the four vertex constant registers c0..c3
//           (the engine's ViewProjNativeTranspose), one per column.
//   params  x : the alpha test reference as the engine's 8-bit value; 0 turns the test off
//           y : 1 when a texture is bound at stage 0, 0 otherwise
//           z : the stage-0 colour op (GxRs_ColorOp0), w : the stage-0 alpha op (GxRs_AlphaOp0)
layout(push_constant) uniform Push
{
    mat4 xform;
    vec4 params;
} pc;

// The D3D clip space the engine's matrices produce has y up; Vulkan's has y down.
vec4 ToVulkanClip(vec4 clip)
{
    clip.y = -clip.y;
    return clip;
}
