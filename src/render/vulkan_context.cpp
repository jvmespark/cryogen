#include "cryogen/render/vulkan_context.hpp"
#include "cryogen/core/log.hpp"
#include "cryogen/core/window.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>
#include <vector>

namespace cryogen::render {

namespace {

#if defined(CRYOGEN_ENABLE_VALIDATION)
constexpr bool kEnableValidation = true;
#else
constexpr bool kEnableValidation = false;
#endif

const std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> kBaseDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

const std::vector<const char*> kRayTracingExtensions = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
};

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*) {
    using ::cryogen::log::Severity;
    Severity s = Severity::Trace;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) s = Severity::Error;
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) s = Severity::Warn;
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)  s = Severity::Info;
    CRYOGEN_LOG(s, "vk-validation", data->pMessage);
    return VK_FALSE;
}

bool layers_supported(const std::array<const char*, 1>& wanted) {
    u32 count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> avail(count);
    vkEnumerateInstanceLayerProperties(&count, avail.data());
    for (auto* w : wanted) {
        bool found = false;
        for (auto& l : avail) {
            if (std::strcmp(w, l.layerName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

QueueFamilyIndices find_queue_families(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    QueueFamilyIndices q;
    u32 n = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &n, nullptr);
    std::vector<VkQueueFamilyProperties> props(n);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &n, props.data());

    for (u32 i = 0; i < n; ++i) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) q.graphics = i;
        if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT)  q.compute  = i;
        if (props[i].queueFlags & VK_QUEUE_TRANSFER_BIT) q.transfer = i;

        VkBool32 present_ok = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present_ok);
        if (present_ok) q.present = i;

        if (q.complete()) break;
    }
    return q;
}

