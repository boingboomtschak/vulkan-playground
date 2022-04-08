#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <set>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <optional>
#include <limits>
#include <algorithm>
#include <fstream>

const uint32_t WIN_WIDTH = 800;
const uint32_t WIN_HEIGHT = 800;
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

GLFWwindow* window;
VkInstance instance;
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
VkDevice logicalDevice;
VkQueue graphicsQueue;
VkQueue presentQueue;
VkSurfaceKHR surface;
VkSwapchainKHR swapChain;
VkFormat swapChainImageFormat;
VkExtent2D swapChainExtent;
std::vector<VkImage> swapChainImages;
std::vector<VkImageView> swapChainImageViews;
std::vector<VkFramebuffer> swapChainFramebuffers;
VkRenderPass renderPass;
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;
VkCommandPool commandPool;
VkCommandBuffer commandBuffer;
VkSemaphore imageAvailableSemaphore;
VkSemaphore renderFinishedSemaphore;
VkFence inFlightFence;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily = std::nullopt;
    std::optional<uint32_t> presentFamily = std::nullopt;
    bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

static std::vector<char> readBinaryFile(const std::string& filename) {
    std::ifstream file;
    file.open(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open file '" + filename + "'!");
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buf(fileSize);
    file.seekg(0);
    file.read(buf.data(), fileSize);
    file.close();
    return buf;
}

void kbdCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, true);
}

QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices queueFamilyIndices;
    // Retrieve queue families for device
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
        return queueFamilyIndices;
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    // Check for queue families with graphics and presentation support
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queueFamilyIndices.graphicsFamily = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
        if (present)
            queueFamilyIndices.presentFamily = i;
    }
    return queueFamilyIndices;
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    // Query device surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    // Query device surface formats
    uint32_t formatCounts;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCounts, nullptr);
    if (formatCounts != 0) {
        details.formats.resize(formatCounts);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCounts, details.formats.data());
    }
    // Query device presentation modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}


bool isDeviceSuitable(VkPhysicalDevice device) {
    // Check if device has queue family with graphics and present capabilities
    QueueFamilyIndices queueFamilyIndices = getQueueFamilies(device);
    // Check if device supports all required extensions (from deviceExtensions)
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const VkExtensionProperties& extension : availableExtensions)
        requiredExtensions.erase(extension.extensionName);
    // Check if swapchain adequate
    bool adequateSwapChain = false;
    if (requiredExtensions.empty()) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        adequateSwapChain = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    return queueFamilyIndices.isComplete() && requiredExtensions.empty() && adequateSwapChain;
}

VkShaderModule createShaderModule(const std::vector<char>& code) {
    // Create shader module creation info struct
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = (const uint32_t*)code.data();
    // Create shader module
    VkShaderModule shaderModule;
    VkResult res = vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module!");
    return shaderModule;
}

void createVkInstance() {
    // Create application info struct
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Playground";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    // Get instance extensions required by GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    printf("GLFW Required Extensions:\n");
    for (int i = 0; i < glfwExtensionCount; i++) printf("\t%s\n", glfwExtensions[i]);
    // Get available extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    printf("Available extensions\n");
    for (const VkExtensionProperties& ext : extensions) printf("\t%s\n", ext.extensionName);
    // Create instance creation info struct
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;
    // Create instance
    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create vulkan instance!");
}

void selectPhysicalDevice() {
    // Get number of physical devices
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0)
        throw std::runtime_error("Couldn't find any GPUs with Vulkan support!");
    // Enumerate all found physical devices
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    // Pick a suitable device
    for (const VkPhysicalDevice device : devices)
        if (isDeviceSuitable(device)) { physicalDevice = device; break; }
    if (physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("Failed to find a suitable GPU!");
    // Retrieve device properties, report selected device
    VkPhysicalDeviceProperties device_props;
    vkGetPhysicalDeviceProperties(physicalDevice, &device_props);
    printf("Selected GPU '%s'.\n", device_props.deviceName);
}

void createLogicalDevice() {
    QueueFamilyIndices queueFamilyIndices = getQueueFamilies(physicalDevice);
    float queuePriority = 1.0f;
    // Create graphics queue creation info struct
    VkDeviceQueueCreateInfo gfxQueueCreateInfo{};
    gfxQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    gfxQueueCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    gfxQueueCreateInfo.queueCount = 1;
    gfxQueueCreateInfo.pQueuePriorities = &queuePriority;
    // Create present queue creation info struct
    VkDeviceQueueCreateInfo prsQueueCreateInfo{};
    prsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    prsQueueCreateInfo.queueFamilyIndex = queueFamilyIndices.presentFamily.value();
    prsQueueCreateInfo.queueCount = 1;
    prsQueueCreateInfo.pQueuePriorities = &queuePriority;
    // Create device features struct
    VkPhysicalDeviceFeatures deviceFeatures{};
    // Create logical device creation info struct
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{ gfxQueueCreateInfo, prsQueueCreateInfo };
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.enabledLayerCount = 0;
    VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device!");
    // Get graphics queue
    vkGetDeviceQueue(logicalDevice, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(logicalDevice, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
}

void createSurface() {
    VkResult res = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface!");
}

void createSwapchain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
    // Select swapchain format
    VkSurfaceFormatKHR surfaceFormat = swapChainSupport.formats[0];
    for (const VkSurfaceFormatKHR availableFormat : swapChainSupport.formats)
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            surfaceFormat = availableFormat;
    // Select presentation mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const VkPresentModeKHR availablePresentMode : swapChainSupport.presentModes)
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            presentMode = availablePresentMode;
    // Select swap extent
    VkExtent2D extent = swapChainSupport.capabilities.currentExtent;
    if (extent.width == std::numeric_limits<uint32_t>::max()) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        extent.width = std::clamp((uint32_t)width, swapChainSupport.capabilities.minImageExtent.width, swapChainSupport.capabilities.maxImageExtent.width);
        extent.height = std::clamp((uint32_t)height, swapChainSupport.capabilities.minImageExtent.height, swapChainSupport.capabilities.maxImageExtent.height);
    }
    // Query reasonable image count
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        imageCount = swapChainSupport.capabilities.maxImageCount;
    // Create swapchain creation info struct
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    // Check queue indices for sharing mode
    QueueFamilyIndices indices = getQueueFamilies(physicalDevice);
    if (indices.graphicsFamily != indices.presentFamily) {
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
        
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    // Create swapchain
    VkResult res = vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain!");
    // Retrieve handles to swapchain images
    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());
    // Save swapchain image format and extent
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void createImageViews() {
    // Reserve space for image views
    swapChainImageViews.resize(swapChainImages.size());
    // Create swapchain image views
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        VkResult res = vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create swapchain image view!");
    }
}

