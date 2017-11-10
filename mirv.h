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
        *out_count = src.size();
        return VK_SUCCESS;
    }

    VkResult res;
    if (*out_count < src.size()) {
        res = VK_INCOMPLETE;
    } else {
        res = VK_SUCCESS;
        *out_count = src.size();
    }

    std::copy_n(src.cbegin(), *out_count, out_begin);
    return res;
}

template<typename T>
bool
SnapshotStruct(T* const dest, const T* const src, const VkStructureType type)
{
    const auto srcType = src->sType;
    if (srcType != type)
        return false;

    Copy(dest, src);
    dest->sType = srcType; // Catch racey copies.
    return true;
}

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

class HandleTable;

struct MirvObject : public RefCounted
{
    friend class HandleTable;

    const MirvObjectType mType;
private:
    uint64_t mHandle;
public:
    mutable std::mutex mMutex;

    explicit MirvObject(const MirvObjectType type)
        : mType(type)
        , mHandle(0)
    { }

    DECL_GETTER(Handle);
};

// --


// -------------------------------------

class MirvPhysicalDevice;

class MirvInstance
    : public MirvObject
{
public:
    std::vector<rp<MirvPhysicalDevice>> mPhysDevs;

    explicit MirvInstance();
    ~MirvInstance();

private:
    template<Backends B>
    void AddPhysDevs();
};

// --

class MirvDevice;

class MirvPhysicalDevice
    : public MirvObject
{
public:
    MirvInstance& mInstance;

    VkPhysicalDeviceProperties mProperties;
    VkPhysicalDeviceLimits mLimits;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;

protected:
    MirvPhysicalDevice(MirvInstance& instance)
        : MirvObject(MirvObjectType::PhysicalDevice)
        , mInstance(instance)
    {
        Zero(&mProperties);
        Zero(&mLimits);
        mProperties.apiVersion = VK_API_VERSION_1_0;
    }

public:
    virtual VkResult CreateDevice(const VkDeviceCreateInfo& createInfo,
                                  rp<MirvDevice>* out_device) = 0;
};

// --

class MirvQueue;

class MirvDevice
    : public MirvObject
{
public:
    MirvPhysicalDevice& mPhysDev;

    std::map< uint32_t, std::vector<rp<MirvQueue>> > mQueuesByFamily;

    explicit MirvDevice(MirvPhysicalDevice& physDev);
    ~MirvDevice() override;

public:
    VkResult AddAllQueues(const VkDeviceCreateInfo& info);

protected:
    virtual VkResult AddQueues(const VkDeviceQueueCreateInfo& info,
                               const VkQueueFamilyProperties& familyInfo,
                               std::vector<rp<MirvQueue>>* out) = 0;
};

// --

class MirvQueue
    : public MirvObject
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

// -----------------

namespace Handles {

uint64_t AddT(MirvObject* obj);
rp<MirvObject> GetT(uint64_t handle, MirvObjectType type);
rp<MirvObject> RemoveT(uint64_t handle, MirvObjectType type);

#define _(X) \
inline Vk##X \
Add(Mirv##X* const obj) \
{ \
    return Vk##X(AddT(obj)); \
} \
inline rp<Mirv##X> \
Get(const Vk##X handle) \
{ \
    const auto& obj = GetT(uint64_t(handle), MirvObjectType::X); \
    return static_cast<Mirv##X*>(obj.get()); \
} \
inline rp<Mirv##X> \
Remove(const Vk##X handle) \
{ \
    const auto& obj = RemoveT(uint64_t(handle), MirvObjectType::X); \
    return static_cast<Mirv##X*>(obj.get()); \
}

_(Instance)
_(PhysicalDevice)
_(Device)
_(Queue)

#undef _

template<typename T>
inline rp<T>
Remove(const T* const obj)
{
    const auto& ret = RemoveT(obj);
    return static_cast<T*>(ret);
}

} // namespace Handles
