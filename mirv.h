#pragma once

#include "vulkan.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <mutex>
#include <stack>
#include <unordered_map>
#include <vector>

#define HAS_D3D12
//#define HAS_METAL
//#define HAS_VULKAN

#define VK_ERROR_NOT_IMPLEMENTED VkResult(-2000*1000*1000)

////

template<typename T>
void
Zero(T* const data)
{
    memset(data, 0, sizeof(*data));
}

// -------------------------------------

template<typename T>
struct RangeImpl final
{
    T* const mBegin;
    T* const mEnd;

    RangeImpl(T* const begin, T* const end)
        : mBegin(begin)
        , mEnd(end)
    { }

    T* begin() const { return mBegin; }
    T* end() const { return mEnd; }
};

template<typename T>
RangeImpl<T>
Range(T* const begin, T* const end)
{
    return RangeImpl<T>(begin, end);
}

template<typename T>
RangeImpl<T>
Range(T* const begin, const size_t n)
{
    return RangeImpl<T>(begin, begin + n);
}

// -------------------------------------

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

// -------------------------------------

class RefCounted
{
    mutable size_t mRefCount;

protected:
    RefCounted()
        : mRefCount(0)
    {}

    virtual ~RefCounted() {}

public:
    void AddRef() const {
        const auto res = ++mRefCount;
        assert(res > 0);
    }

    void Release() const {
        const auto res = --mRefCount;
        assert(res < SIZE_MAX/2);
        if (res > 0)
            return;

        delete this;
    }
};

template<typename T>
class rp final
{
    T* mPtr;

    template<typename U> friend class rp;

    inline void AddRef(T* const x)
    {
        mPtr = x;
        if (mPtr) {
            mPtr->AddRef();
        }
    }

    inline void Release()
    {
        const auto was = mPtr;
        mPtr = nullptr;
        if (was) {
            was->Release();
        }
    }

public:
    rp()
    {
        AddRef(nullptr);
    }

    rp(nullptr_t)
    {
        AddRef(nullptr);
    }

    template<typename U>
    rp(U* const x)
    {
        AddRef(x);
    }

    template<typename U>
    explicit rp(const rp<U>& x)
    {
        AddRef(x.mPtr);
    }

    template<typename U>
    explicit rp(rp<U>&& x)
    {
        mPtr = x.mPtr;
        x.mPtr = nullptr;
    }

    ////

    ~rp()
    {
        Release();
    }

    ////

    template<typename U>
    rp<T>& operator =(U* const x)
    {
        Release();
        AddRef(x);
        return *this;
    }

    template<typename U>
    rp<T>& operator =(const rp<U>& x)
    {
        Release();
        AddRef(x.mPtr);
        return *this;
    }

    ////

    T* get() const { return mPtr; }
    operator T*() const { return mPtr; }

    T** asOutVar() {
        Release();
        return &mPtr;
    }

    ////

    T* operator ->() const { return mPtr; }
    operator bool() const { return bool(mPtr); }

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

// -------------------------------------

template<typename handleT, typename objectT>
class ObjectTracker final
{
    mutable std::mutex mMutex;

    struct Slot final {
        std::mutex mutex;
        rp<objectT> object;
    };

    std::unordered_map<uint64_t, std::unique_ptr<Slot>> mSlots;
    std::unordered_map<objectT*, handleT> mHandleLookup;
    std::stack<handleT> mFreeList;
    uint64_t mNextId;

public:
    ObjectTracker()
        : mNextId(1)
    { }

    handleT Put(objectT* const obj) {
        const std::lock_guard<std::mutex> guard(mMutex);
        return Put_WithLock(obj);
    }

    rp<objectT> Get(const handleT handle) const {
        const std::lock_guard<std::mutex> guard(mMutex);
        return Get_WithLock(handle);
    }

    handleT Ensure(objectT* const obj) {
        const std::lock_guard<std::mutex> guard(mMutex);

        const auto itr = mHandleLookup.find(obj);
        if (itr == mHandleLookup.end())
            return Put(obj);

        return itr->second;
    }

    rp<objectT> Reset(const handleT handle) const {
        const std::lock_guard<std::mutex> guard(mMutex);

        const auto slot = Find(handle);
        if (!slot)
            return nullptr;

        const std::lock_guard<std::mutex> slotGuard(slot->mutex);
        const auto ret = slot->object;
        slot->object = nullptr;
        mHandleLookup.erase(ret);
        mFreeList.push(handle);
        return ret;
    }

private:
    handleT Put_WithLock(objectT* const obj) {
        handleT handle;
        if (mFreeList.size()) {
            handle = mFreeList.top();
            mFreeList.pop();
        } else {
            mSlots.insert({mNextId, std::unique_ptr<Slot>(new Slot)});
            handle = handleT(mNextId);
            mNextId += 1;
        }

        const auto slot = Find(handle);
        assert(slot);

        const std::lock_guard<std::mutex> slotGuard(slot->mutex);
        slot->object = obj;
        mHandleLookup.insert({obj, handle});
        return handle;
    }

    rp<objectT> Get_WithLock(const handleT handle) const {
        const auto slot = Find(handle);
        if (!slot)
            return nullptr;

        const std::lock_guard<std::mutex> slotGuard(slot->mutex);
        return slot->object;
    }

    // ---

    Slot* Find(const handleT handle) const {
        const auto itr = mSlots.find(uint64_t(handle));
        if (itr == mSlots.end())
            return nullptr;
        return itr->second.get();
    }
};

template<typename T>
struct QI final
{
    template<typename U>
    static rp<T> From(const rp<U>& x) {
        rp<T> ret;
        x->QueryInterface(__uuidof(T), ret.asOutVar());
        return ret;
    }
};

// -------------------------------------

class MirvInstanceBackend;
class MirvPhysicalDevice;

class MirvInstance
    : public RefCounted
{
    MirvInstanceBackend* CreateBackend_D3D12();
    MirvInstanceBackend* CreateBackend_Metal();
    MirvInstanceBackend* CreateBackend_Vulkan();

    std::vector<rp<MirvInstanceBackend>> mBackends;
    void EnsureBackends();

public:
    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
};

// --

class MirvInstanceBackend
    : public RefCounted
{
public:
    virtual std::vector<VkPhysicalDevice> EnumeratePhysicalDevices() = 0;
};

// --

class MirvDevice;

class MirvPhysicalDevice
    : public RefCounted
{
public:
    VkPhysicalDeviceProperties mProperties;
    VkPhysicalDeviceLimits mLimits;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;

protected:
    MirvPhysicalDevice()
    {
        Zero(&mProperties);
        Zero(&mLimits);
        mProperties.apiVersion = VK_API_VERSION_1_0;
    }

public:
    virtual VkResult CreateDevice(const VkDeviceCreateInfo* createInfo,
                                  const VkAllocationCallbacks* allocator,
                                  rp<MirvDevice>* out_device) = 0;
};

// --

class MirvQueue
    : public RefCounted
{
};

// --

class MirvDevice
    : public RefCounted
{
protected:
    const rp<MirvPhysicalDevice> mPhysDev;

    std::map<uint32_t,std::vector< rp<MirvQueue> >> mQueuesByFamily;

    explicit MirvDevice(MirvPhysicalDevice* physDev);
    ~MirvDevice() override;
};

// ---------------------

extern ObjectTracker<VkInstance, MirvInstance> gInstances;
extern ObjectTracker<VkPhysicalDevice, MirvPhysicalDevice> gPhysicalDevices;
extern ObjectTracker<VkDevice, MirvDevice> gDevices;
extern ObjectTracker<VkQueue, MirvQueue> gQueues;
