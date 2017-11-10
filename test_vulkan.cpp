#include "vulkan.h"
#include <windows.h>

#include <cassert>
#include <cstdio>
#include <vector>

#include "util.h"

int
main(const int argc, const char* const argv[])
{
    const VkApplicationInfo appInfo = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr,
        "test_vulkan", 1,
        nullptr, 0,
        VK_API_VERSION_1_0
    };

    const VkInstanceCreateInfo instCreateInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0,
        &appInfo,
        0, nullptr,
        0, nullptr
    };

    VkInstance inst;
    auto res = vkCreateInstance(&instCreateInfo, nullptr, &inst);
    ASSERT(res == VK_SUCCESS)
    ASSERT(inst)

    std::vector<VkPhysicalDevice> physDevs;
    uint32_t numPhysDevs = 0;
    (void)vkEnumeratePhysicalDevices(inst, &numPhysDevs, nullptr);
    ASSERT(numPhysDevs)
    printf("numPhysDevs: %u\n", numPhysDevs);

    physDevs.resize(numPhysDevs);
    (void)vkEnumeratePhysicalDevices(inst, &numPhysDevs, physDevs.data());

    std::vector<VkQueueFamilyProperties> families;

    VkDevice dev = 0;
    VkQueue queue = 0;

    for (const auto& physDev : physDevs) {
        uint32_t familyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physDev, &familyCount, nullptr);
        families.resize(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physDev, &familyCount, families.data());
        for (uint32_t i = 0; i < familyCount; i++) {
            const auto& fam = families[i];
            if (!(fam.queueFlags & VK_QUEUE_TRANSFER_BIT))
                continue;

            const float priorities[] = { 0.5f };
            const VkDeviceQueueCreateInfo queueInfo = {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,
                i, 1,
                priorities
            };
            const VkDeviceCreateInfo deviceInfo = {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0,
                1, &queueInfo,
                0, nullptr,
                0, nullptr,
                nullptr
            };

            res = vkCreateDevice(physDev, &deviceInfo, nullptr, &dev);
            ASSERT(res == VK_SUCCESS)
            if (dev) {
                vkGetDeviceQueue(dev, i, 0, &queue);
                break;
            }
        }
        if (dev)
            break;
    }
    ASSERT(dev);
    ASSERT(queue);

    printf("OK!\n");
    return 0;
}
