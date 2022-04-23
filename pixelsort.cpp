#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <VecMat.h>

#include <vector>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <optional>
#include <limits>
#include <fstream>

#pragma warning(disable : 26812)

using std::vector;
using std::runtime_error;
using std::optional;
using std::nullopt;
using std::make_optional;
using std::string;

const uint32_t WIN_WIDTH = 800;
const uint32_t WIN_HEIGHT = 800;
const vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

GLFWwindow* window;
bool framebufferResized = false;

float clamp(float val, float min, float max) { float _val = val < min ? min : val; return _val > max ? max : _val; }
uint32_t clamp(uint32_t val, uint32_t min, uint32_t max) { uint32_t _val = val < min ? min : val; return _val > max ? max : _val; }

static std::vector<char> readBinaryFile(const std::string& filename) {
    std::ifstream file;
    file.open(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open file '" + filename + "'!");
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buf(fileSize);
    file.seekg(0);
    file.read(buf.data(), fileSize);
    file.close();
    return buf;
}

struct Vertex {
    vec3 pos;
    vec2 uv;
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    static vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, uv);
        return attributeDescriptions;
    }
};

const vector<Vertex> vertices = {
    {{-1, -1, 0}, {1, 1}},
    {{1, -1, 0},  {0, 1}},
    {{1, 1, 0},   {0, 0}},
    {{1, 1, 0},   {0, 0}},
    {{-1, 1, 0},  {1, 0}},
    { {-1, -1, 0}, {1, 1}}
};

struct QueueFamilyIndices {
    optional<uint32_t> graphicsFamily = nullopt;
    optional<uint32_t> computeFamily = nullopt;
    optional<uint32_t> presentFamily = nullopt;
    QueueFamilyIndices(VkPhysicalDevice dev, VkSurfaceKHR surface) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0)
            return;
        vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphicsFamily = make_optional(i);
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                computeFamily = make_optional(i);
            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if (present)
                presentFamily = make_optional(i);
        }
    }
    bool complete() { return graphicsFamily.has_value() && computeFamily.has_value() && presentFamily.has_value(); }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    vector<VkSurfaceFormatKHR> formats;
    vector<VkPresentModeKHR> presentModes;
    SwapchainSupportDetails(VkPhysicalDevice dev, VkSurfaceKHR surface) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &capabilities);
        uint32_t formatCounts;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCounts, nullptr);
        if (formatCounts != 0) {
            formats.resize(formatCounts);
            vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCounts, formats.data());
        }
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentModeCount, presentModes.data());
        }
    }
};

struct Swapchain {
    VkSwapchainKHR chain;
    VkFormat format;
    VkExtent2D extent;
    vector<VkImage> images;
    vector<VkImageView> imageViews;
    vector<VkFramebuffer> framebuffers;
};

