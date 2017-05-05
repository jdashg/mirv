#include "mirv.h"

#include <memory>
#include <mutex>

//////////////////////////////////////////////

static ObjectTracker<VkInstance, MirvInstance> sInstances;

//////////////////////////////////////////////

VKAPI_ATTR VkResult VKAPI_CALL
vkEnumerateInstanceLayerProperties(uint32_t* const out_propertyCount,
                                   VkLayerProperties* const out_properties)
{
    if (!out_properties) {
        *out_propertyCount = 0;
        return VK_SUCCESS;
    }

    *out_propertyCount = 0;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkEnumerateInstanceExtensionProperties(const char* const layerName,
                                       uint32_t* const out_propertyCount,
                                       VkExtensionProperties* const out_properties)
{
    if (layerName)
        return VK_ERROR_LAYER_NOT_PRESENT;

    if (!out_properties) {
        *out_propertyCount = 0;
        return VK_SUCCESS;
    }

    *out_propertyCount = 0;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateInstance(const VkInstanceCreateInfo* const createInfo,
                 const VkAllocationCallbacks* const allocator,
                 VkInstance* const out)
{
    if (!createInfo) {
        return VK_ERROR_VALIDATION_FAILED_EXT;
    if (createInfo->sType != VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)
        return VK_ERROR_VALIDATION_FAILED_EXT;
    if (createInfo->pNext)
        return VK_ERROR_VALIDATION_FAILED_EXT;
    if (createInfo->flags)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    const auto& appInfo = createInfo->pApplicationInfo;
    if (appInfo) {
        if (appInfo->sType != VK_STRUCTURE_TYPE_APPLICATION_INFO)
            return VK_ERROR_VALIDATION_FAILED_EXT;
        if (appInfo->pNext)
            return VK_ERROR_VALIDATION_FAILED_EXT;

        const auto& apiVersion = appInfo->apiVersion;
        if (apiVersion) {
            if (apiVersion != VK_API_VERSION_1_0)
                return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    }

    if (createInfo->enabledLayerCount)
        return VK_ERROR_LAYER_NOT_PRESENT;
    if (createInfo->enabledExtensionCount)
        return VK_ERROR_EXTENSION_NOT_PRESENT;

    if (allocator)
        return VK_ERROR_NOT_IMPLEMENTED;

    const rp<MirvInstance> created = new MirvInstance;
    *out = sInstances.Track(created);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* const out_physicalDeviceCount,
                           VkPhysicalDevice* const out_physicalDevices)
{
    const auto inst = sInstances.GetTracked(instance);
    if (!inst)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    const auto& physicalDevices = inst->EnumeratePhysicalDevices();
    if (!out_physicalDevices) {
        *out_physicalDeviceCount = physicalDevices.size();
        return VK_SUCCESS;
    }

    if (*out_physicalDeviceCount < physicalDevices.size())
        return VK_INCOMPLETE;

    *out_physicalDeviceCount = physicalDevices.size();
    std::copy(physicalDevices.cbegin(), physicalDevices.cend(), out_physicalDevices);
    return VK_SUCCESS;
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

VKAPI_ATTR void VKAPI_CALL
vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                              VkPhysicalDeviceProperties* const out_properties)
{
    const auto physDev = MirvPhysicalDevice::From(physicalDevice);
    if (!physDev) {
        ZeroMemory(out_properties);
        return; // VK_ERROR_VALIDATION_FAILED_EXT
    }

    *out_properties = physDev->mProperties;
    return; // VK_SUCCESS
}

VKAPI_ATTR void VKAPI_CALL
vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
                                         uint32_t* const out_propertyCount,
                                         VkQueueFamilyProperties* const out_properties)
{
    const auto physDev = MirvPhysicalDevice::From(physicalDevice);
    if (!physDev) {
        ZeroMemory(out_properties);
        return; // VK_ERROR_VALIDATION_FAILED_EXT
    }

    const auto& properties = physDev->mQueueFamilyProperties;

    if (!out_properties) {
        *out_propertyCount = properties.size();
        return;// VK_SUCCESS;
    }

    if (*out_propertyCount < properties.size()) {
        ZeroMemory(out_properties);
        return;// VK_INCOMPLETE;
    }
    *out_propertyCount = properties.size();

    auto outItr = out_properties;
    for (const auto& cur : properties) {
        *outItr = cur;
        ++outItr;
    }
    return; // VK_SUCCESS
}
/*
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties);
*/

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateDevice(VkPhysicalDevice physicalDevice,
               const VkDeviceCreateInfo* createInfo,
               const VkAllocationCallbacks* allocator,
               VkDevice* const out_device)
{
    const auto physDev = MirvPhysicalDevice::From(physicalDevice);
    if (!physDev)
        return VK_ERROR_VALIDATION_FAILED_EXT

    rp<MirvDevice> ret;
    const auto res = physDev->CreateDevice(createInfo, allocator, &ret);
    if (res == VK_SUCCESS) {
        MirvDevice::Track(ret);
        *out_device = ret->ToOpaque();
        ret->AddRef();
    }
    return res;
}
