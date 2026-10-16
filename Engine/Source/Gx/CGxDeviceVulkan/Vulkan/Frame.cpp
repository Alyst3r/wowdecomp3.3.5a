// The frame: opening a frame slot and its render pass, the clear, the submit and the present, the
// frame serials and the retirement of buffers and images by serial.
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Frame/Internal.hpp"

using namespace gxvk::vk::frame::detail;

namespace gxvk::vk::frame::detail
{
    /// Opens the frame on the next slot: waits for the GPU to be done with that slot's last frame,
    /// destroys what that completion released, resets the slot's staging ring, begins its two
    /// command buffers.
    bool EnsureFrame()
    {
        if (s.failed) return false;   // a hard failure stops the frame for good, open or not
        if (s.open) return true;
        const context::Handles* h = context::Get();
        if (!h || !h->device || !h->dapi) return false;
        if (!EnsureDeviceObjects(*h)) return false;

        const loader::Device& d    = *s.dapi;
        FrameSlot&            slot = s.slots[s.fi];
        VkResult r = d.WaitForFences(s.device, 1, &slot.fence, VK_TRUE, UINT64_MAX);   // the GPU two frames behind
        if (r != VK_SUCCESS)
        {
            Fail("vkWaitForFences (frame slot)", r);
            return false;
        }
        // One queue, submitted in order: this slot's last frame finishing proves every earlier one did.
        if (slot.serial > s.completed) s.completed = slot.serial;
        Poll();   // the other slot may have finished as well
        CollectRetired(false);
        StagingFrameBegin(slot);

        s.serial = s.serial + 1;

        VkCommandBufferBeginInfo begin = {};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        r = d.BeginCommandBuffer(slot.pre, &begin);
        if (r == VK_SUCCESS) r = d.BeginCommandBuffer(slot.cmd, &begin);
        if (r != VK_SUCCESS)
        {
            Fail("vkBeginCommandBuffer", r);
            return false;
        }
        s.open     = true;
        s.passOpen = false;
        s.resume   = false;
        s.acquired = false;
        s.noImage  = false;
        return true;
    }

    /// Begins the render pass on the acquired image: the frame's first pass clears it to the clear
    /// colour, a resumed pass loads what the earlier one stored.
    bool BeginPass()
    {
        if (s.passOpen) return true;
        if (!Acquire()) return false;
        const loader::Device& d = *s.dapi;
        VkClearValue clear = {};
        for (uint32_t i = 0; i < 4; ++i) clear.color.float32[i] = s.clear[i];

        VkRenderPassBeginInfo begin = {};
        begin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin.renderPass        = s.resume ? s.passResume : s.pass;
        begin.framebuffer       = s.framebuffers[s.image];
        begin.renderArea.extent = s.extent;
        begin.clearValueCount   = 1;
        begin.pClearValues      = &clear;
        d.CmdBeginRenderPass(s.slots[s.fi].cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
        s.passOpen = true;
        ++s.stats.passes;
        if (s.resume) ++s.stats.passResumes;
        return true;
    }

    void EndPass()
    {
        if (!s.passOpen) return;
        s.dapi->CmdEndRenderPass(s.slots[s.fi].cmd);
        s.passOpen = false;
    }
}

namespace
{
    /// Ends, submits and (if an image was acquired) presents the open frame.
    void FinishFrame()
    {
        if (!s.open) return;
        const context::Handles* h    = context::Get();
        const loader::Device&   d    = *s.dapi;
        FrameSlot&              slot = s.slots[s.fi];

        EndPass();
        StagingFrameEnd(slot);

        VkResult r = d.EndCommandBuffer(slot.pre);
        if (r == VK_SUCCESS) r = d.EndCommandBuffer(slot.cmd);
        if (r != VK_SUCCESS) return Fail("vkEndCommandBuffer", r);

        // Reset right before the submit that signals it: no early return can then leave it unsignalled.
        r = d.ResetFences(s.device, 1, &slot.fence);
        if (r != VK_SUCCESS) return Fail("vkResetFences", r);

        // The pre-pass first, then the main buffer: the barriers the uploads end with cover the
        // commands that follow them on the queue. The pass's first use of the swapchain image is its
        // colour output, so that stage waits for the acquire.
        const VkCommandBuffer      buffers[2] = { slot.pre, slot.cmd };
        const VkPipelineStageFlags wait       = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit = {};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 2;
        submit.pCommandBuffers    = buffers;
        if (s.acquired)
        {
            submit.waitSemaphoreCount   = 1;
            submit.pWaitSemaphores      = &slot.imageAvailable;
            submit.pWaitDstStageMask    = &wait;
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores    = &s.renderFinished[s.image];
        }
        r = d.QueueSubmit(h->graphicsQueue, 1, &submit, slot.fence);
        if (r != VK_SUCCESS) return Fail("vkQueueSubmit", r);
        slot.serial = s.serial;
        s.submitted = s.serial;
        ++s.stats.frames;

        if (s.acquired)
        {
            VkPresentInfoKHR present = {};
            present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores    = &s.renderFinished[s.image];
            present.swapchainCount     = 1;
            present.pSwapchains        = &h->swapchain;
            present.pImageIndices      = &s.image;
            r = d.QueuePresentKHR(h->presentQueue, &present);
            ++s.presentedCount;
            ++s.stats.presented;
            if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) s.rebuild = true;
            else if (r != VK_SUCCESS) Fail("vkQueuePresentKHR", r);
            if (s.presentedCount == 1)
                Write(Debug, "vulkan: frame: first frame presented (%ux%u, %u images)", s.extent.width, s.extent.height, s.imageCount);
        }

        s.open = false;
        s.fi   = (s.fi + 1) % kFramesInFlight;
    }