struct Application {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue presentQueue;
    Swapchain swapchain;
    VkRenderPass renderPass;
    VkDescriptorSetLayout graphicsDescriptorSetLayout;
    VkPipeline graphicsPipeline;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkPipeline computePipeline;
    VkCommandPool commandPool;
    VkImage srcImage = VK_NULL_HANDLE;
    VkDeviceMemory srcImageMemory;
    VkImageView srcImageView;
    VkImage dstImage;
    VkDeviceMemory dstImageMemory;
    VkImageView dstImageView;
    VkSampler dstSampler;
    void createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Pixel Sorter";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        VkInstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0;
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            throw runtime_error("Failed to create instance!");
    }
    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
            throw runtime_error("Failed to create surface!");
    }
    bool deviceSuitable(VkPhysicalDevice _device) {
        QueueFamilyIndices queueFamilyIndices(_device, surface);
        uint32_t availableExtensionCount;
        vkEnumerateDeviceExtensionProperties(_device, nullptr, &availableExtensionCount, nullptr);
        vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
        vkEnumerateDeviceExtensionProperties(_device, nullptr, &availableExtensionCount, availableExtensions.data());
        bool extensionsMatched = true;
        for (const char* devExtName : deviceExtensions) {
            bool matched = false;
            for (const VkExtensionProperties& avlExt : availableExtensions) {
                if (strcmp(devExtName, avlExt.extensionName) == 0)
                    matched = true;
            }
            if (!matched)
                extensionsMatched = false;
        }
        bool adequateSwapchain = false;
        SwapchainSupportDetails swapchainSupport(_device, surface);
        adequateSwapchain = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
        return queueFamilyIndices.complete() && extensionsMatched && adequateSwapchain;
    }
    void selectPhysicalDevice() {
        uint32_t dev_ct = 0;
        vkEnumeratePhysicalDevices(instance, &dev_ct, nullptr);
        if (dev_ct == 0)
            throw runtime_error("No devices with Vulkan support!");
        vector<VkPhysicalDevice> devs(dev_ct);
        vkEnumeratePhysicalDevices(instance, &dev_ct, devs.data());
        for (const VkPhysicalDevice _device : devs)
            if (deviceSuitable(_device))
                { physicalDevice = _device; break; }
        if (physicalDevice == VK_NULL_HANDLE)
            throw runtime_error("Failed to find suitable device!");
        VkPhysicalDeviceProperties dev_props;
        vkGetPhysicalDeviceProperties(physicalDevice, &dev_props);
        printf("Selected device '%s'.\n", dev_props.deviceName);
    }
    void createDeviceInterface() {
        QueueFamilyIndices queueFamilyIndices(physicalDevice, surface);
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo gfxQueueCreateInfo{};
        gfxQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        gfxQueueCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        gfxQueueCreateInfo.queueCount = 1;
        gfxQueueCreateInfo.pQueuePriorities = &queuePriority;
        VkDeviceQueueCreateInfo cmpQueueCreateInfo{};
        cmpQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        cmpQueueCreateInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();
        cmpQueueCreateInfo.queueCount = 1;
        cmpQueueCreateInfo.pQueuePriorities = &queuePriority;
        VkDeviceQueueCreateInfo prsQueueCreateInfo{};
        prsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        prsQueueCreateInfo.queueFamilyIndex = queueFamilyIndices.presentFamily.value();
        prsQueueCreateInfo.queueCount = 1;
        prsQueueCreateInfo.pQueuePriorities = &queuePriority;
        VkPhysicalDeviceFeatures deviceFeatures{};
        vector<VkDeviceQueueCreateInfo> queueCreateInfos{ gfxQueueCreateInfo, cmpQueueCreateInfo, prsQueueCreateInfo };
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.enabledLayerCount = 0;
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
            throw runtime_error("Failed to create device interface!");
        vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, queueFamilyIndices.computeFamily.value(), 0, &computeQueue);
        vkGetDeviceQueue(device, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
    }
    VkImageView createImageView(VkImage image, VkFormat format) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
            throw runtime_error("Failed to create image view!");
        return imageView;
    }
    void createSwapchain() {
        SwapchainSupportDetails swapchainSupport(physicalDevice, surface);
        VkSurfaceFormatKHR surfaceFormat = swapchainSupport.formats[0];
        for (const VkSurfaceFormatKHR availableFormat : swapchainSupport.formats)
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
                && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                surfaceFormat = availableFormat;
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const VkPresentModeKHR availablePresentMode : swapchainSupport.presentModes)
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
                presentMode = availablePresentMode;
        VkExtent2D extent = swapchainSupport.capabilities.currentExtent;
        if (extent.width == std::numeric_limits<uint32_t>::max()) {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            extent.width = clamp((uint32_t)width, swapchainSupport.capabilities.minImageExtent.width, swapchainSupport.capabilities.maxImageExtent.width);
            extent.width = clamp((uint32_t)height, swapchainSupport.capabilities.minImageExtent.height, swapchainSupport.capabilities.maxImageExtent.height);
        }
        uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
        if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount < swapchainSupport.capabilities.maxImageCount)
            imageCount = swapchainSupport.capabilities.maxImageCount;
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        QueueFamilyIndices indices(physicalDevice, surface);
        if (indices.graphicsFamily != indices.computeFamily || 
            indices.computeFamily != indices.presentFamily || 
            indices.graphicsFamily != indices.presentFamily) {
            printf("Setting swapchain to concurrent mode:\n");
            printf("\tgfx: %d\n\tcmp: %d\n\tprs: %d\n", indices.graphicsFamily.value(), indices.computeFamily.value(), indices.presentFamily.value());
            vector<uint32_t> queueFamilyIndices { indices.graphicsFamily.value(), indices.computeFamily.value(), indices.presentFamily.value() };
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = (uint32_t)queueFamilyIndices.size();
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        } else {
            printf("Setting swapchain to exclusive mode.\n");
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain.chain) != VK_SUCCESS)
            throw runtime_error("Failed to create swapchain!");
        vkGetSwapchainImagesKHR(device, swapchain.chain, &imageCount, nullptr);
        swapchain.images.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain.chain, &imageCount, swapchain.images.data());
        swapchain.format = surfaceFormat.format;
        swapchain.extent = extent;
        swapchain.imageViews.resize(imageCount);
        for (size_t i = 0; i < imageCount; i++)
            swapchain.imageViews[i] = createImageView(swapchain.images[i], swapchain.format);
    }
    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchain.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
            throw runtime_error("Failed to create render pass!");
    }
    VkShaderModule createShaderModule(const vector<char>& code) {
        // Create shader module creation info struct
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = (const uint32_t*)code.data();
        // Create shader module
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            throw runtime_error("Failed to create shader module!");
        return shaderModule;
    }
    void createGraphicsPipeline() {
        vector<char> vertShaderCode = readBinaryFile("shaders/spv/pixelsort-vert.spv");
        vector<char> fragShaderCode = readBinaryFile("shaders/spv/pixelsort-frag.spv");
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
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
        vector<VkPipelineShaderStageCreateInfo> shaderStages{ vertShaderStageInfo, fragShaderStageInfo };
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
        vector<VkVertexInputAttributeDescription> attributeDescriptions = Vertex::getAttributeDescriptions();
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain.extent.width;
        viewport.height = (float)swapchain.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapchain.extent;
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        VkPipelineMultisampleStateCreateInfo multisampler{};
        multisampler.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampler.sampleShadingEnable = VK_FALSE;
        multisampler.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        vector<VkDescriptorSetLayoutBinding> bindings(1);
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = bindings.size();
        descriptorLayoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &graphicsDescriptorSetLayout) != VK_SUCCESS)
            throw runtime_error("Failed to create graphics descriptor set layout!");
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // descriptor set layout count
        pipelineLayoutInfo.pSetLayouts = nullptr; // descriptor set layout reference
        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw runtime_error("Failed to create graphics pipeline layout!");
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages.data();
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
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
            throw runtime_error("Failed to create graphics pipeline!");
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
    }
    void createComputePipeline() {
        vector<char> compShaderCode = readBinaryFile("shaders/spv/pixelsort-comp.spv");
        VkShaderModule compShaderModule = createShaderModule(compShaderCode);
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // descriptor set layout count
        pipelineLayoutInfo.pSetLayouts = nullptr; // descriptor set layout reference
        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw runtime_error("Failed to create compute pipeline layout!");
        VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = compShaderModule;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = pipelineLayout;
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS)
            throw runtime_error("Failed to create compute pipeline!");
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, compShaderModule, nullptr);
    }
    void createFramebuffers() {
        swapchain.framebuffers.resize(swapchain.imageViews.size());
        for (size_t i = 0; i < swapchain.imageViews.size(); i++) {
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &swapchain.imageViews[i];
            framebufferInfo.width = swapchain.extent.width;
            framebufferInfo.height = swapchain.extent.height;
            framebufferInfo.layers = 1;
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchain.framebuffers[i]) != VK_SUCCESS)
                throw runtime_error("Failed to create framebuffer!");
        }
    }
    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices(physicalDevice, surface);
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
            throw runtime_error("Failed to create command pool!");
    }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        throw std::runtime_error("Failed to find suitable memory type!");
    }
    void beginOneTimeCommands(VkCommandBuffer& commandBuffer) {
        // Allocate new command buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
        // Begin recording to command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
    }
    void endOneTimeCommands(VkCommandBuffer& commandBuffer) {
        // End recording to command buffer
        vkEndCommandBuffer(commandBuffer);
        // Submit command buffer, wait until device idle
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        // Free command buffer
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkResult res;
        // Create buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        res = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create buffer!");
        // Get memory requirements of buffer
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
        // Allocate buffer memory
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        res = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate buffer memory!");
        // Bind memory to buffer
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        // Allocate and start recording to one time use command buffer
        VkCommandBuffer commandBuffer;
        beginOneTimeCommands(commandBuffer);
        // Copy source buffer to destination buffer
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        // End recording and free command buffer
        endOneTimeCommands(commandBuffer);
    }
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        // Allocate and start recording to one time use command buffer
        VkCommandBuffer commandBuffer;
        beginOneTimeCommands(commandBuffer);
        // Set up buffer to image copy
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };
        // Copy buffer to image
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        // End recording and free command buffer
        endOneTimeCommands(commandBuffer);
    }
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        // Allocate and start recording to one time use command buffer
        VkCommandBuffer commandBuffer;
        beginOneTimeCommands(commandBuffer);
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else throw std::invalid_argument("Unsupported image layout transition!");
        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        // End recording and free command buffer
        endOneTimeCommands(commandBuffer);
    }
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        VkResult res;
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        res = vkCreateImage(device, &imageInfo, nullptr, &image);
        if (res != VK_SUCCESS)
            throw std::runtime_error("failed to create image!");
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        res = vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory);
        if (res != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory!");
        vkBindImageMemory(device, image, imageMemory, 0);
    }
    void loadImage(string filename) {
        int srcWidth, srcHeight, srcChannels;
        stbi_uc* pixels = stbi_load(filename.c_str(), &srcWidth, &srcHeight, &srcChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = srcWidth * srcHeight * 4;
        if (!pixels)
            throw runtime_error("Failed to load texture '" + filename + "'!");
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, (size_t)imageSize);
        vkUnmapMemory(device, stagingBufferMemory);
        stbi_image_free(pixels);
        createImage(srcWidth, srcHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, srcImage, srcImageMemory);
        transitionImageLayout(srcImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, srcImage, (uint32_t)srcWidth, (uint32_t)srcHeight);
        transitionImageLayout(srcImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
        srcImageView = createImageView(srcImage, VK_FORMAT_R8G8B8A8_SRGB);
        createImage(srcWidth, srcHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dstImage, dstImageMemory);
        dstImageView = createImageView(dstImage, VK_FORMAT_R8G8B8A8_SRGB);
    }
    
    void initialize() {
        createInstance();
        createSurface();
        selectPhysicalDevice();
        createDeviceInterface();
        createSwapchain();
        createRenderPass();
        createGraphicsPipeline();
        createComputePipeline();
        createFramebuffers();
        createCommandPool();
    }
    void compute() {
        
    }
    void present() {
        if (srcImage != VK_NULL_HANDLE) {
            uint32_t imageIndex;
            VkResult res = vkAcquireNextImageKHR(device, swapchain.chain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &imageIndex);
            
            
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 0;
            presentInfo.pWaitSemaphores = VK_NULL_HANDLE;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain.chain;
            presentInfo.pImageIndices = &imageIndex;
            presentInfo.pResults = nullptr;
            vkQueuePresentKHR(presentQueue, &presentInfo);
        }
    }
    void cleanup() {
        vkDeviceWaitIdle(device);
        if (srcImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, srcImage, nullptr);
            vkFreeMemory(device, srcImageMemory, nullptr);
            vkDestroyImageView(device, srcImageView, nullptr);
            vkDestroyImage(device, dstImage, nullptr);
            vkFreeMemory(device, dstImageMemory, nullptr);
            vkDestroyImageView(device, dstImageView, nullptr);
        }
        vkDestroyCommandPool(device, commandPool, nullptr);
        for (VkFramebuffer framebuffer : swapchain.framebuffers)
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
        //vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyDescriptorSetLayout(device, graphicsDescriptorSetLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        for (VkImageView imageView : swapchain.imageViews)
            vkDestroyImageView(device, imageView, nullptr);
        vkDestroySwapchainKHR(device, swapchain.chain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
};

Application app;

void kbdCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(w, true);
}

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    framebufferResized = true;
}

int main() {
    if (!glfwInit())
        throw runtime_error("Failed to initialize GLFW!");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Pixel Sorter", nullptr, nullptr);
    if (!window)
        throw runtime_error("Failed to create GLFW window!");
    glfwSetKeyCallback(window, kbdCallback);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    app.initialize();
    app.loadImage("textures/l'ete.jpg");
    while (!glfwWindowShouldClose(window)) {
        app.compute();
        app.present();
        glfwPollEvents();
    }
    app.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
