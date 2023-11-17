#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Validation layers
const uint32_t validation_layer_count = 1;
const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };

// Extensions
const uint32_t extension_count = 1;
const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NODEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif

// Structs

typedef struct App
{
    GLFWwindow *window;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device; // Logical device
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swap_chain;
    VkImageView *swap_chain_image_views;
} App;

typedef struct QueueFamilyIndices
{
    uint32_t graphics_family;
    bool has_graphics_family;

    uint32_t present_family;
    bool has_present_family;
} QueueFamilyIndices;

typedef struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t format_count;
    VkSurfaceFormatKHR *formats;
    uint32_t present_count;
    VkPresentModeKHR *present_modes;
} SwapChainDetails;

// Prototypes

/* GLFW */
void init_window(App *app);
void create_surface(App *app);

/* Vulkan functions */
void create_vulkan_instance(App *app);
bool check_validation_layer_support();
void pick_physical_device(App *app);
void create_logical_device(App *app);
QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);
SwapChainDetails query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface);
VkSurfaceFormatKHR choose_swap_surface_format(const VkSurfaceFormatKHR *available_formats, uint32_t format_count);
VkPresentModeKHR choose_swap_present_mode(const VkPresentModeKHR *available_present_modes, uint32_t present_count);
VkExtent2D choose_swap_extent(GLFWwindow *window, const VkSurfaceCapabilitiesKHR capabilities);
uint32_t clamp_u32(uint32_t n, uint32_t min, uint32_t max);
bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface);
void init_vulkan(App *app);
bool device_has_extension_support(VkPhysicalDevice device);
void create_swap_chain(App *app);
void create_image_views(App *app);

/* main and closing functions */
void main_loop(App *app);
void clean_up(App *app);

// Definitions

void init_window(App *app)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    app->window = glfwCreateWindow(800, 600, "test", NULL, NULL);
}

void create_surface(App *app)
{
    if (glfwCreateWindowSurface(app->instance, app->window, NULL, &app->surface) != VK_SUCCESS)
    {
        printf("Failed to create window surface!\n");
        exit(6);
    }
}

bool check_validation_layer_support()
{
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    VkLayerProperties available_layers[layer_count];
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    uint32_t found_layers = 0;
    for (int i = 0; i < validation_layer_count; i++)
    {
        bool layer_found = false;

        for (int j = 0; j < layer_count; j++)
        {
            if (strcmp(available_layers[j].layerName, validation_layers[i]) == 0)
            {
                layer_found = true;
                break;
            }
        }

        if (!layer_found)
        {
            return false;
        }
    }
    return true;
}

void create_vulkan_instance(App *app)
{
    if (enable_validation_layers && !check_validation_layer_support())
    {
        printf("Validation layers requested, but not available.\n");
        exit(1);
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions,
    };

    if(enable_validation_layers)
    {
        createInfo.enabledLayerCount = validation_layer_count;
        createInfo.ppEnabledLayerNames = validation_layers;
    }
    else
        createInfo.enabledLayerCount = 0;

    VkResult result = vkCreateInstance(&createInfo, NULL, &app->instance);

    if (result != VK_SUCCESS)
    {
        printf("Failed to initialize Vulkan\n");
        exit(2);
    }

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);

    VkExtensionProperties extensions[extensionCount];
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions);

    printf("Extensions found: %d\n", extensionCount);

    for (int i = 0; i < extensionCount; i++)
    {
        printf("Extension: %s\n", extensions[i].extensionName);
    }
}

uint32_t clamp_u32(uint32_t n, uint32_t min, uint32_t max)
{
    if (n < min) return min;
    if (n > max) return max;
    return n;
}

VkExtent2D choose_swap_extent(GLFWwindow *window, const VkSurfaceCapabilitiesKHR capabilities)
{
    if(capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actual_extent = { (uint32_t)width, (uint32_t)height };

        actual_extent.width = clamp_u32(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = clamp_u32(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }
}

void pick_physical_device(App *app)
{    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);

    if (device_count == 0)
    {
        printf("No Vulkan capable physical devices found!\n");
        exit(3);
    }

    VkPhysicalDevice devices[device_count];
    vkEnumeratePhysicalDevices(app->instance, &device_count, devices);

    for (int i = 0; i < device_count; i++)
    {
        if (is_device_suitable(devices[i], app->surface))
        {
            app->physical_device = devices[i];
            break;
        }
    }

    if (app->physical_device == VK_NULL_HANDLE)
    {
        printf("Failed to find suitable GPU.\n");
        exit(4);
    }

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(app->physical_device, &device_properties);

    printf("Physical Device: %s\n", device_properties.deviceName);
    printf(" - Device Driver: %d\n", device_properties.apiVersion);
}

QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    VkBool32 has_present_support = false;

    for (int i = 0; i < queue_family_count; i++)
    {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphics_family = i;
            indices.has_graphics_family = true;
        }

        has_present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &has_present_support);

        if (has_present_support)
        {
            indices.present_family = i;
            indices.has_present_family = true;
        }
    }

    return indices;
}

