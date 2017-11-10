#include "mirv.h"

#include <memory>
#include <mutex>

//////////////////////////////////////////////
/*
ObjectTracker<VkInstance, MirvInstance> gInstances;
ObjectTracker<VkPhysicalDevice, MirvPhysicalDevice> gPhysicalDevices;
ObjectTracker<VkDevice, MirvDevice> gDevices;
ObjectTracker<VkQueue, MirvQueue> gQueues;
*/
//////////////////////////////////////////////

class HandleTable final
{
    mutable std::mutex mMutex;
    mutable std::vector<rp<MirvObject>> mObjects;
    std::stack<uint64_t> mFreeList;

public:
    uint64_t Add(MirvObject* obj) {
        const std::lock_guard<std::mutex> lock(mMutex);
        ASSERT(obj);
        if (obj->mHandle)
            return obj->mHandle;

        uint64_t handle;
        if (mFreeList.size()) {
            handle = mFreeList.top();
            mFreeList.pop();
        } else {
            handle = mObjects.size() + 1;
            mObjects.push_back(nullptr);
        }
        const auto index = handle - 1;
        mObjects[size_t(index)] = obj;
        obj->mHandle = handle;
        return handle;
    }

private:
    rp<MirvObject>* GetSlot(const uint64_t handle, const MirvObjectType type) const {
        const auto index = handle - 1;
        if (index >= mObjects.size())
            return nullptr;

        auto& slot = mObjects[size_t(index)];
        if (!slot || slot->mType != type)
            return nullptr;

        return &slot;
    }

public:
    rp<MirvObject> Get(uint64_t handle, MirvObjectType type) const {
        const std::lock_guard<std::mutex> lock(mMutex);
        const auto& slot = GetSlot(handle, type);
        if (!slot)
            return nullptr;
        return *slot;
    }

    rp<MirvObject> Remove(const uint64_t handle, const MirvObjectType type) {
        const std::lock_guard<std::mutex> lock(mMutex);
        const auto& slot = GetSlot(handle, type);
        if (!slot)
            return nullptr; // Already removed.

        auto ret = *slot;
        *slot = nullptr;
        mFreeList.push(handle);
        return ret;
    }
};

//////////////////////////////////////////////

namespace Handles {

HandleTable gHandleTable;

uint64_t
AddT(MirvObject* const obj)
{
    return gHandleTable.Add(obj);
}

rp<MirvObject>
GetT(const uint64_t handle, const MirvObjectType type)
{
    return gHandleTable.Get(handle, type);
}

rp<MirvObject>
RemoveT(const uint64_t handle, const MirvObjectType type)
{
    return gHandleTable.Remove(handle, type);
}

} // namespace Handles

//////////////////////////////////////////////

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

// --

MirvDevice::MirvDevice(MirvPhysicalDevice& physDev)
    : MirvObject(MirvObjectType::Device)
    , mPhysDev(physDev)
{ }

MirvDevice::~MirvDevice() = default;

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