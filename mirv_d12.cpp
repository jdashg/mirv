#include "mirv_d12.h"

#include <dxgi1_4.h>
#include <d3d11_1.h>
#include <d3d12.h>

#include <algorithm>
#include <map>

// --

MirvInstanceBackend*
MirvInstance::CreateBackend_D3D12()
{
    const uint32_t flags = DXGI_CREATE_FACTORY_DEBUG;

    rp<IDXGIFactory1> factory;
    CreateDXGIFactory2(flags, __uuidof(IDXGIFactory4), (void**)factory.asOutVar());
    if (!factory) {
        CreateDXGIFactory2(flags, __uuidof(IDXGIFactory1), (void**)factory.asOutVar());
    }

    if (!factory)
        return nullptr;

    return new MirvInstanceBackend_D12(factory);
}

// -------------------------------------

MirvInstanceBackend_D12::MirvInstanceBackend_D12(IDXGIFactory1* const factory)
    : mFactory(factory)
{ }

MirvInstanceBackend_D12::~MirvInstanceBackend_D12() = default;

const std::vector<rp<MirvPhysicalDevice>>
MirvInstanceBackend_D12::EnumeratePhysicalDevices()
{
    std::vector<rp<IDXGIAdapter1>> adapters;

    rp<IDXGIAdapter1> cur;
    for (uint32_t i = 0; true; i++) {
        mFactory->EnumAdapters1(i, cur.asOutVar());
        if (!cur)
            break;
        adapters.push_back(cur);
    }

#ifdef DEBUG
    const auto factory4 = QI<IDXGIFactory4>::From(mFactory);
    if (factory4) {
        rp<IDXGIAdapter1> cur;
        factory4->EnumWarpAdapter(__uuidof(IDXGIAdapter1), cur.asOutVar());
        assert((std::count(adapters.cbegin(), adapters.cend(), cur) == 1));
    }
#endif

    std::vector<rp<MirvPhysicalDevice>> ret;
    for (const auto& cur : adapters) {
        LARGE_INTEGER umdVersion;
        if (FAILED(cur->CheckInterfaceSupport(__uuidof(ID3D12Device), &umdVersion)))
            continue;

        DXGI_ADAPTER_DESC1 desc;
        if (FAILED(cur->GetDesc1(&desc)))
            continue;

        ret.push_back(new MirvPhysicalDevice_D12(cur, umdVersion, desc));
    }

    return ret;
}

// -------------------------------------

