#include "vulkan.h"
#include <windows.h>

int
main(const int argc, const char* const argv[])
{
    const auto lib = LoadLibrary("vulkan.dll");
    if (!lib)
        return -1;

    return 0;
}