    /// vkCmdClearAttachments over `rect` (clamped to the attachment) or the whole image.
    void ClearInPass(const float colour[4], const VkRect2D* rect)
    {
        VkClearAttachment a = {};
        a.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        a.colorAttachment = 0;
        for (uint32_t i = 0; i < 4; ++i) a.clearValue.color.float32[i] = colour[i];
        VkClearRect r = {};
        r.rect.extent = s.extent;
        if (rect)
        {
            int32_t x0 = rect->offset.x < 0 ? 0 : rect->offset.x;
            int32_t y0 = rect->offset.y < 0 ? 0 : rect->offset.y;
            int64_t x1 = static_cast<int64_t>(rect->offset.x) + rect->extent.width;
            int64_t y1 = static_cast<int64_t>(rect->offset.y) + rect->extent.height;
            if (x1 > s.extent.width) x1 = s.extent.width;
            if (y1 > s.extent.height) y1 = s.extent.height;
            if (x1 <= x0 || y1 <= y0) return;
            r.rect.offset.x      = x0;
            r.rect.offset.y      = y0;
            r.rect.extent.width  = static_cast<uint32_t>(x1 - x0);
            r.rect.extent.height = static_cast<uint32_t>(y1 - y0);
        }
        r.layerCount = 1;
        s.dapi->CmdClearAttachments(s.slots[s.fi].cmd, 1, &a, 1, &r);
        ++s.stats.clearsInPass;
    }
}

namespace gxvk::vk::frame
{
    void Clear(uint32_t mask, const float colour[4], const VkRect2D* rect)
    {
        if (!(mask & 1u)) return;
        if (!s.passOpen)
        {
            if (!s.resume)
            {
                // Before the frame's first pass: its load-op clear.
                for (uint32_t i = 0; i < 4; ++i) s.clear[i] = colour[i];
                return;
            }
            // A pass was ended by Outside(): the resumed pass loads, so the clear is recorded in it.
            if (!Pass()) return;
        }
        ClearInPass(colour, rect);
    }

    VkCommandBuffer Pass()
    {
        if (!EnsureFrame()) return nullptr;
        if (!BeginPass()) return nullptr;
        return s.slots[s.fi].cmd;
    }

    VkRenderPass RenderPass() { return s.pass; }

    uint32_t PassGeneration() { return s.passGen; }

    VkExtent2D Extent() { return s.extent; }

    uint32_t Slot() { return s.fi; }

    uint64_t Serial() { return s.open ? s.serial : s.serial + 1; }

    uint64_t Completed() { return s.completed; }

    uint64_t Submitted() { return s.submitted; }

    bool UsedThisFrame(uint64_t serial) { return s.open && serial == s.serial; }

    bool WaitSerial(uint64_t serial)
    {
        if (serial <= s.completed) return true;
        if (serial > s.submitted || !s.dapi) return false;   // the open frame: nothing to wait for yet
        for (FrameSlot& slot : s.slots)
        {
            // The slot that submitted `serial` still holds it; a slot that moved past it waited it.
            if (slot.serial < serial) continue;
            const VkResult r = s.dapi->WaitForFences(s.device, 1, &slot.fence, VK_TRUE, UINT64_MAX);
            if (r != VK_SUCCESS)
            {
                Fail("vkWaitForFences (a retired serial)", r);
                return false;
            }
            ++s.stats.serialWaits;
            if (slot.serial > s.completed) s.completed = slot.serial;
            break;
        }
        Poll();
        return serial <= s.completed;
    }

    void RetireBuffer(memory::Buffer& buffer, uint64_t lastUsed)
    {
        if (buffer.handle == VK_NULL_HANDLE && buffer.block.memory == VK_NULL_HANDLE) return;
        Retired e;
        e.buffer = buffer;
        Retire(e, lastUsed);
        buffer = memory::Buffer();
    }

    void RetireImage(texture::Image& image, uint64_t lastUsed)
    {
        if (image.handle == VK_NULL_HANDLE) return;
        Retired e;
        e.image = image;
        Retire(e, lastUsed);
        image = texture::Image();
    }

    bool Present(bool visible)
    {
        if (s.failed) return false;
        if (!EnsureFrame()) return false;

        // A visible frame with no draw and no clear still shows the clear colour: the pass is what
        // puts it on the image.
        if (visible) BeginPass();

        FinishFrame();
        memory::Tick();

        if (s.rebuild && !s.failed)
        {
            // Out of date, suboptimal, or a WM_SIZE: rebuilt once, here, between frames. The caller
            // does the context's half (Rebind) with the window it owns.
            s.rebuild = false;
            if (s.dapi) s.dapi->DeviceWaitIdle(s.device);
            s.completed = s.submitted;
            DropImageObjects();
            return true;
        }
        return false;
    }

    void RequestRebuild()
    {
        // Only once objects were built for a swapchain: before that the first Pass() builds them for
        // the swapchain as it is, and a swapchain that went stale in between is caught by the acquire
        // (OUT_OF_DATE) rather than rebuilt blind.
        if (s.swapGen != UINT32_MAX) s.rebuild = true;
    }

    void ReleaseSwapchain()
    {
        if (!s.dapi || !s.device || !context::Live()) return;
        FinishFrame();
        s.dapi->DeviceWaitIdle(s.device);
        s.completed = s.submitted;
        CollectRetired(true);
        DropImageObjects();
        s.rebuild = false;
    }

    void Destroy()
    {
        if (!s.dapi || !s.device) return;
        if (context::Live())
        {
            ReleaseSwapchain();
            DestroyDeviceObjects();
        }
        delete[] s.retired;
        s = State();
    }

    uint64_t Count() { return s.presentedCount; }

    bool Failed() { return s.failed; }

    void GetStats(Stats& out) { out = s.stats; }
}
