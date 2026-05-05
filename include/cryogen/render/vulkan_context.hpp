#pragma once
//
// cryogen/render/vulkan_context.hpp
// Vulkan instance + physical device + logical device + queues + allocator.
//

#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "cryogen/core/types.hpp"

namespace cryogen::core { class Window; }

namespace cryogen::render {

struct QueueFamilyIndices {
    std::optional<u32> graphics;
    std::optional<u32> present;
    std::optional<u32> compute;
    std::optional<u32> transfer;
    bool complete() const {
        return graphics.has_value() && present.has_value();
    }
};

struct DeviceFeaturesRequested {
    bool ray_tracing = true;       // VK_KHR_ray_tracing_pipeline + accel structure
    bool buffer_device_address = true;
    bool descriptor_indexing = true;
};

class VulkanContext {
public:
    VulkanContext(core::Window& window, const DeviceFeaturesRequested& features);
    ~VulkanContext();

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Handles ------------------------------------------------------------
    VkInstance       instance()        const { return instance_; }
    VkPhysicalDevice physical_device() const { return phys_; }
    VkDevice         device()          const { return device_; }
    VkSurfaceKHR     surface()         const { return surface_; }
    VkQueue          graphics_queue()  const { return graphics_q_; }
    VkQueue          present_queue()   const { return present_q_; }
    u32              graphics_family() const { return families_.graphics.value(); }
    u32              present_family()  const { return families_.present.value(); }

    // Memory helpers (small bespoke allocator) ---------------------------
    struct AllocatedBuffer {
        VkBuffer        buffer = VK_NULL_HANDLE;
        VkDeviceMemory  memory = VK_NULL_HANDLE;
        VkDeviceSize    size   = 0;
        VkDeviceAddress device_address = 0;     // 0 if not buffer_device_address
    };
    struct AllocatedImage {
        VkImage        image  = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView    view   = VK_NULL_HANDLE;
        VkExtent3D     extent = {0,0,0};
        VkFormat       format = VK_FORMAT_UNDEFINED;
    };

    AllocatedBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags props) const;
    void destroy_buffer(AllocatedBuffer& b) const;

    AllocatedImage create_image(VkExtent3D extent, VkFormat format,
                                VkImageUsageFlags usage,
                                VkImageAspectFlags aspect) const;
    void destroy_image(AllocatedImage& i) const;

    void  upload_buffer(VkBuffer dst, const void* src, VkDeviceSize size) const;
    u32   find_memory_type(u32 type_filter, VkMemoryPropertyFlags props) const;

    // One-time command buffers (for uploads, AS builds) -----------------
    VkCommandBuffer begin_one_shot() const;
    void            end_one_shot(VkCommandBuffer cb) const;

    // Capabilities -------------------------------------------------------
    bool                                          rt_enabled() const { return rt_enabled_; }
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rt_props() const { return rt_props_; }

private:
    void create_instance(core::Window& window);
    void create_surface(core::Window& window);
    void pick_physical_device();
    void create_logical_device();
    void create_command_pool();

    VkInstance               instance_       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_= VK_NULL_HANDLE;
    VkSurfaceKHR             surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice         phys_           = VK_NULL_HANDLE;
    VkDevice                 device_         = VK_NULL_HANDLE;
    VkQueue                  graphics_q_     = VK_NULL_HANDLE;
    VkQueue                  present_q_      = VK_NULL_HANDLE;
    VkCommandPool            one_shot_pool_  = VK_NULL_HANDLE;

    QueueFamilyIndices       families_;
    DeviceFeaturesRequested  requested_;
    bool                     rt_enabled_     = false;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props_{};
    VkPhysicalDeviceProperties2                     phys_props2_{};
};

// Utility: read a SPIR-V file from disk relative to the working dir.
std::vector<u8> read_spv(const std::string& path);

// Utility: VkResult -> string for error logs.
const char* vk_result_string(VkResult r);

#define CRYOGEN_VK_CHECK(expr) do {                                     \
        VkResult _r = (expr);                                           \
        if (_r != VK_SUCCESS) {                                         \
            CRYOGEN_ERROR("vulkan", #expr, " => ",                      \
                          ::cryogen::render::vk_result_string(_r));     \
        }                                                               \
    } while(0)

} // namespace cryogen::render
