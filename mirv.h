#pragma once

#include "vulkan.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stack>
#include <unordered_map>
#include <vector>

#include "util.h"

#define VK_ERROR_NOT_IMPLEMENTED VkResult(-2000*1000*1000)

template<typename T>
VkResult
VulkanArrayCopyMeme(const std::vector<T>& src, uint32_t* const out_count, T* const out_begin)
{
    if (!out_begin) {
        *out_count = (uint32_t)src.size();
        return VK_SUCCESS;
    }

    VkResult res;
    if (*out_count < src.size()) {
        res = VK_INCOMPLETE;
    } else {
        res = VK_SUCCESS;
        *out_count = (uint32_t)src.size();
    }

    std::copy_n(src.cbegin(), *out_count, out_begin);
    return res;
}

using mutex_guard = std::lock_guard<std::mutex>;

// -------------------------------------

class RefCounted
{
    mutable std::atomic<size_t> mRefCount;

protected:
    RefCounted()
        : mRefCount(0)
    {}

    virtual ~RefCounted() {}

public:
    void AddRef() const {
        const auto res = ++mRefCount;
        //printf("%p: AddRef => %u\n", this, res);
        ASSERT(res > 0)
    }

    void Release() const {
        const auto res = --mRefCount;
        //printf("%p: Release => %u\n", this, res);
        ASSERT(res < SIZE_MAX/2)
        if (res > 0)
            return;

        delete this;
    }
};

//#define SPEW_RP

template<typename T>
class rp final
{
    T* mPtr;

    template<typename U> friend class rp;

    inline void Swap(T* const x)
    {
#ifdef SPEW_RP
        printf("[%p] %p => %p\n", this, mPtr, x);
#endif
        if (x) {
            x->AddRef();
        }
        const auto was = mPtr;
        mPtr = x;
        if (was) {
            was->Release();
        }
    }

public:
    rp()
        : mPtr(nullptr)
    { }

    rp(nullptr_t)
        : mPtr(nullptr)
    { }

    rp(const rp<T>& x)
        : mPtr(nullptr)
    {
        Swap(x.mPtr);
    }
    rp(rp<T>&& x)
        : mPtr(x.mPtr)
    {
        x.mPtr = nullptr;
    }

    template<typename U>
    rp(U* const x)
        : mPtr(nullptr)
    {
        Swap(x);
    }

    template<typename U>
    explicit rp(const rp<U>& x)
        : mPtr(nullptr)
    {
        Swap(x.mPtr);
    }
    /*
    template<typename U>
    explicit rp(rp<U>&& x)
    {
        mPtr = x.mPtr;
        x.mPtr = nullptr;
    }
    */
    ////

    ~rp()
    {
        Swap(nullptr);
    }

    ////

    template<typename U>
    rp<T>& operator =(U* const x)
    {
        Swap(x);
        return *this;
    }

    template<typename U>
    rp<T>& operator =(const rp<U>& x)
    {
        ASSERT((uintptr_t)&x != (uintptr_t)this)
        Swap(x.mPtr);
        return *this;
    }

    rp<T>& operator =(const rp<T>& x)
    {
        Swap(x.mPtr);
        return *this;
    }
    rp<T>& operator =(rp<T>&& x)
    {
        std::swap(mPtr, x.mPtr);
        return *this;
    }

    ////

    T* get() const { return mPtr; }
    //operator T*() const { return mPtr; }

    T** asOutVar() {
        Swap(nullptr);
#ifdef SPEW_RP
        printf("[%p] asOutVar\n", this);
#endif
        return &mPtr;
    }

    ////

    T* operator ->() const { return mPtr; }
    operator bool() const { return bool(mPtr); }

    bool operator <(const rp<T>& x) const {
        return mPtr < x.mPtr;
    }

    template<typename U>
    bool operator ==(const rp<U>& x) const {
        return mPtr == x.mPtr;
    }

    template<typename U>
    bool operator !=(const rp<U>& x) const { return !(*this == x); }
};

template<typename T>
rp<T>
AsRP(T* const x) {
    return rp<T>(x);
}

template<typename T>
struct QI final
{
    template<typename U>
    static rp<T> From(const U& x) {
        rp<T> ret;
        x->QueryInterface(__uuidof(T), (void**)ret.asOutVar());
        return ret;
    }
};

// -------------------------------------

enum class MirvObjectType {
    Instance,
    PhysicalDevice,
    Device,
    Queue,
};

enum class Backends {
    D3D12,
    Metal,
    Vulkan,
};

