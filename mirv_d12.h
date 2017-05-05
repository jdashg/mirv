#pragma once

#include "mirv.h"

#include <windows.h>

class ID3D12CommandQueue;
class ID3D12Device;
class IDXGIAdapter1;
class IDXGIFactory1;
struct DXGI_ADAPTER_DESC1;

// --

class MirvInstanceBackend_D12 final : public MirvInstanceBackend
{
    const rp<IDXGIFactory1> mFactory;

public:
    explicit MirvInstanceBackend_D12(IDXGIFactory1* factory);
    ~MirvInstanceBackend_D12() override;

    virtual const std::vector<rp<MirvPhysicalDevice>> EnumeratePhysicalDevices() override;
};

// --

class MirvPhysicalDevice_D12 final : public MirvPhysicalDevice
{
    const rp<IDXGIAdapter1> mAdapter;

public:
    MirvPhysicalDevice_D12(IDXGIAdapter1* adapter, const LARGE_INTEGER& umdVersion,
                           const DXGI_ADAPTER_DESC1& desc);
    ~MirvPhysicalDevice_D12() override;

    virtual VkResult CreateDevice(const VkDeviceCreateInfo* createInfo,
                                  const VkAllocationCallbacks* allocator,
                                  rp<MirvDevice>* out_device) const override;
};

// --

class MirvQueue_D12 final : public MirvQueue
{
    const rp<ID3D12CommandQueue> mQueue;

public:
    explicit MirvQueue_D12(ID3D12CommandQueue* queue);
    ~MirvQueue_D12() override;
};

// --

class MirvDevice_D12 final : public MirvDevice
{
    const rp<ID3D12Device> mDevice;

    std::map< uint32_t, std::vector<rp<MirvQueue_D12>> > mQueuesByFamily;

public:
    MirvDevice_D12(const std::vector<rp<MirvQueue>>& queues, ID3D12Device* device);
    ~MirvDevice_D12() override;

    VkResult AddQueue(const VkDeviceQueueCreateInfo* info);
};
