// The frame: two command buffers per frame slot (a pre-pass for uploads ahead of the draws, the
// main one the draws land in), the swapchain's images and their framebuffers, the render passes,
// the staging ring for this frame's uploads, the acquire, the submit and the present, and the frame
// serials that say which GPU work is done.
#pragma once

#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Memory.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Texture.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Vk.hpp"

#include <cstdint>

// Two frames in flight. A frame is opened by the first Pass(), PrePass(), Outside(), Stage() or
// Present(), and closed by Present(): Present() ends the render pass, submits the slot's two command
// buffers (pre-pass first) with the slot's fence, and presents the acquired swapchain image. Each
// frame has a serial; a resource stamped with the serial of the frame that last named it can be
// asked about through Completed() and WaitSerial(), which is how the pools know when a buffer may be
// rewritten and the textures when an image may be destroyed.
namespace gxvk::vk::frame
{
    /// Frame slots: the CPU records frame N+1 while the GPU runs frame N, and no more, because each
    /// extra one is input latency.
    constexpr uint32_t kFramesInFlight = 2;

    /**
     * The engine's clear. With the frame's render pass open the colour is cleared in place over
     * `rect` (vkCmdClearAttachments), as D3D's Clear clears the viewport rectangle between draws;
     * before the first pass of the frame the colour becomes the pass's own clear (its load op), over
     * the whole image; after a pass was ended by Outside() the pass is resumed and cleared in place.
     * The depth bit is accepted and does nothing: the pass has no depth attachment.
     *
     * @param uint32 mask : bit 0 colour, bit 1 depth
     * @param float[4] colour : r, g, b, a in 0..1
     * @param VkRect2D* rect : the rectangle, inside the pass's extent; null for the whole image
     */
    void Clear(uint32_t mask, const float colour[4], const VkRect2D* rect);

    /**
     * The frame's main command buffer with its render pass open on the acquired swapchain image:
     * what a draw records into. Opens the frame and the pass on first call; after Outside() ended
     * the pass, resumes it with the image's contents kept. Null when no image can be had this frame.
     */
    VkCommandBuffer Pass();

    /**
     * The frame's main command buffer OUTSIDE the render pass, for a transfer into a resource this
     * frame's draws already named: ends the pass if it is open (the next Pass() resumes it). Opens
     * the frame. Null when the frame cannot open.
     */
    VkCommandBuffer Outside();

    /**
     * The frame's pre-pass command buffer: recorded ahead of the frame's draws and submitted before
     * the main buffer, so a transfer recorded here is complete before any draw of the frame. For a
     * resource nothing in this frame has named yet. Opens the frame. Null when the frame cannot open.
     */
    VkCommandBuffer PrePass();

    /**
     * Host-visible bytes for an upload recorded in this frame, from the slot's staging ring (reused
     * once the frame completes) or, past the ring, from a buffer of their own retired with the
     * frame. The caller writes `mapped`; the bytes are flushed at the submit.
     *
     * @param VkDeviceSize bytes : non-zero
     * @param VkBuffer& buffer : receives the buffer the bytes are in
     * @param VkDeviceSize& offset : receives the byte offset in it (a multiple of 16)
     * @param void*& mapped : receives where to write
     * @return bool ok : false when no memory could be had (logged once) or the frame cannot open
     */
    bool Stage(VkDeviceSize bytes, VkBuffer& buffer, VkDeviceSize& offset, void*& mapped);

    /// The render pass draws are recorded in; pipelines are built against it (the resumed pass is
    /// compatible with it). Valid once Pass() has succeeded; changes with PassGeneration().
    VkRenderPass RenderPass();

    /// Bumped whenever RenderPass() is recreated (a swapchain of another format): pipelines built
    /// against an older generation are no longer compatible.
    uint32_t PassGeneration();

    /// The extent of the pass's target: the swapchain's.
    VkExtent2D Extent();

    /// The slot of the open frame (0 .. kFramesInFlight - 1), or of the frame that opens next: what
    /// a per-slot cache (descriptor pools) indexes by.
    uint32_t Slot();

    /// The serial of the open frame, or of the frame that opens next.
    uint64_t Serial();

    /// The highest serial known complete on the GPU.
    uint64_t Completed();

    /// The last serial submitted.
    uint64_t Submitted();

    /// Whether `serial` is the open frame's.
    bool UsedThisFrame(uint64_t serial);

    /**
     * Waits until the frame of `serial` has completed. The open frame cannot be waited for.
     *
     * @return bool done : false for the open frame or a failed wait
     */
    bool WaitSerial(uint64_t serial);

    /**
     * Destroys a buffer once the frame that last named it has completed: at once when it has,
     * else at a later frame's open. Zeroes `buffer`.
     *
     * @param uint64 lastUsed : the serial of the frame that last named it (0: never)
     */
    void RetireBuffer(memory::Buffer& buffer, uint64_t lastUsed);

    /// As RetireBuffer(), for an image (texture::DestroyIdle once its frame has completed).
    void RetireImage(texture::Image& image, uint64_t lastUsed);

    /**
     * Ends the frame: opens the pass when `visible` and nothing opened it (so the clear colour is
     * what the window shows), ends the pass, submits, presents the acquired image.
     *
     * @param bool visible : the window can show a frame (not minimised, non-empty client area)
     * @return bool rebind : the swapchain went stale and the frame has dropped what named it; the
     *         caller rebuilds it with context::Rebind()
     */
    bool Present(bool visible);

    /// A WM_SIZE: the swapchain is stale, rebuild it once at the next Present.
    void RequestRebuild();

    /**
     * Finishes an open frame (presenting it when it acquired an image), waits for the device,
     * drops everything that names the swapchain. Before context::ReleaseWindow() and
     * context::Teardown(). Safe when nothing stands.
     */
    void ReleaseSwapchain();

    /// Everything: ReleaseSwapchain(), the retired resources, the render passes, the staging
    /// ring, the slots and the command pool. Before texture::Shutdown() and memory::Shutdown().
    void Destroy();

    /// Frames presented since the device came up.
    uint64_t Count();

    /// A Vulkan call failed in a way the frame cannot recover from; nothing is presented any more.
    bool Failed();

    struct Stats
    {
        uint64_t frames          = 0;   ///< submitted
        uint64_t presented       = 0;
        uint64_t passes          = 0;   ///< render passes begun (resumes included)
        uint64_t passResumes     = 0;   ///< passes ended by Outside() and begun again
        uint64_t clearsInPass    = 0;   ///< clears recorded inside an open pass
        uint64_t noImage         = 0;   ///< Pass() calls with no swapchain image to be had
        uint64_t serialWaits     = 0;   ///< WaitSerial() calls that blocked
        uint64_t retired         = 0;   ///< buffers and images destroyed through Retire*()
        uint64_t retireLeaked    = 0;   ///< neither destroyed nor listed (list full)
        uint32_t retirePeak      = 0;   ///< the longest waiting list
        uint64_t stagedBytes     = 0;   ///< handed out by Stage()
        uint64_t stagedDedicated = 0;   ///< Stage() requests answered with a buffer of their own
        uint32_t stagingChunks   = 0;   ///< ring chunks alive, all slots
        uint32_t stagingPeak     = 0;
    };
    void GetStats(Stats& out);
}
