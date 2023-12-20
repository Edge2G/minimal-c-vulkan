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
    VkImage *swap_chain_images;
    VkFormat swap_chain_image_format;
    VkExtent2D swap_chain_extent;
    VkImageView *swap_chain_image_views;
    uint32_t swap_chain_image_count;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkFramebuffer *swapchain_framebuffers;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
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

typedef struct ShaderFile
{
    size_t file_size;
    char *content;
} ShaderFile;

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
void create_graphics_pipeline(App *app);
VkShaderModule create_shader_module(App *app, ShaderFile *shaderfile);
void create_render_pass(App *app);
void create_framebuffers(App *app);
void createCommandPool(App *app);
void create_command_buffer(App *app); 
void recordCommandBuffer(App *app, VkCommandBuffer commandBuffer, uint32_t imageIndex); 

/* main and closing functions */
void main_loop(App *app);
void clean_up(App *app);
void read_file(const char* filename, ShaderFile *shaderfile);

// Definitions
void read_file(const char* filename, ShaderFile *shaderfile)
{
    FILE *file;

    file = fopen(filename, "rb");

    if(file ==  NULL)
    {
        printf("Failed to open file: %s\n", filename);
        exit(9);
    }

    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    shaderfile->file_size = size;
    fseek(file, 0L, SEEK_SET);
    printf("%s size = %ld\n", filename, size);
    //char *buff[4096];
    shaderfile->content = (char*)malloc(sizeof(char) * shaderfile->file_size);
    fread(shaderfile->content, size, sizeof(char), file);
    fclose(file);

    //return *buff;
}


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

    vkGetSwapchainImagesKHR(app->device, app->swap_chain, &image_count, NULL);
    app->swap_chain_images = (VkImage*)malloc(sizeof(VkImage) * image_count);

    vkGetSwapchainImagesKHR(app->device, app->swap_chain, &image_count, app->swap_chain_images);
    app->swap_chain_image_count = image_count;

    app->swap_chain_image_format = surface_format.format;
    app->swap_chain_extent = extent;
}

void create_image_views(App *app)
{
    app->swap_chain_image_views = (VkImageView*)malloc(sizeof(VkImageView) * app->swap_chain_image_count);

    printf("Image count: %d\n", app->swap_chain_image_count);
    for (uint32_t i = 0; i < app->swap_chain_image_count; i++)
    {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = app->swap_chain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = app->swap_chain_image_format,
            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };
        printf("Image address: %p\n", app->swap_chain_images[i]);

        if(vkCreateImageView(app->device, &create_info, NULL, &app->swap_chain_image_views[i]) != VK_SUCCESS)
        {
            printf("Failed to create image view...\n");
            exit(8);
        }
    }
}

void create_graphics_pipeline(App *app)
{
    // Vertex and fragment creation

    ShaderFile vert_file = {0};
    ShaderFile frag_file = {0};
    read_file("shaders/vert.spv", &vert_file);
    read_file("shaders/frag.spv", &frag_file);

    VkShaderModule vert_module = create_shader_module(app, &vert_file);
    VkShaderModule frag_module = create_shader_module(app, &frag_file);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
    };

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main",
    };

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

    // Dynamic states creation

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    uint32_t dynamic_states_count = 2;

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_states_count,
        .pDynamicStates = dynamic_states,
    };

    // Vertex input creation

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL, // Optional
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL, // Optional
    };

    // Input assembly

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Viewports and scissors

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)app->swap_chain_extent.width,
        .height = (float)app->swap_chain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = app->swap_chain_extent,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // Rasterizer

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f, // Optional
        .depthBiasClamp = 0.0f, // Optional
        .depthBiasSlopeFactor = 0.0f, // Optional
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f, // Optional
        .pSampleMask = NULL, // Optional
        .alphaToCoverageEnable = VK_FALSE, // Optional
        .alphaToOneEnable = VK_FALSE, // Optional
    };

    // Color blending

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
        .colorBlendOp = VK_BLEND_OP_ADD, // Optional
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
        .alphaBlendOp = VK_BLEND_OP_ADD, // Optional
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY, // Optional
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants[0] = 0.0f, // Optional
        .blendConstants[1] = 0.0f, // Optional
        .blendConstants[2] = 0.0f, // Optional
        .blendConstants[3] = 0.0f, // Optional
    };

    // Pipeline Layout

    //VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0, // Optional
        .pSetLayouts = NULL, // Optional
        .pushConstantRangeCount = 0, // Optional
        .pPushConstantRanges = NULL, // Optional
    };

    if (vkCreatePipelineLayout(app->device, &pipelineLayoutInfo, NULL, &app->pipeline_layout) != VK_SUCCESS)
    {
       printf("failed to create pipeline layout!");
       exit(11);
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = NULL, // Optional
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamic_state,
        .layout = app->pipeline_layout,
        .renderPass = app->render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE, // Optional
        .basePipelineIndex = -1, // Optional
    };

    if (vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &app->graphics_pipeline) != VK_SUCCESS)
    {
        printf("failed to create graphics pipeline!");
        exit(13);
    }

    vkDestroyShaderModule(app->device, vert_module, NULL);
    vkDestroyShaderModule(app->device, frag_module, NULL);
}

