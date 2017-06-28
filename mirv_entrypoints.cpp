#include "mirv.h"

#include <memory>
#include <mutex>

//////////////////////////////////////////////

VKAPI_ATTR VkResult VKAPI_CALL
vkEnumerateInstanceLayerProperties(uint32_t* const out_propertyCount,
                                   VkLayerProperties* const out_properties)
{
    std::vector<VkLayerProperties> props;
    return VulkanArrayCopyMeme(props, out_propertyCount, out_properties);
}

VKAPI_ATTR VkResult VKAPI_CALL
vkEnumerateInstanceExtensionProperties(const char* const layerName,
                                       uint32_t* const out_propertyCount,
                                       VkExtensionProperties* const out_properties)
{
    if (layerName)
        return VK_ERROR_LAYER_NOT_PRESENT;

    std::vector<VkExtensionProperties> props;
    return VulkanArrayCopyMeme(props, out_propertyCount, out_properties);
}

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateInstance(const VkInstanceCreateInfo* const createInfo,
                 const VkAllocationCallbacks* const allocator,
                 VkInstance* const out)
{
    if (!createInfo)
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
    *out = gInstances.Put(created);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* const out_physicalDeviceCount,
                           VkPhysicalDevice* const out_physicalDevices)
{
    const auto inst = gInstances.Get(instance);
    if (!inst)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    const auto& physDevs = inst->EnumeratePhysicalDevices();
    return VulkanArrayCopyMeme(physDevs, out_physicalDeviceCount,
                               out_physicalDevices);
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
vkGetPhysicalDeviceProperties(const VkPhysicalDevice handle,
                              VkPhysicalDeviceProperties* const out_properties)
{
    const auto physDev = gPhysicalDevices.Get(handle);
    if (!physDev)
        return;

    *out_properties = physDev->mProperties;
}

VKAPI_ATTR void VKAPI_CALL
vkGetPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice handle,
                                         uint32_t* const out_propertyCount,
                                         VkQueueFamilyProperties* const out_properties)
{
    const auto physDev = gPhysicalDevices.Get(handle);
    if (!physDev)
        return;

    const auto& properties = physDev->mQueueFamilyProperties;
    (void)VulkanArrayCopyMeme(properties, out_propertyCount, out_properties);
}

/*
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties);
*/

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateDevice(const VkPhysicalDevice handle,
               const VkDeviceCreateInfo* createInfo,
               const VkAllocationCallbacks* allocator,
               VkDevice* const out_device)
{
    const auto physDev = gPhysicalDevices.Get(handle);
    if (!physDev)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    rp<MirvDevice> device;
    const auto res = physDev->CreateDevice(createInfo, allocator, &device);
    if (res != VK_SUCCESS)
        return res;

    *out_device = gDevices.Put(device);
    return VK_SUCCESS;
}