// --

template<typename DerivedT, typename DerivedHandleT>
struct MirvObject : public RefCounted
{
    typedef DerivedHandleT HandleT;
    typedef MirvObject<DerivedT,DerivedHandleT> ObjectT;

    const MirvObjectType mType;
    mutable std::mutex mMutex;

    explicit MirvObject(const MirvObjectType type)
        : mType(type)
    { }

    HandleT Handle() const {
        return reinterpret_cast<HandleT>(this);
    }

    static DerivedT* For(const HandleT handle) {
        ASSERT(handle)
        return reinterpret_cast<DerivedT*>(handle);
    }
};

// -------------------------------------

class MirvPhysicalDevice;

class MirvInstance
    : public MirvObject<MirvInstance, VkInstance>
{
    static std::mutex gMutex;
    static std::set<rp<MirvInstance>> gInstances;

    std::vector<rp<MirvPhysicalDevice>> mPhysDevs;

    MirvInstance();
    ~MirvInstance();

public:
    static VkResult vkCreateInstance(const VkInstanceCreateInfo& createInfo,
                                     MirvInstance** out);
    VkResult vkEnumeratePhysicalDevices(VkInstance handle,
                                        uint32_t* out_physicalDeviceCount,
                                        MirvPhysicalDevice** out_physicalDevices) const;
    void vkDestroyInstance();

private:
    template<Backends B>
    void AddPhysDevs();
};

// --

class MirvDevice;

class MirvPhysicalDevice
    : public MirvObject<MirvPhysicalDevice, VkPhysicalDevice>
{
public:
    MirvInstance& mInstance;

    VkPhysicalDeviceProperties mProperties;
    VkPhysicalDeviceLimits mLimits;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;

protected:
    std::set<rp<MirvDevice>> mDevices;

    MirvPhysicalDevice(MirvInstance& instance)
        : MirvObject(MirvObjectType::PhysicalDevice)
        , mInstance(instance)
    {
        Zero(&mProperties);
        Zero(&mLimits);
        mProperties.apiVersion = VK_API_VERSION_1_0;
    }

    virtual VkResult CreateDevice(const VkDeviceCreateInfo& createInfo,
                                  rp<MirvDevice>* out) = 0;

public:
    VkResult vkCreateDevice(const VkDeviceCreateInfo& createInfo,
                            MirvDevice** out);
};

// --

class MirvQueue;
class MirvCommandPool;

class MirvDevice
    : public MirvObject<MirvDevice, VkDevice>
{
public:
    MirvPhysicalDevice& mPhysDev;

    std::map< uint32_t, std::vector<rp<MirvQueue>> > mQueuesByFamily;

    explicit MirvDevice(MirvPhysicalDevice& physDev);
    ~MirvDevice() override;

public:
    void vkGetDeviceQueue(uint32_t queueFamilyIndex, uint32_t queueIndex,
                          MirvQueue** out) const;
    VkResult vkCreateCommandPool(const VkCommandPoolCreateInfo& createInfo,
                                 MirvCommandPool** out) const;
    void vkDestroyDevice() { }

    VkResult AddAllQueues(const VkDeviceCreateInfo& info);

protected:
    virtual VkResult AddQueues(const VkDeviceQueueCreateInfo& info,
                               const VkQueueFamilyProperties& familyInfo,
                               std::vector<rp<MirvQueue>>* out) = 0;
};

// --

class MirvQueue
    : public MirvObject<MirvQueue, VkQueue>
{
public:
    MirvDevice& mDevice;
    const VkQueueFamilyProperties& mFamily;

    MirvQueue(MirvDevice& device, const VkQueueFamilyProperties& family)
        : MirvObject(MirvObjectType::Queue)
        , mDevice(device)
        , mFamily(family)
    { }
};

// --

class MirvCommandPool
    : public MirvObject<MirvCommandPool, VkCommandPool>
{
};

// -----------------

template<typename T, typename U>
inline U MapHandle(const rp<T>& x) { return MapHandle(x.get()); }

#define _(X) constexpr X* MapHandle(const X::HandleT h) { return (X*)h; } \
             constexpr X** MapHandle(X::HandleT* const out_h) { return (X**)out_h; } \
             constexpr X::HandleT MapHandle(const X* const x) { return (X::HandleT)x; }
_(MirvInstance)
_(MirvPhysicalDevice)
_(MirvDevice)
_(MirvQueue)
_(MirvCommandPool)
#undef _
