// The swapchain-derived objects: the image views, the render pass, the framebuffers, the acquire.
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Frame/Internal.hpp"

namespace gxvk::vk::frame::detail
{
    /**
     * One of the two render passes for a swapchain format: one colour attachment, stored, ending in
     * PRESENT_SRC (what the present reads).
     *
     *   first  : CLEAR on load from UNDEFINED (the acquired image's contents are not kept). The
     *            external dependency orders the attachment's write after the acquire: the submit
     *            waits for the image-available semaphore at COLOR_ATTACHMENT_OUTPUT, and the
     *            dependency makes the layout transition happen after that wait rather than at the
     *            top of the pipe.
     *   resume : LOAD from PRESENT_SRC, for a pass begun again after Outside() ended it in the
     *            same frame. The dependency makes the earlier pass's stores visible to the load.
     *
     * The two differ only in load op and layouts, so they are compatible: one pipeline and one
     * framebuffer serve both.
     */
    static bool CreatePass(VkFormat format, bool resume, VkRenderPass& out)
    {
        VkAttachmentDescription a = {};
        a.format         = format;
        a.samples        = VK_SAMPLE_COUNT_1_BIT;
        a.loadOp         = resume ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        a.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout  = resume ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colour = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription sub = {};
        sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments    = &colour;

        VkSubpassDependency dep = {};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = resume ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | (resume ? VK_ACCESS_COLOR_ATTACHMENT_READ_BIT : 0);

        VkRenderPassCreateInfo info = {};
        info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments    = &a;
        info.subpassCount    = 1;
        info.pSubpasses      = &sub;
        info.dependencyCount = 1;
        info.pDependencies   = &dep;

        const VkResult r = s.dapi->CreateRenderPass(s.device, &info, nullptr, &out);
        if (r != VK_SUCCESS)
        {
            out = VK_NULL_HANDLE;
            Fail("vkCreateRenderPass", r);
            return false;
        }
        return true;
    }

    /// The two render passes for the swapchain's format. Remade, with the generation bumped, when
    /// the format changes; kept otherwise.
    bool EnsurePass(VkFormat format)
    {
        if (s.pass != VK_NULL_HANDLE && s.passResume != VK_NULL_HANDLE && s.passFormat == format) return true;
        const loader::Device& d = *s.dapi;
        if (s.pass != VK_NULL_HANDLE) d.DestroyRenderPass(s.device, s.pass, nullptr);
        if (s.passResume != VK_NULL_HANDLE) d.DestroyRenderPass(s.device, s.passResume, nullptr);
        s.pass       = VK_NULL_HANDLE;
        s.passResume = VK_NULL_HANDLE;

        if (!CreatePass(format, false, s.pass)) return false;
        if (!CreatePass(format, true, s.passResume))
        {
            d.DestroyRenderPass(s.device, s.pass, nullptr);
            s.pass = VK_NULL_HANDLE;
            return false;
        }
        s.passFormat = format;
        ++s.passGen;
        return true;
    }

    /// Destroys what names the swapchain. The caller has waited for the device.
    void DropImageObjects()
    {
        if (!s.dapi || !s.device) return;
        const loader::Device& d = *s.dapi;
        for (uint32_t i = 0; i < kMaxImages; ++i)
        {
            if (s.framebuffers[i] != VK_NULL_HANDLE) d.DestroyFramebuffer(s.device, s.framebuffers[i], nullptr);
            if (s.views[i] != VK_NULL_HANDLE) d.DestroyImageView(s.device, s.views[i], nullptr);
            if (s.renderFinished[i] != VK_NULL_HANDLE) d.DestroySemaphore(s.device, s.renderFinished[i], nullptr);
            s.framebuffers[i]   = VK_NULL_HANDLE;
            s.views[i]          = VK_NULL_HANDLE;
            s.renderFinished[i] = VK_NULL_HANDLE;
            s.imagesInFlight[i] = VK_NULL_HANDLE;
            s.images[i]         = VK_NULL_HANDLE;
        }
        s.imageCount = 0;
        s.extent     = VkExtent2D{ 0, 0 };
        s.swapGen    = UINT32_MAX;
    }

