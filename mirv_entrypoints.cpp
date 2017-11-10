#include "mirv.h"

#include <memory>
#include <mutex>

//////////////////////////////////////////////

#ifdef _WIN32
#define LIB_EXPORT __declspec(dllexport)
#endif

extern "C" {

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkEnumerateInstanceLayerProperties(uint32_t* const out_propertyCount,
                                   VkLayerProperties* const out_properties)
{
    std::vector<VkLayerProperties> props;
    return VulkanArrayCopyMeme(props, out_propertyCount, out_properties);
}

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkEnumerateInstanceExtensionProperties(const char* const layerName,
                                       uint32_t* const out_propertyCount,
                                       VkExtensionProperties* const out_properties)
{
    if (layerName)
        return VK_ERROR_LAYER_NOT_PRESENT;

    std::vector<VkExtensionProperties> props;
    return VulkanArrayCopyMeme(props, out_propertyCount, out_properties);
}

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkCreateInstance(const VkInstanceCreateInfo* const pCreateInfo,
                 const VkAllocationCallbacks* const allocator,
                 VkInstance* const out)
{
    if (allocator) {
        // All other entrypoints just ignore it.
        return VK_ERROR_NOT_IMPLEMENTED;
    }

    VkInstanceCreateInfo createInfo;
    if (!SnapshotStruct(&createInfo, pCreateInfo, VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO))
        return VK_ERROR_VALIDATION_FAILED_EXT;

    if (createInfo.pNext || createInfo.flags)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    const auto& pAppInfo = createInfo.pApplicationInfo;
    if (pAppInfo) {
        VkApplicationInfo appInfo;
        if (!SnapshotStruct(&appInfo, pAppInfo, VK_STRUCTURE_TYPE_APPLICATION_INFO))
            return VK_ERROR_VALIDATION_FAILED_EXT;

        if (appInfo.pNext)
            return VK_ERROR_VALIDATION_FAILED_EXT;

        const auto& apiVersion = appInfo.apiVersion;
        if (apiVersion) {
            if (apiVersion != VK_API_VERSION_1_0)
                return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    }

    if (createInfo.enabledLayerCount)
        return VK_ERROR_LAYER_NOT_PRESENT;
    if (createInfo.enabledExtensionCount)
        return VK_ERROR_EXTENSION_NOT_PRESENT;

    const auto& obj = new MirvInstance;
    *out = Handles::Add(obj);
    return VK_SUCCESS;
}

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkDestroyInstance(const VkInstance instance, const VkAllocationCallbacks*)
{
    // Just release the handle table ref, and let the instance die when its refcount falls
    // to 0. (when it's no longer in use)
    // When no strong references are held, we're safe to destroy it, since the dtor thread
    // will be the only active user.
    (void)Handles::Remove(instance);
}

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkEnumeratePhysicalDevices(const VkInstance instance, uint32_t* const out_physicalDeviceCount,
                           VkPhysicalDevice* const out_physicalDevices)
{
    const auto& inst = Handles::Get(instance);
    if (!inst)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    std::vector<VkPhysicalDevice> handles;
    handles.reserve(inst->mPhysDevs.size());
    for (const auto& x : inst->mPhysDevs) {
        handles.push_back(Handles::Add(x.get()));
    }
    return VulkanArrayCopyMeme(handles, out_physicalDeviceCount, out_physicalDevices);
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
    const auto& physDev = Handles::Get(handle);
    if (!physDev)
        return;

    *out_properties = physDev->mProperties;
}

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkGetPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice handle,
                                         uint32_t* const out_propertyCount,
                                         VkQueueFamilyProperties* const out_properties)
{
    const auto& physDev = Handles::Get(handle);
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

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkCreateDevice(const VkPhysicalDevice handle,
               const VkDeviceCreateInfo* const pCreateInfo,
               const VkAllocationCallbacks*,
               VkDevice* const out_device)
{
    const auto& physDev = Handles::Get(handle);
    if (!physDev)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    VkDeviceCreateInfo createInfo;
    if (!SnapshotStruct(&createInfo, pCreateInfo, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO))
        return VK_ERROR_VALIDATION_FAILED_EXT;

    rp<MirvDevice> device;
    const auto res = physDev->CreateDevice(createInfo, &device);
    if (res != VK_SUCCESS)
        return res;

    *out_device = Handles::Add(device.get());
    return VK_SUCCESS;
}

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkDestroyDevice(const VkDevice handle, const VkAllocationCallbacks*)
{
    (void)Handles::Remove(handle);
}

// --

LIB_EXPORT VKAPI_ATTR void VKAPI_CALL
vkGetDeviceQueue(const VkDevice handle, const uint32_t queueFamilyIndex,
                 const uint32_t queueIndex, VkQueue* const out_queue)
{
    const auto& dev = Handles::Get(handle);
    if (!dev)
        return;

    const auto& itr = dev->mQueuesByFamily.find(queueFamilyIndex);
    if (itr == dev->mQueuesByFamily.end())
        return;
    const auto& queues = itr->second;

    if (queueIndex >= queues.size())
        return;
    const auto& queue = queues[queueIndex];

    *out_queue = Handles::Add(queue.get());
}

// --

LIB_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkCreateCommandPool(const VkDevice handle,
                    const VkCommandPoolCreateInfo* const pCreateInfo,
                    const VkAllocationCallbacks*, VkCommandPool* const pCommandPool)
{
    const auto& dev = Handles::Get(handle);
    if (!dev)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    VkCommandPoolCreateInfo createInfo;
    if (!SnapshotStruct(&createInfo, pCreateInfo, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO))
        return VK_ERROR_VALIDATION_FAILED_EXT;

    if (createInfo.pNext)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    const auto& itr = dev->mQueuesByFamily.find(createInfo.queueFamilyIndex);
    if (itr == dev->mQueuesByFamily.end())
        return VK_ERROR_VALIDATION_FAILED_EXT;
    const auto& queues = itr->second;
    if (!queues.size())
        return VK_ERROR_VALIDATION_FAILED_EXT;
    const auto& someQueue = queues[0];
    const auto& family = someQueue->mFamily;
}

} // extern "C"
