#pragma once

#include "vulkan.h"

#include <cassert>
#include <map>
#include <mutex>
#include <vector>

#define HAS_D3D12
//#define HAS_METAL
//#define HAS_VULKAN

#define VK_ERROR_NOT_IMPLEMENTED -2000000000

////

template<typename T>
void
Zero(T* const data)
{
    memset(data, 0, sizeof(*data));
}

// -------------------------------------

class RefCounted
{
    mutable size_t mRefCount;

protected:
    RefCounted()
        : mRefCount(0)
    {}

    virtual ~RefCounted() = 0;

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
class ObjectTracker
{
    mutable std::mutex mMutex;
    std::vector<rp<objectT>> mValid;

public:
    handleT Track(objectT* const obj) {
        assert(obj);
        {
            const std::lock_guard<std::mutex> guard(mMutex);
            mValid.push_back(AsRP(obj));
        }
        return Handle(obj);
    }

    void Untrack(const objectT* const obj) {
        const std::lock_guard<std::mutex> guard(mMutex);
        std::remove(mValid.begin(), mValid.end(), obj);
    }

    objectT* GetTracked(const handleT handle) const {
        if (!handle)
            return nullptr;

        const std::lock_guard<std::mutex> guard(mMutex);

        const auto obj = (objectT*)handle;
        if (!std::count(mValid.cbegin(), mValid.cend(), obj))
            return nullptr;
        return obj;
    }

    handleT Handle(const objectT* const obj) const {
#ifdef DEBUG
        const std::lock_guard<std::mutex> guard(mMutex);
        assert(std::count(mValid.cbegin(), mValid.cend(), obj) == 1);
#endif
        return (handleT)obj;
    }

    std::vector<handleT> Enumerate() const {
        std::vector<handleT> ret;
        const std::lock_guard<std::mutex> guard(mMutex);
        for (const auto& cur : mValid) {
            ret.push_back(Handle(cur.get()));
        }
        return ret;
    }

    size_t Count() const {
        const std::lock_guard<std::mutex> guard(mMutex);
        return mValid.size();
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

    ObjectTracker<VkPhysicalDevice, MirvPhysicalDevice> mPhysicalDevices;
    void EnsurePhysicalDevices();

public:
    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices() {
        EnsurePhysicalDevices();
        return mPhysicalDevices.Enumerate();
    }
};

// --

class MirvInstanceBackend
    : public RefCounted
{
public:
    virtual const std::vector<rp<MirvPhysicalDevice>> EnumeratePhysicalDevices() = 0;
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
                                  rp<MirvDevice>* out_device) const = 0;
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