    /// The image list, the per-image views, framebuffers and semaphores for the swapchain that stands now.
    bool EnsureImageObjects(const context::Handles& h)
    {
        if (s.swapGen == context::SwapchainGeneration() && s.imageCount) return true;
        const loader::Device& d = *h.dapi;
        if (s.imageCount)
        {
            // Earlier frames may still be presenting from these images.
            d.DeviceWaitIdle(s.device);
            s.completed = s.submitted;
            DropImageObjects();
        }
        if (!EnsurePass(h.format)) return false;
        s.format = h.format;

        uint32_t count = 0;
        VkResult r = d.GetSwapchainImagesKHR(s.device, h.swapchain, &count, nullptr);
        if (r != VK_SUCCESS || count == 0 || count > kMaxImages)
        {
            Write(Error, "vulkan: frame: the swapchain has %u image(s) (%s); at most %u are tracked", count, ResultName(r), kMaxImages);
            s.failed = true;
            return false;
        }
        r = d.GetSwapchainImagesKHR(s.device, h.swapchain, &count, s.images);
        if (r != VK_SUCCESS)
        {
            Fail("vkGetSwapchainImagesKHR", r);
            return false;
        }

        VkSemaphoreCreateInfo sem = {};
        sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (uint32_t i = 0; i < count; ++i)
        {
            VkImageViewCreateInfo view = {};
            view.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view.image    = s.images[i];
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format   = h.format;
            view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view.subresourceRange.levelCount = 1;
            view.subresourceRange.layerCount = 1;
            r = d.CreateImageView(s.device, &view, nullptr, &s.views[i]);
            if (r != VK_SUCCESS)
            {
                s.views[i] = VK_NULL_HANDLE;
                Fail("vkCreateImageView", r);
                return false;
            }

            VkFramebufferCreateInfo fb = {};
            fb.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb.renderPass      = s.pass;
            fb.attachmentCount = 1;
            fb.pAttachments    = &s.views[i];
            fb.width           = h.extent.width;
            fb.height          = h.extent.height;
            fb.layers          = 1;
            r = d.CreateFramebuffer(s.device, &fb, nullptr, &s.framebuffers[i]);
            if (r != VK_SUCCESS)
            {
                s.framebuffers[i] = VK_NULL_HANDLE;
                Fail("vkCreateFramebuffer", r);
                return false;
            }

            // Per IMAGE, not per frame: only re-acquiring an image proves its present consumed it.
            r = d.CreateSemaphore(s.device, &sem, nullptr, &s.renderFinished[i]);
            if (r != VK_SUCCESS)
            {
                s.renderFinished[i] = VK_NULL_HANDLE;
                Fail("vkCreateSemaphore (render finished)", r);
                return false;
            }
        }
        s.imageCount = count;
        s.extent     = h.extent;
        s.swapGen    = context::SwapchainGeneration();
        return true;
    }

    /// The acquire, once a frame.
    bool Acquire()
    {
        if (s.acquired) return true;
        if (s.noImage) return false;
        const context::Handles* h = context::Get();
        FrameSlot&              slot = s.slots[s.fi];
        if (!h || h->swapchain == VK_NULL_HANDLE || h->extent.width == 0 || h->extent.height == 0)
        {
            // No swapchain (its creation declined at bring-up, a mid-resize zero extent): asked for
            // again after this frame's submit, through the caller's Rebind, as long as the window is
            // visible enough to be presented into.
            s.noImage = true;
            if (h) s.rebuild = true;
            ++s.stats.noImage;
            return false;
        }
        if (!EnsureImageObjects(*h))
        {
            s.noImage = true;
            ++s.stats.noImage;
            return false;
        }
        uint32_t image = 0;
        VkResult r = s.dapi->AcquireNextImageKHR(s.device, h->swapchain, UINT64_MAX, slot.imageAvailable, VK_NULL_HANDLE, &image);
        if (r == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Nothing acquired, the semaphore untouched: this frame presents nothing, the swapchain
            // is rebuilt after its submit.
            s.noImage = true;
            s.rebuild = true;
            ++s.stats.noImage;
            return false;
        }
        if (r == VK_SUBOPTIMAL_KHR) s.rebuild = true;   // acquired: finish the frame, rebuild after
        else if (r != VK_SUCCESS)
        {
            Fail("vkAcquireNextImageKHR", r);
            return false;
        }
        if (image >= s.imageCount)
        {
            s.failed = true;
            return false;
        }
        if (s.imagesInFlight[image] != VK_NULL_HANDLE && s.imagesInFlight[image] != slot.fence)
            s.dapi->WaitForFences(s.device, 1, &s.imagesInFlight[image], VK_TRUE, UINT64_MAX);
        s.imagesInFlight[image] = slot.fence;
        s.image    = image;
        s.acquired = true;
        return true;
    }
}