MirvPhysicalDevice_D12::MirvPhysicalDevice_D12(IDXGIAdapter1* const adapter,
                                               const LARGE_INTEGER& umdVersion,
                                               const DXGI_ADAPTER_DESC1& desc)
    : mAdapter(adapter)
{
    if (umdVersion.HighPart) {
        mProperties.driverVersion = uint32_t(umdVersion.HighPart);
    } else {
        mProperties.driverVersion = uint32_t(umdVersion.LowPart);
    }
    mProperties.vendorID = desc.VendorId;
    mProperties.deviceID = desc.DeviceId;

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        mProperties.deviceType = VK_PHYSICAL_DEVICE_TYPE_CPU;
    } else if (desc.DedicatedVideoMemory) {
        mProperties.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    } else {
        mProperties.deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    }

    const std::wstring wdesc = desc.Description;
    const std::string ndesc(wdesc.cbegin(), wdesc.cend());
    mProperties.deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE-1] = 0;
    strncpy(mProperties.deviceName, ndesc.c_str(), VK_MAX_PHYSICAL_DEVICE_NAME_SIZE-1);

    ////
    // https://msdn.microsoft.com/en-us/library/windows/desktop/mt186615(v=vs.85).aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff819065(v=vs.85).aspx

    mLimits.maxImageDimension1D = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    mLimits.maxImageDimension2D = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    mLimits.maxImageDimension3D = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    mLimits.maxImageDimensionCube = D3D11_REQ_TEXTURECUBE_DIMENSION;
    mLimits.maxImageArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    mLimits.maxTexelBufferElements = D3D11_REQ_BUFFER_RESOURCE_TEXEL_COUNT_2_TO_EXP;
    mLimits.maxUniformBufferRange = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT;
    //mLimits.maxStorageBufferRange;
    //mLimits.maxPushConstantsSize;
    //mLimits.maxMemoryAllocationCount;
    //mLimits.maxSamplerAllocationCount;
    mLimits.bufferImageGranularity = D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
    //mLimits.sparseAddressSpaceSize;
    //mLimits.maxBoundDescriptorSets;
    //mLimits.maxPerStageDescriptorSamplers;
    //mLimits.maxPerStageDescriptorUniformBuffers;
    //mLimits.maxPerStageDescriptorStorageBuffers;
    //mLimits.maxPerStageDescriptorSampledImages;
    //mLimits.maxPerStageDescriptorStorageImages;
    //mLimits.maxPerStageDescriptorInputAttachments;
    //mLimits.maxPerStageResources;
    //mLimits.maxDescriptorSetSamplers;
    //mLimits.maxDescriptorSetUniformBuffers;
    //mLimits.maxDescriptorSetUniformBuffersDynamic;
    //mLimits.maxDescriptorSetStorageBuffers;
    //mLimits.maxDescriptorSetStorageBuffersDynamic;
    //mLimits.maxDescriptorSetSampledImages;
    //mLimits.maxDescriptorSetStorageImages;
    //mLimits.maxDescriptorSetInputAttachments;
    mLimits.maxVertexInputAttributes = 4*D3D11_VS_INPUT_REGISTER_COUNT;
    //mLimits.maxVertexInputBindings;
    //mLimits.maxVertexInputAttributeOffset;
    //mLimits.maxVertexInputBindingStride;
    mLimits.maxVertexOutputComponents = 4*D3D11_VS_OUTPUT_REGISTER_COUNT;
    //mLimits.maxTessellationGenerationLevel;
    //mLimits.maxTessellationPatchSize;
    //mLimits.maxTessellationControlPerVertexInputComponents;
    //mLimits.maxTessellationControlPerVertexOutputComponents;
    //mLimits.maxTessellationControlPerPatchOutputComponents;
    //mLimits.maxTessellationControlTotalOutputComponents;
    //mLimits.maxTessellationEvaluationInputComponents;
    //mLimits.maxTessellationEvaluationOutputComponents;
    //mLimits.maxGeometryShaderInvocations;
    mLimits.maxGeometryInputComponents = 4*D3D11_GS_INPUT_REGISTER_COUNT;
    mLimits.maxGeometryOutputComponents = 4*D3D11_GS_OUTPUT_REGISTER_COUNT;
    mLimits.maxGeometryOutputVertices = D3D11_GS_MAX_OUTPUT_VERTEX_COUNT_ACROSS_INSTANCES;
    //mLimits.maxGeometryTotalOutputComponents;
    mLimits.maxFragmentInputComponents = 4*D3D11_PS_INPUT_REGISTER_COUNT;
    mLimits.maxFragmentOutputAttachments = 8; // "Simultaneous Render Targets"
    //mLimits.maxFragmentDualSrcAttachments;
    //mLimits.maxFragmentCombinedOutputResources;
    //mLimits.maxComputeSharedMemorySize;
    //mLimits.maxComputeWorkGroupCount[3];
    //mLimits.maxComputeWorkGroupInvocations;
    //mLimits.maxComputeWorkGroupSize[3];
    //mLimits.subPixelPrecisionBits;
    //mLimits.subTexelPrecisionBits;
    //mLimits.mipmapPrecisionBits;
    mLimits.maxDrawIndexedIndexValue = D3D11_REQ_DRAWINDEXED_INDEX_COUNT_2_TO_EXP - 1;
    mLimits.maxDrawIndirectCount = D3D11_REQ_DRAW_VERTEX_COUNT_2_TO_EXP - 1;
    //mLimits.maxSamplerLodBias;
    mLimits.maxSamplerAnisotropy = D3D11_REQ_MAXANISOTROPY;
    //mLimits.maxViewports;
    //mLimits.maxViewportDimensions[2];
    //mLimits.viewportBoundsRange[2];
    //mLimits.viewportSubPixelBits;
    //mLimits.minMemoryMapAlignment;
    //mLimits.minTexelBufferOffsetAlignment;
    //mLimits.minUniformBufferOffsetAlignment;
    //mLimits.minStorageBufferOffsetAlignment;
    //mLimits.minTexelOffset;
    //mLimits.maxTexelOffset;
    //mLimits.minTexelGatherOffset;
    //mLimits.maxTexelGatherOffset;
    //mLimits.minInterpolationOffset;
    //mLimits.maxInterpolationOffset;
    //mLimits.subPixelInterpolationOffsetBits;
    //mLimits.maxFramebufferWidth;
    //mLimits.maxFramebufferHeight;
    //mLimits.maxFramebufferLayers;
    //mLimits.framebufferColorSampleCounts;
    //mLimits.framebufferDepthSampleCounts;
    //mLimits.framebufferStencilSampleCounts;
    //mLimits.framebufferNoAttachmentsSampleCounts;
    //mLimits.maxColorAttachments;
    //mLimits.sampledImageColorSampleCounts;
    //mLimits.sampledImageIntegerSampleCounts;
    //mLimits.sampledImageDepthSampleCounts;
    //mLimits.sampledImageStencilSampleCounts;
    //mLimits.storageImageSampleCounts;
    //mLimits.maxSampleMaskWords;
    //mLimits.timestampComputeAndGraphics;
    //mLimits.timestampPeriod;
    //mLimits.maxClipDistances;
    //mLimits.maxCullDistances;
    //mLimits.maxCombinedClipAndCullDistances;
    //mLimits.discreteQueuePriorities;
    //mLimits.pointSizeRange[2];
    //mLimits.lineWidthRange[2];
    //mLimits.pointSizeGranularity;
    //mLimits.lineWidthGranularity;
    //mLimits.strictLines;
    //mLimits.standardSampleLocations;
    //mLimits.optimalBufferCopyOffsetAlignment;
    //mLimits.optimalBufferCopyRowPitchAlignment;
    //mLimits.nonCoherentAtomSize;

    ////

    VkQueueFamilyProperties queueFamily = {};
    queueFamily.queueCount = UINT32_MAX;
    queueFamily.timestampValidBits = 0; // 0 means unsupported
    queueFamily.minImageTransferGranularity = {1,1,1};

    queueFamily.queueFlags = VK_QUEUE_GRAPHICS_BIT;
    mQueueFamilyProperties.push_back(queueFamily);

    queueFamily.queueFlags = VK_QUEUE_COMPUTE_BIT;
    mQueueFamilyProperties.push_back(queueFamily);

    queueFamily.queueFlags = VK_QUEUE_TRANSFER_BIT;
    mQueueFamilyProperties.push_back(queueFamily);
}

