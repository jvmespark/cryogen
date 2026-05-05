#pragma once
//
// cryogen/render/swapchain.hpp
// Manages VkSwapchainKHR + per-image views, depth target, and frames-in-flight
// synchronisation. Handles invalidation on window resize.
//

#include <vector>

#include <vulkan/vulkan.h>

#include "cryogen/core/types.hpp"

namespace cryogen::render {

class VulkanContext;

struct FrameSync {
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence     in_flight       = VK_NULL_HANDLE;
};

class Swapchain {
public:
    static constexpr u32 kFramesInFlight = 2;

    Swapchain(VulkanContext& ctx, u32 desired_w, u32 desired_h);
    ~Swapchain();

    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void recreate(u32 w, u32 h);

    // Frame lifecycle ----------------------------------------------------
    // Returns the swapchain image index, or UINT32_MAX if a recreate is needed.
    u32 acquire_next_image();
    // Submits and presents the previously-acquired image.
    void submit_and_present(VkCommandBuffer cb, u32 image_index);

    // Accessors ----------------------------------------------------------
    VkSwapchainKHR handle()        const { return swapchain_; }
    VkFormat       color_format()  const { return color_format_; }
    VkFormat       depth_format()  const { return depth_format_; }
    VkExtent2D     extent()        const { return extent_; }
    u32            image_count()   const { return static_cast<u32>(images_.size()); }

    VkImage        color_image(u32 i) const { return images_[i]; }
    VkImageView    color_view(u32 i)  const { return image_views_[i]; }
    VkImageView    depth_view()       const { return depth_view_; }
    u32            current_frame()    const { return frame_; }

private:
    void create();
    void destroy();
    void create_depth();
    void destroy_depth();

    VulkanContext&            ctx_;
    VkSwapchainKHR            swapchain_   = VK_NULL_HANDLE;
    VkFormat                  color_format_= VK_FORMAT_UNDEFINED;
    VkFormat                  depth_format_= VK_FORMAT_D32_SFLOAT;
    VkExtent2D                extent_      = {0,0};

    std::vector<VkImage>      images_;
    std::vector<VkImageView>  image_views_;

    VkImage                   depth_image_  = VK_NULL_HANDLE;
    VkDeviceMemory            depth_memory_ = VK_NULL_HANDLE;
    VkImageView               depth_view_   = VK_NULL_HANDLE;

    FrameSync                 sync_[kFramesInFlight]{};
    u32                       frame_       = 0;
    u32                       image_index_ = 0;
    u32                       desired_w_   = 0;
    u32                       desired_h_   = 0;
};

} // namespace cryogen::render
