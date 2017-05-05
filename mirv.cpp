#include "mirv.h"

#include <memory>
#include <mutex>

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

void
MirvInstance::EnsurePhysicalDevices()
{
    if (mPhysicalDevices.Count())
        return;

    EnsureBackends();
    for (const auto& backend : mBackends) {
        const auto& backendPDs = backend->EnumeratePhysicalDevices();
        for (const auto& pd : backendPDs) {
            (void)mPhysicalDevices.Track(pd);
        }
    }
}

// --

MirvDevice::MirvDevice(MirvPhysicalDevice* const physDev)
    : mPhysDev(physDev)
{ }

MirvDevice::~MirvDevice() = default;