MirvPhysicalDevice_D12::~MirvPhysicalDevice_D12() = default;

VkResult
MirvPhysicalDevice_D12::CreateDevice(const VkDeviceCreateInfo* const createInfo,
                                     const VkAllocationCallbacks* const allocator,
                                     rp<MirvDevice>* const out_device) const
{
    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
    rp<ID3D12Device> d3dDev;
    const auto hr = D3D12CreateDevice(mAdapter, featureLevel, __uuidof(ID3D12Device),
                                      (void**)d3dDev.asOutVar());
    if (FAILED(hr))
        return VK_ERROR_INITIALIZATION_FAILED;

    rp<MirvDevice_D12> dev = new MirvDevice_D12(this, d3dDev);

    auto queueInfoItr = createInfo->pQueueCreateInfos;
    const auto& queueInfoEnd = queueInfoItr + createInfo->queueCreateInfoCount;
    for (; queueInfoItr != queueInfoEnd; ++queueInfoItr) {
        const auto res = dev->CreateQueues(queueInfoItr);
        if (res != VK_SUCCESS)
            return res;
    }
    *out_device = dev;
    return VK_SUCCESS;
}

// -------------------------------------

MirvQueue_D12::MirvQueue_D12(ID3D12CommandQueue* const queue)
    : mQueue(queue)
{ }

MirvQueue_D12::~MirvQueue_D12() = default;

// --

MirvDevice_D12::MirvDevice_D12(MirvPhysicalDevice_D12* const physDev,
                               ID3D12Device* const device)
    : MirvDevice(physDev)
    , mDevice(device)
{ }

MirvDevice_D12::~MirvDevice_D12() = default;

VkResult
MirvDevice_D12::CreateQueues(const VkDeviceQueueCreateInfo* const info)
{
    const auto& familyIndex = info->queueFamilyIndex;
    if (familyIndex >= mPhysDev->mQueueFamilyProperties.size())
        return VK_ERROR_INITIALIZATION_FAILED;
    const auto& familyInfo = mPhysDev->mQueueFamilyProperties[familyIndex];

    const auto res = mQueuesByFamily.insert({familyIndex,
                                             std::vector<rp<MirvQueue_D12>>()});
    const auto& didInsert = res.second;
    if (!didInsert)
        return VK_ERROR_INITIALIZATION_FAILED;
    auto& queues = *(itr.first);

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = [&]() {
        switch (familyInfo.queueFlags) {
        case VK_QUEUE_GRAPHICS_BIT:
            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        case VK_QUEUE_COMPUTE_BIT:
            return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case VK_QUEUE_TRANSFER_BIT:
            return D3D12_COMMAND_LIST_TYPE_COPY;
        default:
            abort();
        }
    }();

    for (uint32_t i = 0; i < info->queueCount; i++) {
        rp<ID3D12CommandQueue> queue;
        const auto hr = mDevice->CreateCommandQueue(desc, __uuidof(ID3D12CommandQueue),
                                                    queue.AsOutVar());
        if (FAILED(hr)) {
            assert(hr == E_OUTOFMEMORY)
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        const rp<MirvQueue_D12> mirvQueue = new MirvQueue_D12(queue);
        queues.push_back(mirvQueue);
    }
    return VK_SUCCESS;
}
