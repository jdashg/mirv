#include "mirv.h"

#include <memory>
#include <mutex>

//////////////////////////////////////////////

ObjectTracker<VkInstance, MirvInstance> gInstances;
ObjectTracker<VkPhysicalDevice, MirvPhysicalDevice> gPhysicalDevices;
ObjectTracker<VkDevice, MirvDevice> gDevices;
ObjectTracker<VkQueue, MirvQueue> gQueues;

//////////////////////////////////////////////

#ifndef HAS_D3D12
MirvInstanceBackend* MirvInstance::CreateBackend_D3D12() { return nullptr; }
#endif
#ifndef HAS_METAL
MirvInstanceBackend* MirvInstance::CreateBackend_Metal() { return nullptr; }
#endif
#ifndef HAS_VULKAN
MirvInstanceBackend* MirvInstance::CreateBackend_Vulkan() { return nullptr; }
#endif

void
MirvInstance::EnsureBackends()
{
    if (mBackends.size())
        return;

    const auto fnAdd = [&](MirvInstanceBackend* const backend) {
        if (backend) {
            mBackends.push_back(AsRP(backend));
        }
    };

    fnAdd(CreateBackend_D3D12());
    fnAdd(CreateBackend_Metal());
    fnAdd(CreateBackend_Vulkan());
}

std::vector<VkPhysicalDevice>
MirvInstance::EnumeratePhysicalDevices()
{
    EnsureBackends();

    std::vector<VkPhysicalDevice> ret;
    for (const auto& backend : mBackends) {
        const auto& perBackend = backend->EnumeratePhysicalDevices();
        ret.insert(ret.end(), perBackend.begin(), perBackend.end());
    }
    return ret;
}

// --

MirvDevice::MirvDevice(MirvPhysicalDevice* const physDev)
    : mPhysDev(physDev)
{ }

MirvDevice::~MirvDevice() = default;