VkSurfaceFormatKHR choose_swap_surface_format(const VkSurfaceFormatKHR *available_formats, uint32_t format_count)
{
    for(int i = 0; i < format_count; i++)
    {
        if(available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && 
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return available_formats[i];
        }
    }

    return available_formats[0];
}

VkPresentModeKHR choose_swap_present_mode(const VkPresentModeKHR *available_present_modes, uint32_t present_count)
{
    for(int i = 0; i < present_count; i++)
    {
        if(available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return available_present_modes[i];
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

SwapChainDetails query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, NULL);
    details.format_count = format_count;
    VkSurfaceFormatKHR formats[format_count];
    details.formats = formats;
    if(format_count != 0)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats);
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, NULL);
    details.present_count = present_mode_count;
    VkPresentModeKHR present_modes[present_mode_count];
    details.present_modes = present_modes;
    if(present_mode_count != 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes);
    }

    return details;
}

bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices = find_queue_families(device, surface);
    SwapChainDetails swap_chain_details = query_swap_chain_support(device, surface);

    bool swap_chain_adequate = swap_chain_details.format_count != 0
                         && swap_chain_details.present_count != 0;

    if (indices.has_graphics_family)
        if (device_has_extension_support(device))
            if (swap_chain_adequate)
                return true;

    printf("Device has no family queues.\n");
    return false;
}

bool device_has_extension_support(VkPhysicalDevice device)
{
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    VkExtensionProperties available_extensions[extension_count];
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    for(uint32_t i = 0; i < extension_count; i++)
    {
        for(uint32_t j = 0; j < extension_count; j++)
        {
            if(strcmp(device_extensions[i], available_extensions[j].extensionName) == 0)
            {
                return true;
            }
        }
    } 

    return false;
}

void create_swap_chain(App *app)
{
    SwapChainDetails swap_chain_support = query_swap_chain_support(app->physical_device, app->surface);

    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats, swap_chain_support.format_count);
    VkExtent2D extent = choose_swap_extent(app->window, swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    uint32_t max_img_count = swap_chain_support.capabilities.maxImageCount;

    if (max_img_count > 0 && image_count > max_img_count)
    {
        image_count = max_img_count;
    }

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    };

    QueueFamilyIndices indices = find_queue_families(app->physical_device, app->surface);
    uint32_t queue_family_indices[] = {indices.graphics_family, indices.present_family};

    if (indices.graphics_family != indices.present_family)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = NULL;
    }

    createInfo.preTransform = swap_chain_support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    //createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(app->device, &createInfo, NULL, &app->swap_chain) != VK_SUCCESS)
    {
        printf("Could not create swap chain...\n");
        exit(7);
    }
}

void create_image_views(App *app)
{

}

void create_logical_device(App *app)
{
    QueueFamilyIndices indices = find_queue_families(app->physical_device, app->surface);

    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = indices.graphics_family,
        .queueCount = 1
    };

    float queue_priority = 1.0f;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features = {0};

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queue_create_info,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = device_extensions
    };

    if (enable_validation_layers)
    {
        create_info.enabledLayerCount = validation_layer_count;
        create_info.ppEnabledLayerNames = validation_layers;
    }
    else
    {
        create_info.enabledLayerCount = 0;
    }

    if (vkCreateDevice(app->physical_device, &create_info, NULL, &app->device) != VK_SUCCESS)
    {
        printf("Failed to create logical device!\n");
        exit(5);
    }

    vkGetDeviceQueue(app->device, indices.graphics_family, 0, &app->graphics_queue);
}

void init_vulkan(App *app)
{
    create_vulkan_instance(app);
    create_surface(app);
    pick_physical_device(app);
    is_device_suitable(app->physical_device, app->surface);
    create_logical_device(app);
    create_swap_chain(app);
    create_image_views(app);
}

void main_loop(App *app)
{
    while(!glfwWindowShouldClose(app->window))
    {
        glfwPollEvents();
    }
}

void clean_up(App *app)
{
    vkDestroySwapchainKHR(app->device, app->swap_chain, NULL);
    vkDestroyDevice(app->device, NULL);
    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    vkDestroyInstance(app->instance, NULL);
    glfwDestroyWindow(app->window);
    glfwTerminate();
}

int main()
{
    App app = {0};

    init_window(&app);
    init_vulkan(&app);
    main_loop(&app);
    clean_up(&app);
    printf("Helloaaaaaaaaa\n");
    return 0;
}