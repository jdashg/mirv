#include "mirv.h"

#include <memory>
#include <mutex>

//////////////////////////////////////////////

#ifdef _WIN32
#define LIB_EXPORT __declspec(dllexport)
#endif

static std::mutex gMutex;
static std::set<rp<MirvInstance>> gInstances;

extern "C" {

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkEnumerateInstanceLayerProperties(uint32_t* const out_propertyCount,
                                   VkLayerProperties* const out_properties)
{
    std::vector<VkLayerProperties> props; // empty
    return VulkanArrayCopyMeme(props, out_propertyCount, out_properties);
}

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkEnumerateInstanceExtensionProperties(const char* const layerName,
                                       uint32_t* const out_propertyCount,
                                       VkExtensionProperties* const out_properties)
{
    if (layerName)
        return VK_ERROR_LAYER_NOT_PRESENT;

    std::vector<VkExtensionProperties> props; // empty
    return VulkanArrayCopyMeme(props, out_propertyCount, out_properties);
}

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkCreateInstance(const VkInstanceCreateInfo* const createInfo,
                 const VkAllocationCallbacks* const allocator,
                 VkInstance* const out)
{
    ASSERT(!allocator)
    return MirvInstance::vkCreateInstance(*createInfo, MapHandle(out));
}

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkDestroyInstance(const VkInstance handle, const VkAllocationCallbacks*)
{
    MapHandle(handle)->vkDestroyInstance();
}

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkEnumeratePhysicalDevices(const VkInstance handle, uint32_t* const out_physicalDeviceCount,
                           VkPhysicalDevice* const out_physicalDevices)
{
    return MapHandle(handle)->vkEnumeratePhysicalDevices(handle, out_physicalDeviceCount,
                                                         MapHandle(out_physicalDevices));
}

/*
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures);

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          flags,
    VkImageFormatProperties*                    pImageFormatProperties);
*/

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkGetPhysicalDeviceProperties(const VkPhysicalDevice handle,
                              VkPhysicalDeviceProperties* const out_properties)
{
    *out_properties = MapHandle(handle)->mProperties;
}

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkGetPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice handle,
                                         uint32_t* const out_propertyCount,
                                         VkQueueFamilyProperties* const out_properties)
{
    const auto& props = MapHandle(handle)->mQueueFamilyProperties;
    (void)VulkanArrayCopyMeme(props, out_propertyCount, out_properties);
}

/*
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties);
*/

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkCreateDevice(const VkPhysicalDevice handle,
               const VkDeviceCreateInfo* const createInfo,
               const VkAllocationCallbacks*,
               VkDevice* const out_device)
{
    return MapHandle(handle)->vkCreateDevice(*createInfo, MapHandle(out_device));
}

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkDestroyDevice(const VkDevice handle, const VkAllocationCallbacks*)
{
    MapHandle(handle)->vkDestroyDevice();
}

// --

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkGetDeviceQueue(const VkDevice handle, const uint32_t queueFamilyIndex,
                 const uint32_t queueIndex, VkQueue* const out_queue)
{
    return MapHandle(handle)->vkGetDeviceQueue(queueFamilyIndex, queueIndex,
                                               MapHandle(out_queue));
}

// --

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkCreateCommandPool(const VkDevice handle,
                    const VkCommandPoolCreateInfo* const createInfo,
                    const VkAllocationCallbacks*, VkCommandPool* const out)
{
    return MapHandle(handle)->vkCreateCommandPool(*createInfo, MapHandle(out));
}

} // extern "C"
