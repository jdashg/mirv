#pragma once

// --

#ifdef DEBUG
#define ASSERT(x) \
    if (!(x)) { \
        printf("ASSERT(%s): %s:%u\n", #x, __FILE__, __LINE__); \
        *(uint8_t*)0 = 42; \
    }
#else
#define ASSERT(x)
#endif

// --

#ifdef DEBUG
#define ALWAYS_TRUE(x) ASSERT(x)
#else
#define ALWAYS_TRUE(x) (void)(x);
#endif

// --

#define DECL_GETTER(Name) const decltype(m##Name)& Name() const { return m##Name; }

// --

template<typename T>
void
Zero(T* const data)
{
    memset(data, 0, sizeof(*data));
}

template<typename T>
void
Copy(T* const dest, const T* const src)
{
    memcpy(dest, src, sizeof(T));
}

// --

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
