// The Vulkan backend's virtuals that are not implemented on the device: texture copies, render
// states beyond what the draw reads, render targets as targets, queries, read-back, stereo. Where
// the base class has a complete engine-side implementation it is called, so the engine's own
// bookkeeping (the query list, the target record) stays correct; the rest answer "not available"
// the way the engine expects.
#include <cstdint>
#include <cstring>
#include "Engine/Source/Gx/CGxDeviceVulkan/CGxDeviceVulkan.h"
#include "Engine/Source/Gx/CGxTex.h"

void CGxDeviceVulkan::PrimPointSize(float size) {
}

void CGxDeviceVulkan::PrimLineWidth(float width) {
}

// --- texture copies ---------------------------------------------------------------------------------

int32_t CGxDeviceVulkan::TexCopy(CGxTex* dest, const C2iVector& destPos, const C2iVector& sourcePos, const C2iVector& size, uint32_t level, uint32_t plane) {
    return 0;
}

int32_t CGxDeviceVulkan::TexStretch(CGxTex* dest, const CiRect* destRect, const CiRect* sourceRect, uint32_t level, uint32_t face) {
    return 0;
}

// --- render states, render targets, read-back, queries ----------------------------------------------

// IRsSync calls this for each state whose app value differs from the shadow and then copies the
// value into the shadow itself; the draw reads the app array (the blend mode, the textures, the
// stage ops, the alpha reference, the shaders), so nothing is sent.
void CGxDeviceVulkan::IRsSendToHw(EGxRenderState which) {
}

// The base records the target; the frame keeps drawing on the swapchain image.
void CGxDeviceVulkan::DeviceSetRenderTarget(EGxBuffer buffer, CGxTex* texture, uint32_t plane) {
    CGxDevice::DeviceSetRenderTarget(buffer, texture, plane);
}

// A screenshot gets black pixels of the right size rather than a read past an empty array.
void CGxDeviceVulkan::DeviceReadPixels(CiRect& rect, TSGrowableArray<CImVector>& pixels) {
    int32_t width = rect.maxX - rect.minX;
    int32_t height = rect.maxY - rect.minY;

    if (width <= 0 || height <= 0) {
        pixels.SetCount(0);
        return;
    }

    pixels.SetCount(static_cast<uint32_t>(width) * static_cast<uint32_t>(height));
    memset(pixels.Ptr(), 0, pixels.Count() * sizeof(CImVector));
}

void CGxDeviceVulkan::DeviceReadDepths(CiRect& rect, TSGrowableArray<float>& depths) {
    int32_t width = rect.maxX - rect.minX;
    int32_t height = rect.maxY - rect.minY;

    if (width <= 0 || height <= 0) {
        depths.SetCount(0);
        return;
    }

    depths.SetCount(static_cast<uint32_t>(width) * static_cast<uint32_t>(height));

    for (uint32_t i = 0; i < depths.Count(); i++) {
        depths[i] = 1.0f;
    }
}

// The base keeps the engine's query list; a query never begins (the caps offer none).
void CGxDeviceVulkan::QueryCreate(CGxQuery*& query, EGxQueryType type) {
    CGxDevice::QueryCreate(query, type);
}

void CGxDeviceVulkan::QueryDestroy(CGxQuery*& query) {
    CGxDevice::QueryDestroy(query);
}

bool CGxDeviceVulkan::QueryBegin(CGxQuery* query) {
    return false;
}

bool CGxDeviceVulkan::QueryEnd(CGxQuery* query) {
    return false;
}

bool CGxDeviceVulkan::QueryGetParam(CGxQuery* query, EGxQueryParam param, uint32_t& data) {
    return false;
}

bool CGxDeviceVulkan::QueryGetData(CGxQuery* query, uint32_t& data) {
    return false;
}

// --- stereo: not offered (m_stereoAvailable is 0) ---------------------------------------------------

void CGxDeviceVulkan::StereoSetConvergence(float convergence) {
}

float CGxDeviceVulkan::StereoGetConvergence() {
    return 0.0f;
}

void CGxDeviceVulkan::StereoSetSeparation(float separation) {
}

float CGxDeviceVulkan::StereoGetSeparation() {
    return 0.0f;
}

bool CGxDeviceVulkan::StereoEnabled() {
    return false;
}

void CGxDeviceVulkan::DeviceUnk81(uint32_t a, uint32_t b) {
}

void CGxDeviceVulkan::DeviceUnk82() {
}

void CGxDeviceVulkan::DeviceUnk83(uint32_t a, uint32_t b) {
}
