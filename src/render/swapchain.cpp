//
// cryogen/render/swapchain.cpp
//
#include "cryogen/render/swapchain.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

#include "cryogen/core/log.hpp"
#include "cryogen/render/vulkan_context.hpp"

namespace cryogen::render {

Swapchain::Swapchain(VulkanContext& ctx, u32 desired_w, u32 desired_h)
    : ctx_(ctx), desired_w_(desired_w), desired_h_(desired_h) {
    create();
    create_depth();

    // Per-frame sync objects.
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo     fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (u32 i = 0; i < kFramesInFlight; ++i) {
        CRYOGEN_VK_CHECK(vkCreateSemaphore(ctx_.device(), &sci, nullptr, &sync_[i].image_available));
        CRYOGEN_VK_CHECK(vkCreateSemaphore(ctx_.device(), &sci, nullptr, &sync_[i].render_finished));
        CRYOGEN_VK_CHECK(vkCreateFence    (ctx_.device(), &fci, nullptr, &sync_[i].in_flight));
    }
}

Swapchain::~Swapchain() {
    vkDeviceWaitIdle(ctx_.device());
    for (u32 i = 0; i < kFramesInFlight; ++i) {
        if (sync_[i].image_available) vkDestroySemaphore(ctx_.device(), sync_[i].image_available, nullptr);
        if (sync_[i].render_finished) vkDestroySemaphore(ctx_.device(), sync_[i].render_finished, nullptr);
        if (sync_[i].in_flight)       vkDestroyFence    (ctx_.device(), sync_[i].in_flight,       nullptr);
    }
    destroy_depth();
    destroy();
}

void Swapchain::recreate(u32 w, u32 h) {
    desired_w_ = w;
    desired_h_ = h;
    vkDeviceWaitIdle(ctx_.device());
    destroy_depth();
    destroy();
    create();
    create_depth();
}

void Swapchain::create() {
    VkSurfaceCapabilitiesKHR caps{};
    CRYOGEN_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx_.physical_device(), ctx_.surface(), &caps));

    u32 fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx_.physical_device(), ctx_.surface(), &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx_.physical_device(), ctx_.surface(), &fmt_count, formats.data());

    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f; break;
        }
    }
    color_format_ = chosen.format;

    u32 pm_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx_.physical_device(), ctx_.surface(), &pm_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(pm_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx_.physical_device(), ctx_.surface(), &pm_count, present_modes.data());

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed
    for (auto m : present_modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { present_mode = m; break; }

    if (caps.currentExtent.width != std::numeric_limits<u32>::max()) {
        extent_ = caps.currentExtent;
    } else {
        extent_.width  = std::clamp(desired_w_, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent_.height = std::clamp(desired_h_, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    u32 image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) image_count = std::min(image_count, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface          = ctx_.surface();
    ci.minImageCount    = image_count;
    ci.imageFormat      = chosen.format;
    ci.imageColorSpace  = chosen.colorSpace;
    ci.imageExtent      = extent_;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    u32 qfs[2] = { ctx_.graphics_family(), ctx_.present_family() };
    if (qfs[0] != qfs[1]) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = qfs;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    ci.preTransform   = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = present_mode;
    ci.clipped        = VK_TRUE;

    CRYOGEN_VK_CHECK(vkCreateSwapchainKHR(ctx_.device(), &ci, nullptr, &swapchain_));

    u32 actual = 0;
    vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &actual, nullptr);
    images_.resize(actual);
    vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &actual, images_.data());

    image_views_.resize(actual);
    for (u32 i = 0; i < actual; ++i) {
        VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        iv.image    = images_[i];
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format   = chosen.format;
        iv.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        CRYOGEN_VK_CHECK(vkCreateImageView(ctx_.device(), &iv, nullptr, &image_views_[i]));
    }

    CRYOGEN_INFO("render", "Swapchain ", extent_.width, "x", extent_.height,
                 " images=", actual,
                 " present_mode=", (present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO"));
}

void Swapchain::destroy() {
    for (auto v : image_views_) vkDestroyImageView(ctx_.device(), v, nullptr);
    image_views_.clear();
    images_.clear();
    if (swapchain_) {
        vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void Swapchain::create_depth() {
    VkImageCreateInfo ic{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ic.imageType   = VK_IMAGE_TYPE_2D;
    ic.format      = depth_format_;
    ic.extent      = {extent_.width, extent_.height, 1};
    ic.mipLevels   = 1;
    ic.arrayLayers = 1;
    ic.samples     = VK_SAMPLE_COUNT_1_BIT;
    ic.tiling      = VK_IMAGE_TILING_OPTIMAL;
    ic.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ic.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ic.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CRYOGEN_VK_CHECK(vkCreateImage(ctx_.device(), &ic, nullptr, &depth_image_));

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx_.device(), depth_image_, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = ctx_.find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CRYOGEN_VK_CHECK(vkAllocateMemory(ctx_.device(), &ai, nullptr, &depth_memory_));
    CRYOGEN_VK_CHECK(vkBindImageMemory(ctx_.device(), depth_image_, depth_memory_, 0));

    VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    iv.image    = depth_image_;
    iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
    iv.format   = depth_format_;
    iv.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    CRYOGEN_VK_CHECK(vkCreateImageView(ctx_.device(), &iv, nullptr, &depth_view_));
}

void Swapchain::destroy_depth() {
    if (depth_view_)   { vkDestroyImageView(ctx_.device(), depth_view_, nullptr); depth_view_ = VK_NULL_HANDLE; }
    if (depth_image_)  { vkDestroyImage(ctx_.device(), depth_image_, nullptr);    depth_image_ = VK_NULL_HANDLE; }
    if (depth_memory_) { vkFreeMemory(ctx_.device(), depth_memory_, nullptr);     depth_memory_ = VK_NULL_HANDLE; }
}

u32 Swapchain::acquire_next_image() {
    vkWaitForFences(ctx_.device(), 1, &sync_[frame_].in_flight, VK_TRUE, UINT64_MAX);

    u32 idx = 0;
    VkResult r = vkAcquireNextImageKHR(
        ctx_.device(), swapchain_, UINT64_MAX,
        sync_[frame_].image_available, VK_NULL_HANDLE, &idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        return UINT32_MAX;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        CRYOGEN_ERROR("render", "vkAcquireNextImageKHR => ", vk_result_string(r));
        return UINT32_MAX;
    }
    vkResetFences(ctx_.device(), 1, &sync_[frame_].in_flight);
    image_index_ = idx;
    return idx;
}

void Swapchain::submit_and_present(VkCommandBuffer cb, u32 image_index) {
    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &sync_[frame_].image_available;
    si.pWaitDstStageMask    = &wait_stages;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &sync_[frame_].render_finished;
    CRYOGEN_VK_CHECK(vkQueueSubmit(ctx_.graphics_queue(), 1, &si, sync_[frame_].in_flight));

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &sync_[frame_].render_finished;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchain_;
    pi.pImageIndices      = &image_index;
    VkResult pr = vkQueuePresentKHR(ctx_.present_queue(), &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
        recreate(desired_w_, desired_h_);
    } else if (pr != VK_SUCCESS) {
        CRYOGEN_ERROR("render", "vkQueuePresentKHR => ", vk_result_string(pr));
    }

    frame_ = (frame_ + 1) % kFramesInFlight;
}

} // namespace cryogen::render
