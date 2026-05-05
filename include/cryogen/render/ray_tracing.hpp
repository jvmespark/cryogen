#pragma once
//
// cryogen/render/ray_tracing.hpp
// Builds and owns BLASes (one per Mesh) and a TLAS (rebuilt every frame for
// dynamic transforms in the current scene).
//

#include <vector>

#include <vulkan/vulkan.h>

#include "cryogen/core/types.hpp"
#include "cryogen/math/mat.hpp"
#include "cryogen/render/vulkan_context.hpp"

namespace cryogen::render {

class Mesh;

struct BLAS {
    VkAccelerationStructureKHR     handle = VK_NULL_HANDLE;
    VkBuffer                       buffer = VK_NULL_HANDLE;
    VkDeviceMemory                 memory = VK_NULL_HANDLE;
    VkDeviceAddress                device_address = 0;
};

struct TLAS {
    VkAccelerationStructureKHR     handle = VK_NULL_HANDLE;
    VkBuffer                       buffer = VK_NULL_HANDLE;
    VkDeviceMemory                 memory = VK_NULL_HANDLE;
    VkBuffer                       instances_buffer = VK_NULL_HANDLE;
    VkDeviceMemory                 instances_memory = VK_NULL_HANDLE;
    VkBuffer                       scratch_buffer   = VK_NULL_HANDLE;
    VkDeviceMemory                 scratch_memory   = VK_NULL_HANDLE;
    u32                            instance_count   = 0;
};

struct TLASInstance {
    const BLAS* blas      = nullptr;
    math::mat4  transform = math::mat4::identity();
    u32         custom_id = 0;
};

class AccelStructures {
public:
    explicit AccelStructures(VulkanContext& ctx);
    ~AccelStructures();

    AccelStructures(const AccelStructures&)            = delete;
    AccelStructures& operator=(const AccelStructures&) = delete;

    BLAS build_blas(const Mesh& mesh);
    void destroy_blas(BLAS& blas);

    void build_or_update_tlas(const std::vector<TLASInstance>& instances);
    void destroy_tlas();

    VkAccelerationStructureKHR tlas_handle() const { return tlas_.handle; }

private:
    VulkanContext& ctx_;
    TLAS           tlas_{};
};

} // namespace cryogen::render
