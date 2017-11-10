#pragma once

#include "mirv.h"

#include <windows.h>

struct ID3D12CommandQueue;
struct ID3D12Device;
struct IDXGIAdapter1;
struct IDXGIFactory1;
struct DXGI_ADAPTER_DESC1;

// --

class MirvPhysicalDevice_D12 final : public MirvPhysicalDevice
{
    const rp<IDXGIFactory1> mFactory;
    const rp<IDXGIAdapter1> mAdapter;

public:
    MirvPhysicalDevice_D12(MirvInstance& instance, IDXGIAdapter1* adapter,
                           const DXGI_ADAPTER_DESC1& desc);
    ~MirvPhysicalDevice_D12() override;

    VkResult CreateDevice(const VkDeviceCreateInfo& createInfo,
                          rp<MirvDevice>* out_device) override;
};

// --

class MirvDevice_D12 final : public MirvDevice
{
    const rp<ID3D12Device> mDevice;

public:
    MirvDevice_D12(MirvPhysicalDevice_D12& physDev, ID3D12Device* device);
    ~MirvDevice_D12() override;

    VkResult AddQueues(const VkDeviceQueueCreateInfo& info,
                       const VkQueueFamilyProperties& familyInfo,
                       std::vector<rp<MirvQueue>>* out) override;
};

// --

class MirvQueue_D12 final : public MirvQueue
{
    const rp<ID3D12CommandQueue> mQueue;

public:
    MirvQueue_D12(MirvDevice_D12& device, const VkQueueFamilyProperties& family,
                  ID3D12CommandQueue* queue);
    ~MirvQueue_D12() override;
};