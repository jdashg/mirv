#include "mirv.h"

#include <memory>
#include <mutex>

std::mutex MirvInstance::gMutex;
std::set<rp<MirvInstance>> MirvInstance::gInstances;

MirvInstance::MirvInstance()
    : MirvObject(MirvObjectType::Instance)
{
#ifdef HAS_D3D12
    AddPhysDevs<Backends::D3D12>();
#endif
#ifdef HAS_METAL
    AddPhysDevs<Backends::Metal>();
#endif
#ifdef HAS_VULKAN
    AddPhysDevs<Backends::Vulkan>();
#endif
}

MirvInstance::~MirvInstance() = default;

/*static*/ VkResult
MirvInstance::vkCreateInstance(const VkInstanceCreateInfo& createInfo,
                               MirvInstance** const out)
{
    ASSERT(!createInfo.pNext)
    ASSERT(!createInfo.flags)

    const auto& appInfo = createInfo.pApplicationInfo;
    if (appInfo) {
        ASSERT(!appInfo->pNext)

        const auto& apiVersion = appInfo->apiVersion;
        if (apiVersion) {
            if (apiVersion != VK_API_VERSION_1_0)
                return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    }

    if (createInfo.enabledLayerCount)
        return VK_ERROR_LAYER_NOT_PRESENT;
    if (createInfo.enabledExtensionCount)
        return VK_ERROR_EXTENSION_NOT_PRESENT;

    const auto& inst = new MirvInstance;
    {
        const mutex_guard guard(gMutex);
        gInstances.insert(inst);
    }
    *out = inst;
    return VK_SUCCESS;
}

VkResult
MirvInstance::vkEnumeratePhysicalDevices(const VkInstance handle,
                                         uint32_t* const out_physicalDeviceCount,
                                         MirvPhysicalDevice** const out_physicalDevices) const
{
    std::vector<MirvPhysicalDevice*> ret;
    ret.reserve(mPhysDevs.size());
    for (const auto& x : mPhysDevs) {
        ret.push_back(x.get());
    }
    return VulkanArrayCopyMeme(ret, out_physicalDeviceCount, out_physicalDevices);
}

void
MirvInstance::vkDestroyInstance()
{
    const mutex_guard guard(gMutex);
    gInstances.erase(this);
}

// --

VkResult
MirvPhysicalDevice::vkCreateDevice(const VkDeviceCreateInfo& createInfo,
                                   MirvDevice** const out)
{
    rp<MirvDevice> dev;
    const auto res = CreateDevice(createInfo, &dev);
    if (dev) {
        const mutex_guard guard(mMutex);
        mDevices.insert(dev);
    }
    *out = dev.get();
    return res;
}

// --

MirvDevice::MirvDevice(MirvPhysicalDevice& physDev)
    : MirvObject(MirvObjectType::Device)
    , mPhysDev(physDev)
{ }

MirvDevice::~MirvDevice() = default;


void
MirvDevice::vkGetDeviceQueue(const uint32_t queueFamilyIndex, const uint32_t queueIndex,
                             MirvQueue** const out) const
{
    const auto& itr = mQueuesByFamily.find(queueFamilyIndex);
    ASSERT(itr != mQueuesByFamily.end())
    const auto& queues = itr->second;

    ASSERT(queueIndex >= queues.size())
    *out = queues[queueIndex].get();
}

VkResult
MirvDevice::AddAllQueues(const VkDeviceCreateInfo& createInfo)
{
    if (!createInfo.queueCreateInfoCount)
        return VK_ERROR_INITIALIZATION_FAILED;

    for (const auto& info : Range(createInfo.pQueueCreateInfos,
                                  createInfo.queueCreateInfoCount))
    {
        const auto& familyIndex = info.queueFamilyIndex;
        if (familyIndex >= mPhysDev.mQueueFamilyProperties.size())
            return VK_ERROR_INITIALIZATION_FAILED;
        const auto& familyInfo = mPhysDev.mQueueFamilyProperties[familyIndex];

        const auto res = mQueuesByFamily.insert({familyIndex,
                                                 std::vector<rp<MirvQueue>>()});
        const auto& didInsert = res.second;
        if (!didInsert)
            return VK_ERROR_INITIALIZATION_FAILED;
        auto& queues = res.first->second;

        const auto vkRes = AddQueues(info, familyInfo, &queues);
        if (vkRes != VK_SUCCESS)
            return vkRes;
    }
    return VK_SUCCESS;
}

VkResult
MirvDevice::vkCreateCommandPool(const VkCommandPoolCreateInfo& createInfo,
                                MirvCommandPool** const out) const
{
    const auto& itr = mQueuesByFamily.find(createInfo.queueFamilyIndex);
    ASSERT(itr != mQueuesByFamily.end())
    const auto& queues = itr->second;

    ASSERT(!queues.size())
    const auto& someQueue = queues[0];
    const auto& family = someQueue->mFamily;
    return VK_ERROR_NOT_IMPLEMENTED;
}