bool device_supports_extensions(VkPhysicalDevice dev,
                                const std::vector<const char*>& wanted) {
    u32 n = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> avail(n);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, avail.data());
    for (auto* w : wanted) {
        bool found = false;
        for (auto& a : avail) {
            if (std::strcmp(w, a.extensionName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

} // namespace

const char* vk_result_string(VkResult r) {
    switch (r) {
        case VK_SUCCESS:                            return "VK_SUCCESS";
        case VK_NOT_READY:                          return "VK_NOT_READY";
        case VK_TIMEOUT:                            return "VK_TIMEOUT";
        case VK_INCOMPLETE:                         return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:           return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:         return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:        return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:                  return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_LAYER_NOT_PRESENT:            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:        return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:          return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_TOO_MANY_OBJECTS:             return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:         return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL:              return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_OUT_OF_DATE_KHR:              return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_SUBOPTIMAL_KHR:                     return "VK_SUBOPTIMAL_KHR";
        default:                                    return "VK_ERROR_<unknown>";
    }
}

std::vector<u8> read_spv(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        CRYOGEN_ERROR("vulkan", "Failed to open SPIR-V file: ", path);
        return {};
    }
    const std::streamsize n = f.tellg();
    f.seekg(0);
    std::vector<u8> data(static_cast<usize>(n));
    f.read(reinterpret_cast<char*>(data.data()), n);
    return data;
}

VulkanContext::VulkanContext(core::Window& window, const DeviceFeaturesRequested& f)
    : requested_(f) {
    create_instance(window);
    create_surface(window);
    pick_physical_device();
    create_logical_device();
    create_command_pool();
    CRYOGEN_INFO("vulkan", "VulkanContext ready, RT=", rt_enabled_);
}

VulkanContext::~VulkanContext() {
    if (device_) {
        vkDeviceWaitIdle(device_);
        if (one_shot_pool_) vkDestroyCommandPool(device_, one_shot_pool_, nullptr);
        vkDestroyDevice(device_, nullptr);
    }
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (debug_messenger_) {
        auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (fn) fn(instance_, debug_messenger_, nullptr);
    }
    if (instance_) vkDestroyInstance(instance_, nullptr);
}

void VulkanContext::create_instance(core::Window& window) {
    if (kEnableValidation && !layers_supported(kValidationLayers)) {
        CRYOGEN_WARN("vulkan", "Validation layers requested but not present.");
    }

    VkApplicationInfo app{};
    app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName   = "CryoGen";
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName        = "CryoGen";
    app.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion         = VK_API_VERSION_1_2;

    auto exts = window.required_instance_extensions();
    if constexpr (kEnableValidation) {
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo ic{};
    ic.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ic.pApplicationInfo        = &app;
    ic.enabledExtensionCount   = static_cast<u32>(exts.size());
    ic.ppEnabledExtensionNames = exts.data();
    if constexpr (kEnableValidation) {
        ic.enabledLayerCount   = static_cast<u32>(kValidationLayers.size());
        ic.ppEnabledLayerNames = kValidationLayers.data();
    }

    CRYOGEN_VK_CHECK(vkCreateInstance(&ic, nullptr, &instance_));

    if constexpr (kEnableValidation) {
        VkDebugUtilsMessengerCreateInfoEXT mc{};
        mc.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        mc.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        mc.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        mc.pfnUserCallback = debug_callback;

        auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (fn) fn(instance_, &mc, nullptr, &debug_messenger_);
    }
}

void VulkanContext::create_surface(core::Window& window) {
    CRYOGEN_VK_CHECK(window.create_vulkan_surface(instance_, &surface_));
}

void VulkanContext::pick_physical_device() {
    u32 n = 0;
    vkEnumeratePhysicalDevices(instance_, &n, nullptr);
    if (n == 0) throw std::runtime_error("No Vulkan-capable GPUs.");

    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(instance_, &n, devs.data());

    auto score = [&](VkPhysicalDevice d) -> int {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(d, &p);
        int s = 0;
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) s += 1000;
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) s += 100;
        s += static_cast<int>(p.limits.maxImageDimension2D / 1024);
        return s;
    };

    std::sort(devs.begin(), devs.end(),
        [&](VkPhysicalDevice a, VkPhysicalDevice b){ return score(a) > score(b); });

    for (auto d : devs) {
        QueueFamilyIndices q = find_queue_families(d, surface_);
        if (!q.complete()) continue;

        if (!device_supports_extensions(d, kBaseDeviceExtensions)) continue;

        bool rt_ok = requested_.ray_tracing &&
                     device_supports_extensions(d, kRayTracingExtensions);

        phys_     = d;
        families_ = q;
        rt_enabled_ = rt_ok;

        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(d, &p);
        CRYOGEN_INFO("vulkan", "Picked GPU: ", p.deviceName);

        if (rt_enabled_) {
            rt_props_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            phys_props2_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            phys_props2_.pNext = &rt_props_;
            vkGetPhysicalDeviceProperties2(d, &phys_props2_);
        }
        return;
    }

    throw std::runtime_error("No suitable Vulkan device.");
}

void VulkanContext::create_logical_device() {
    std::set<u32> unique_families = {
        families_.graphics.value(),
        families_.present.value(),
    };

    f32 priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    for (u32 fam : unique_families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;
        qcis.push_back(qci);
    }

    std::vector<const char*> enabled_exts = kBaseDeviceExtensions;
    if (rt_enabled_) {
        for (auto* e : kRayTracingExtensions) enabled_exts.push_back(e);
    }

    VkPhysicalDeviceFeatures core{};
    core.samplerAnisotropy = VK_TRUE;
    core.fillModeNonSolid  = VK_TRUE;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features f12{};
    f12.sType                                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    f12.bufferDeviceAddress                  = requested_.buffer_device_address;
    f12.descriptorIndexing                   = requested_.descriptor_indexing;
    f12.runtimeDescriptorArray               = requested_.descriptor_indexing;
    f12.shaderSampledImageArrayNonUniformIndexing = requested_.descriptor_indexing;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR as_feat{};
    as_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    as_feat.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_feat{};
    rt_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rt_feat.rayTracingPipeline = VK_TRUE;

    if (rt_enabled_) {
        as_feat.pNext = &rt_feat;
        f12.pNext     = &as_feat;
    }

    VkPhysicalDeviceFeatures2 f2{};
    f2.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    f2.features = core;
    f2.pNext    = &f12;

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext                   = &f2;
    dci.queueCreateInfoCount    = static_cast<u32>(qcis.size());
    dci.pQueueCreateInfos       = qcis.data();
    dci.enabledExtensionCount   = static_cast<u32>(enabled_exts.size());
    dci.ppEnabledExtensionNames = enabled_exts.data();
    if constexpr (kEnableValidation) {
        dci.enabledLayerCount   = static_cast<u32>(kValidationLayers.size());
        dci.ppEnabledLayerNames = kValidationLayers.data();
    }

    CRYOGEN_VK_CHECK(vkCreateDevice(phys_, &dci, nullptr, &device_));

    vkGetDeviceQueue(device_, families_.graphics.value(), 0, &graphics_q_);
    vkGetDeviceQueue(device_, families_.present.value(),  0, &present_q_);
}

void VulkanContext::create_command_pool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
                        | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    ci.queueFamilyIndex = families_.graphics.value();
    CRYOGEN_VK_CHECK(vkCreateCommandPool(device_, &ci, nullptr, &one_shot_pool_));
}