void createRenderPass() {
    // Create render pass color attachment description
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Create render pass color attachment reference
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    // Create subpass dependencies
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // Create render pass info struct
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    VkResult res = vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass!");
}

void createGraphicsPipeline() {
    VkResult res;
    // Read shader code from spv files
    std::vector<char> vertShaderCode = readBinaryFile("shaders/spv/triangle-vert.spv");
    std::vector<char> fragShaderCode = readBinaryFile("shaders/spv/triangle-frag.spv");
    // Create shader modules from code
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
    // Create shader pipeline stage info structs
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
    // Create pipeline vertex input state info struct
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    // Create pipeline input assembly state info struct
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    // Create viewport info
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapChainExtent.width;
    viewport.height = (float) swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    // Create scissor rectangle
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;
    // Create viewport state from viewport and scissor rectangle
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    // Create rasterizer state info struct
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    // Create multisampler state info struct
    VkPipelineMultisampleStateCreateInfo multisampler{};
    multisampler.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampler.sampleShadingEnable = VK_FALSE;
    multisampler.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    // Create color blend attachment state info struct
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    // Create color blend state info struct
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    // Create pipeline uniform layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    res = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout!");
    // Create graphics pipeline info struct
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampler;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    // Create graphics pipeline
    res = vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline!");
    // Clean up shader modules after graphics pipeline created
    vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
}

void createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = { swapChainImageViews[i] };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;
        VkResult res = vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer!");
    }
}

void createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = getQueueFamilies(physicalDevice);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    VkResult res = vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool!");
}

void createSyncObjects() {
    VkResult res;
    // Create semaphores
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    res = vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create image available semaphore!");
    res = vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create render finished semaphore!");
    // Create fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    res = vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFence);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create in flight fence!");
    
}

void allocateCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkResult res = vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create command buffer!");
}

void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkResult res;
    // Begin recording to command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording to command buffer!");
    VkClearValue clearColor = {{{ 0.0f, 0.0f, 1.0f, 1.0f }}};
    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    // Bind pipeline, draw points
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    res = vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to record command buffer!");
}

void drawFrame() {
    // Wait for frame to stop being in flight
    vkWaitForFences(logicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(logicalDevice, 1, &inFlightFence);
    // Acquire next swapchain image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    // Reset and record command buffer
    vkResetCommandBuffer(commandBuffer, 0);
    recordCommandBuffer(commandBuffer, imageIndex);
    // Submit command buffer
    VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    VkResult res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to submit draw command buffer!");
    // Present result to swapChain
    VkSwapchainKHR swapChains[] = { swapChain };
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;
    vkQueuePresentKHR(presentQueue, &presentInfo);
}

int main() {
    // GLFW setup
    if (!glfwInit()) { printf("Error initializing GLFW! Exiting...\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Vulkan Playground", nullptr, nullptr);
    if (!window) { printf("Failed to create GLFW window! Exiting...\n"); glfwTerminate(); return 1; }
    glfwSetKeyCallback(window, kbdCallback);
    // Vulkan setup
    createVkInstance();
    createSurface();
    selectPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    allocateCommandBuffer();
    createSyncObjects();
    // Render loop
    while(!glfwWindowShouldClose(window)) {
        drawFrame();
        glfwPollEvents();
    }
    // Vulkan cleanup
    vkDeviceWaitIdle(logicalDevice);
    vkDestroySemaphore(logicalDevice, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(logicalDevice, renderFinishedSemaphore, nullptr);
    vkDestroyFence(logicalDevice, inFlightFence, nullptr);
    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    for (VkFramebuffer framebuffer : swapChainFramebuffers)
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
    for (VkImageView imageView : swapChainImageViews)
        vkDestroyImageView(logicalDevice, imageView, nullptr);
    vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    // GLFW cleanup
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

/*
--- TODO ---
- Enable / setup vulkan validation layers
- Set up specific debug messenger for instance creation/destruction
- Add better device suitability checks (score devices, prioritize discrete GPUs)
*/
