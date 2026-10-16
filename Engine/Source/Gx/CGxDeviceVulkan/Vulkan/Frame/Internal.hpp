// The frame's shared internals: its constants, its state and the helpers every part of Vulkan/Frame/ uses.
#pragma once

#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Frame.hpp"

#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Context.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Loader.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Log.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Memory.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Texture.hpp"
#include "Engine/Source/Gx/CGxDeviceVulkan/Vulkan/Vk.hpp"

#include <cstdint>

// The frame lives in Vulkan/Frame/, one file per responsibility, over this one header. Nothing here
// is visible outside it: the public face is Vulkan/Frame.hpp.
//
//   Objects   the device's own objects (the command pool, the slots' buffers, fences, semaphores)
//             and the retirement list
//   Images    the swapchain-derived objects (views, the render passes, framebuffers), the acquire
//   Uploads   the pre-pass and outside-the-pass command buffers, the staging ring
//   Frame.cpp opening a frame and its pass, the clear, the submit and the present
namespace gxvk::vk::frame::detail
{
    namespace context = gxvk::vk::context;
    namespace loader  = gxvk::vk::loader;
    namespace memory  = gxvk::vk::memory;
    namespace texture = gxvk::vk::texture;

    using gxvk::log::Write;
    using gxvk::log::Debug;
    using gxvk::log::Info;
    using gxvk::log::Warn;
    using gxvk::log::Error;
    using gxvk::vk::ResultName;
    using gxvk::vk::frame::kFramesInFlight;

    /// A swapchain with more images than this is refused rather than half-tracked.
    constexpr uint32_t kMaxImages = 8;

    /// The retirement list: starts at kRetiredFirst entries and doubles up to kMaxRetired. Full
    /// means leaked (counted), never destroyed early.
    constexpr uint32_t kRetiredFirst = 1024;
    constexpr uint32_t kMaxRetired   = 65536;

    /// The staging ring: chunks of kStagingChunk bytes per slot, up to kMaxStagingChunks; a request
    /// past them, or larger than a chunk, gets a buffer of its own for the frame.
    constexpr VkDeviceSize kStagingChunk     = 8u * 1024u * 1024u;
    constexpr uint32_t     kMaxStagingChunks = 64;
    constexpr uint32_t     kMaxDedicated     = 256;     ///< dedicated staging buffers one frame may hold
    constexpr VkDeviceSize kStagingAlign     = 16;      ///< every Stage() offset: a multiple of any texel block

    /// A ring chunk kept past the frame's need for this many frames is given back.
    constexpr uint32_t kStagingTrimFrames = 600;

    struct FrameSlot
    {
        VkCommandBuffer cmd            = nullptr;   ///< the main buffer: the render pass and the draws
        VkCommandBuffer pre            = nullptr;   ///< the pre-pass buffer: uploads ahead of the draws
        VkSemaphore     imageAvailable = VK_NULL_HANDLE;
        VkFence         fence          = VK_NULL_HANDLE;
        uint64_t        serial         = 0;     ///< the frame this slot last submitted

        // --- the staging ring -----------------------------------------------------------------------
        memory::Buffer chunks[kMaxStagingChunks];
        uint32_t       chunkCount = 0;
        uint32_t       chunkUsed  = 0;          ///< chunks written this frame (the current one included)
        VkDeviceSize   cursor     = 0;          ///< next free byte of the current chunk
        uint32_t       idleFrames = 0;          ///< frames in a row with a spare chunk
        memory::Buffer dedicated[kMaxDedicated];
        uint32_t       dedicatedCount = 0;
    };

    struct Retired
    {
        memory::Buffer buffer;
        texture::Image image;
        uint64_t       serial = 0;
    };

    struct State
    {
        VkDevice              device = nullptr;
        const loader::Device* dapi   = nullptr;
        VkCommandPool         pool   = VK_NULL_HANDLE;
        FrameSlot             slots[kFramesInFlight];

        // --- swapchain-derived ----------------------------------------------------------------------
        uint32_t      swapGen    = UINT32_MAX;
        uint32_t      imageCount = 0;
        VkExtent2D    extent     = { 0, 0 };
        VkFormat      format     = VK_FORMAT_UNDEFINED;
        VkImage       images[kMaxImages]         = {};
        VkImageView   views[kMaxImages]          = {};
        VkFramebuffer framebuffers[kMaxImages]   = {};
        VkSemaphore   renderFinished[kMaxImages] = {};   ///< per IMAGE: only re-acquiring an image proves its present consumed it
        VkFence       imagesInFlight[kMaxImages] = {};

        /// The render passes: one colour attachment, PRESENT_SRC at the end. `pass` CLEARs on load
        /// from UNDEFINED (the frame's first pass); `passResume` LOADs from PRESENT_SRC (a pass
        /// begun again after Outside() ended it). Compatible with each other, so one pipeline
        /// serves both. Keyed on the swapchain format they were made for; remade, with passGen
        /// bumped, when a rebuilt swapchain changes format.
        VkRenderPass pass       = VK_NULL_HANDLE;
        VkRenderPass passResume = VK_NULL_HANDLE;
        VkFormat     passFormat = VK_FORMAT_UNDEFINED;
        uint32_t     passGen    = 0;

        // --- the frame being recorded ---------------------------------------------------------------
        bool     open      = false;
        bool     passOpen  = false;
        bool     resume    = false;  ///< a pass was begun and ended this frame: the next one LOADs
        uint32_t fi        = 0;
        uint64_t serial    = 0;      ///< of the open frame; the last one opened otherwise
        uint64_t completed = 0;
        uint64_t submitted = 0;
        bool     acquired  = false;
        bool     noImage   = false;  ///< the swapchain image could not be had this frame
        uint32_t image     = 0;

        bool     rebuild   = false;
        bool     failed    = false;
        uint64_t presentedCount = 0;

        /// The colour the frame's first pass clears to on load (Clear() before any pass is open).
        float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

        Retired* retired      = nullptr;   ///< heap, retiredCap entries
        uint32_t retiredCount = 0;
        uint32_t retiredCap   = 0;

        bool loggedStagingFail = false;

        gxvk::vk::frame::Stats stats;
    };
    extern State s;

    // Frame.cpp
    bool EnsureFrame();
    bool BeginPass();
    void EndPass();

    // Objects.cpp
    void Fail(const char* what, VkResult r);
    bool EnsureDeviceObjects(const context::Handles& h);
    void DestroyDeviceObjects();
    void Poll();
    void CollectRetired(bool all);
    void Retire(Retired& entry, uint64_t lastUsed);

    // Images.cpp
    bool EnsurePass(VkFormat format);
    bool EnsureImageObjects(const context::Handles& h);
    void DropImageObjects();
    bool Acquire();

    // Uploads.cpp
    void StagingFrameBegin(FrameSlot& slot);
    void StagingFrameEnd(FrameSlot& slot);
    void StagingDestroy(FrameSlot& slot);
}