u32 VulkanContext::find_memory_type(u32 type_filter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys_, &mp);
    for (u32 i = 0; i < mp.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    CRYOGEN_ERROR("vulkan", "find_memory_type: no compatible heap.");
    return 0;
}

VulkanContext::AllocatedBuffer
VulkanContext::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props) const {
    AllocatedBuffer ab;
    ab.size = size;

    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CRYOGEN_VK_CHECK(vkCreateBuffer(device_, &bi, nullptr, &ab.buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, ab.buffer, &req);

    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
                ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0u;

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.pNext           = &flags;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, props);
    CRYOGEN_VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &ab.memory));
    CRYOGEN_VK_CHECK(vkBindBufferMemory(device_, ab.buffer, ab.memory, 0));

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo di{};
        di.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        di.buffer = ab.buffer;
        ab.device_address = vkGetBufferDeviceAddress(device_, &di);
    }
    return ab;
}

void VulkanContext::destroy_buffer(AllocatedBuffer& b) const {
    if (b.buffer) vkDestroyBuffer(device_, b.buffer, nullptr);
    if (b.memory) vkFreeMemory(device_, b.memory, nullptr);
    b = {};
}

VulkanContext::AllocatedImage
VulkanContext::create_image(VkExtent3D extent, VkFormat format,
                            VkImageUsageFlags usage,
                            VkImageAspectFlags aspect) const {
    AllocatedImage img;
    img.extent = extent;
    img.format = format;

    VkImageCreateInfo ic{};
    ic.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ic.imageType     = VK_IMAGE_TYPE_2D;
    ic.format        = format;
    ic.extent        = extent;
    ic.mipLevels     = 1;
    ic.arrayLayers   = 1;
    ic.samples       = VK_SAMPLE_COUNT_1_BIT;
    ic.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ic.usage         = usage;
    ic.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ic.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CRYOGEN_VK_CHECK(vkCreateImage(device_, &ic, nullptr, &img.image));

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device_, img.image, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CRYOGEN_VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &img.memory));
    CRYOGEN_VK_CHECK(vkBindImageMemory(device_, img.image, img.memory, 0));

    VkImageViewCreateInfo vi{};
    vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image    = img.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = format;
    vi.subresourceRange.aspectMask     = aspect;
    vi.subresourceRange.baseMipLevel   = 0;
    vi.subresourceRange.levelCount     = 1;
    vi.subresourceRange.baseArrayLayer = 0;
    vi.subresourceRange.layerCount     = 1;
    CRYOGEN_VK_CHECK(vkCreateImageView(device_, &vi, nullptr, &img.view));

    return img;
}

void VulkanContext::destroy_image(AllocatedImage& i) const {
    if (i.view)   vkDestroyImageView(device_, i.view, nullptr);
    if (i.image)  vkDestroyImage(device_, i.image, nullptr);
    if (i.memory) vkFreeMemory(device_, i.memory, nullptr);
    i = {};
}

VkCommandBuffer VulkanContext::begin_one_shot() const {
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool        = one_shot_pool_;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device_, &ai, &cb);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

void VulkanContext::end_one_shot(VkCommandBuffer cb) const {
    vkEndCommandBuffer(cb);
    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cb;
    vkQueueSubmit(graphics_q_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_q_);
    vkFreeCommandBuffers(device_, one_shot_pool_, 1, &cb);
}

void VulkanContext::upload_buffer(VkBuffer dst, const void* src, VkDeviceSize size) const {
    AllocatedBuffer staging = create_buffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* mapped = nullptr;
    vkMapMemory(device_, staging.memory, 0, size, 0, &mapped);
    std::memcpy(mapped, src, static_cast<size_t>(size));
    vkUnmapMemory(device_, staging.memory);

    VkCommandBuffer cb = begin_one_shot();
    VkBufferCopy cp{0, 0, size};
    vkCmdCopyBuffer(cb, staging.buffer, dst, 1, &cp);
    end_one_shot(cb);

    destroy_buffer(staging);
}

} // namespace cryogen::render
