//
// cryogen/render/ray_tracing.cpp
//
#include "cryogen/render/ray_tracing.hpp"

#include <cstring>
#include <vector>

#include "cryogen/core/log.hpp"
#include "cryogen/render/mesh.hpp"

namespace cryogen::render {

namespace {
PFN_vkCreateAccelerationStructureKHR                pfn_create   = nullptr;
PFN_vkDestroyAccelerationStructureKHR               pfn_destroy  = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR         pfn_sizes    = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR             pfn_cmdbuild = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR      pfn_addr     = nullptr;

void resolve(VkDevice dev) {
    if (pfn_create) return;
    pfn_create   = (PFN_vkCreateAccelerationStructureKHR)        vkGetDeviceProcAddr(dev, "vkCreateAccelerationStructureKHR");
    pfn_destroy  = (PFN_vkDestroyAccelerationStructureKHR)       vkGetDeviceProcAddr(dev, "vkDestroyAccelerationStructureKHR");
    pfn_sizes    = (PFN_vkGetAccelerationStructureBuildSizesKHR) vkGetDeviceProcAddr(dev, "vkGetAccelerationStructureBuildSizesKHR");
    pfn_cmdbuild = (PFN_vkCmdBuildAccelerationStructuresKHR)     vkGetDeviceProcAddr(dev, "vkCmdBuildAccelerationStructuresKHR");
    pfn_addr     = (PFN_vkGetAccelerationStructureDeviceAddressKHR) vkGetDeviceProcAddr(dev, "vkGetAccelerationStructureDeviceAddressKHR");
}
}

AccelStructures::AccelStructures(VulkanContext& ctx) : ctx_(ctx) { resolve(ctx_.device()); }
AccelStructures::~AccelStructures() { destroy_tlas(); }

BLAS AccelStructures::build_blas(const Mesh& mesh) {
    BLAS blas{};
    if (!ctx_.rt_enabled()) return blas;

    VkAccelerationStructureGeometryTrianglesDataKHR tri{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    tri.vertexData.deviceAddress = mesh.vertex_device_address();
    tri.vertexStride = sizeof(Vertex);
    tri.maxVertex    = mesh.vertex_count() - 1;
    tri.indexType    = VK_INDEX_TYPE_UINT32;
    tri.indexData.deviceAddress = mesh.index_device_address();

    VkAccelerationStructureGeometryKHR geom{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geom.geometryType        = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.geometry.triangles  = tri;
    geom.flags               = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR bgi{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bgi.type  = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    bgi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    bgi.mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bgi.geometryCount = 1;
    bgi.pGeometries   = &geom;

    const u32 prim_count = mesh.index_count() / 3;

    VkAccelerationStructureBuildSizesInfoKHR sz{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    pfn_sizes(ctx_.device(),
              VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
              &bgi, &prim_count, &sz);

    auto buf = ctx_.create_buffer(sz.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    blas.buffer = buf.buffer;
    blas.memory = buf.memory;

    VkAccelerationStructureCreateInfoKHR aci{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    aci.buffer = blas.buffer;
    aci.size   = sz.accelerationStructureSize;
    aci.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    CRYOGEN_VK_CHECK(pfn_create(ctx_.device(), &aci, nullptr, &blas.handle));

    auto scratch = ctx_.create_buffer(sz.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    bgi.dstAccelerationStructure  = blas.handle;
    bgi.scratchData.deviceAddress = scratch.device_address;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = prim_count;
    const VkAccelerationStructureBuildRangeInfoKHR* p_range = &range;

    VkCommandBuffer cb = ctx_.begin_one_shot();
    pfn_cmdbuild(cb, 1, &bgi, &p_range);
    ctx_.end_one_shot(cb);

    ctx_.destroy_buffer(scratch);

    VkAccelerationStructureDeviceAddressInfoKHR ai{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    ai.accelerationStructure = blas.handle;
    blas.device_address = pfn_addr(ctx_.device(), &ai);
    return blas;
}

void AccelStructures::destroy_blas(BLAS& blas) {
    if (blas.handle) pfn_destroy(ctx_.device(), blas.handle, nullptr);
    if (blas.buffer) vkDestroyBuffer(ctx_.device(), blas.buffer, nullptr);
    if (blas.memory) vkFreeMemory(ctx_.device(), blas.memory, nullptr);
    blas = {};
}

void AccelStructures::build_or_update_tlas(const std::vector<TLASInstance>& instances) {
    if (!ctx_.rt_enabled() || instances.empty()) return;

    destroy_tlas();
    tlas_.instance_count = static_cast<u32>(instances.size());

    std::vector<VkAccelerationStructureInstanceKHR> raw_instances(instances.size());
    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& src = instances[i];
        auto& dst = raw_instances[i];
        // VkTransformMatrixKHR is row-major 3x4. mat4 is column-major
        // (m.c[col][row]); transpose-on-copy.
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                dst.transform.matrix[row][col] = src.transform.c[col][row];
        dst.instanceCustomIndex                    = src.custom_id & 0x00FFFFFF;
        dst.mask                                   = 0xFF;
        dst.instanceShaderBindingTableRecordOffset = 0;
        dst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        dst.accelerationStructureReference         = src.blas->device_address;
    }

    const VkDeviceSize inst_bytes = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
    auto inst_buf = ctx_.create_buffer(inst_bytes,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ctx_.upload_buffer(inst_buf.buffer, raw_instances.data(), inst_bytes);
    tlas_.instances_buffer = inst_buf.buffer;
    tlas_.instances_memory = inst_buf.memory;

    VkAccelerationStructureGeometryInstancesDataKHR igd{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    igd.data.deviceAddress = inst_buf.device_address;

    VkAccelerationStructureGeometryKHR geom{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geom.geometryType         = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.geometry.instances   = igd;

    VkAccelerationStructureBuildGeometryInfoKHR bgi{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bgi.type           = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    bgi.flags          = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    bgi.mode           = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bgi.geometryCount  = 1;
    bgi.pGeometries    = &geom;

    const u32 inst_count = static_cast<u32>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR sz{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    pfn_sizes(ctx_.device(),
              VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
              &bgi, &inst_count, &sz);

    auto buf = ctx_.create_buffer(sz.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    tlas_.buffer = buf.buffer;
    tlas_.memory = buf.memory;

    VkAccelerationStructureCreateInfoKHR aci{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    aci.buffer = tlas_.buffer;
    aci.size   = sz.accelerationStructureSize;
    aci.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    CRYOGEN_VK_CHECK(pfn_create(ctx_.device(), &aci, nullptr, &tlas_.handle));

    auto scratch = ctx_.create_buffer(sz.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    tlas_.scratch_buffer = scratch.buffer;
    tlas_.scratch_memory = scratch.memory;

    bgi.dstAccelerationStructure  = tlas_.handle;
    bgi.scratchData.deviceAddress = scratch.device_address;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = inst_count;
    const VkAccelerationStructureBuildRangeInfoKHR* p_range = &range;

    VkCommandBuffer cb = ctx_.begin_one_shot();
    pfn_cmdbuild(cb, 1, &bgi, &p_range);
    ctx_.end_one_shot(cb);
}

void AccelStructures::destroy_tlas() {
    if (tlas_.handle)           pfn_destroy(ctx_.device(), tlas_.handle, nullptr);
    if (tlas_.buffer)           vkDestroyBuffer(ctx_.device(), tlas_.buffer, nullptr);
    if (tlas_.memory)           vkFreeMemory(ctx_.device(), tlas_.memory, nullptr);
    if (tlas_.instances_buffer) vkDestroyBuffer(ctx_.device(), tlas_.instances_buffer, nullptr);
    if (tlas_.instances_memory) vkFreeMemory(ctx_.device(), tlas_.instances_memory, nullptr);
    if (tlas_.scratch_buffer)   vkDestroyBuffer(ctx_.device(), tlas_.scratch_buffer, nullptr);
    if (tlas_.scratch_memory)   vkFreeMemory(ctx_.device(), tlas_.scratch_memory, nullptr);
    tlas_ = {};
}

} // namespace cryogen::render