VkShaderModule create_shader_module(App *app, ShaderFile *shaderfile)
{
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderfile->file_size,
        .pCode = (uint32_t*)shaderfile->content,
    };

    printf("code size: %lu\n", create_info.codeSize);

    VkShaderModule shader_module;

    if(vkCreateShaderModule(app->device, &create_info, NULL, &shader_module) != VK_SUCCESS)
    {
        printf("Failed to create shader module.\n");
        exit(10);
    }

    return shader_module;

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

void create_render_pass(App *app)
{
    VkAttachmentDescription colorAttachment = {
        .format = app->swap_chain_image_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    };

    VkAttachmentReference colorAttachmentRef ={
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    if (vkCreateRenderPass(app->device, &renderPassInfo, NULL, &app->render_pass) != VK_SUCCESS)
    {
        printf("failed to create render pass!");
        exit(12);
    }

}

void create_framebuffers(App *app)
{
    app->swapchain_framebuffers = (VkFramebuffer*)malloc(app->swap_chain_image_count * sizeof(VkFramebuffer));

    for(uint32_t i = 0; i < app->swap_chain_image_count; i++)
    {
        VkImageView attachments[] = {
            app->swap_chain_image_views[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = app->render_pass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = app->swap_chain_extent.width;
        framebufferInfo.height = app->swap_chain_extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(app->device, &framebufferInfo, NULL, &app->swapchain_framebuffers[i]) != VK_SUCCESS) {
            printf("failed to create framebuffer!");
            exit(14);
        }
    }
}

void createCommandPool(App *app)
{
    QueueFamilyIndices queueFamilyIndices = find_queue_families(app->physical_device, app->surface);

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndices.graphics_family,
    };

    if (vkCreateCommandPool(app->device, &poolInfo, NULL, &app->commandPool) != VK_SUCCESS)
    {
        printf("failed to create command pool!");
        exit(15);
    }
}

void create_command_buffer(App *app)
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (vkAllocateCommandBuffers(app->device, &allocInfo, &app->commandBuffer) != VK_SUCCESS) {
        printf("failed to allocate command buffers!");
        exit(16);
    }
}

void recordCommandBuffer(App *app, VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0, // Optional
        .pInheritanceInfo = NULL, // Optional
    };

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        printf("failed to begin recording command buffer!");
        exit(17);
    }

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = app->render_pass;
    renderPassInfo.framebuffer = app->swapchain_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset.x = 0;
    renderPassInfo.renderArea.offset.y = 0;
    renderPassInfo.renderArea.extent = app->swap_chain_extent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->graphics_pipeline);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)(app->swap_chain_extent.width);
    viewport.height = (float)(app->swap_chain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = app->swap_chain_extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        printf("failed to record command buffer!");
        exit(18);
    }
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
    create_render_pass(app);
    create_graphics_pipeline(app);
    create_framebuffers(app);
    createCommandPool(app);
    create_command_buffer(app);
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
    vkDestroyCommandPool(app->device, app->commandPool, NULL);

    for(uint32_t i = 0; i < app->swap_chain_image_count; i++)
    {
        vkDestroyFramebuffer(app->device, app->swapchain_framebuffers[i], NULL);
    }

    vkDestroyPipeline(app->device, app->graphics_pipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);
    vkDestroyRenderPass(app->device, app->render_pass, NULL);
    
    for (uint32_t i = 0; i < app->swap_chain_image_count; i++)
    {
        vkDestroyImageView(app->device, app->swap_chain_image_views[i], NULL);
    }

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