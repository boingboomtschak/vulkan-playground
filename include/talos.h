// talos.h : Devon McKee : v0.0.1
// A library of useful functions for working with Vulkan, particularly in removing
// some of the overhead in bootstrapping a simple graphics program.
// 
// Uses some C++17 features and std libs, so must be compiled using at least that 
// version.
// 
// Includes some basic debugging messages, enabled by defining the preprocessor
// directive TALOS_ENABLE_DEBUG
//
// --- TODO ---
// - Enable / setup vulkan validation layers
// - Set up specific debug messenger for instance creation/destruction
// - Add better device suitability checks (score devices, prioritize discrete GPUs)

#ifndef TALOS_HDR
#define TALOS_HDR

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <fstream>
#include <stdexcept>
#include <set>
#include <algorithm>

namespace Talos {

    // ---- STRUCTS ----

    // Struct for holding details about a particular device's supported
    // swapchain capabilities, formats, and presentation modes.
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct SwapchainDetails {
        VkSwapchainKHR swapchain{};
        VkFormat imageFormat{};
        VkExtent2D extent{};
        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;
    };

    // Struct for keeping track of queue family indices, specifically those with
    // graphics and presentation support. On many devices, these two may be the 
    // same, but worth keeping track of both independently for the cases they are
    // not.
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily = std::nullopt;
        std::optional<uint32_t> presentFamily = std::nullopt;
        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    // ---- FUNCTIONS ----

    // Reads binary files from filename, returns vector of chars with file data.
    // Used for reading SPIR-V binary files (.spv) for use in shader modules.
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

    // Retrieves queue families from device, takes the physical device to query
    // and the surface being presented to as a parameter.
    QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
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
            // Checks if queue family supports graphics
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                queueFamilyIndices.graphicsFamily = i;
            // Checks if queue family supports presentation
            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
            if (present)
                queueFamilyIndices.presentFamily = i;
        }
        return queueFamilyIndices;
    }

    // Queries a device's swapchain support, takes the physical device to query 
    // and the surface being presented to as a parameter.
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        SwapchainSupportDetails details;
        // Query device surface capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);
        // Query device surface formats
        uint32_t formatCounts;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCounts, nullptr);
        if (formatCounts != 0) {
            details.formats.resize(formatCounts);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCounts, details.formats.data());
        }
        // Query device presentation modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
        }
        return details;
    }

    // Checks if physical device is suitable for usage with Vulkan. Takes the physical
    // device, the surface being presented to, and a vector of the required extensions.
    // No checks for whether a device might be more preferable (integrated vs. discrete),
    // max buffer sizes, etc.
    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<const char*> deviceExtensions) {
        // Check if device has queue family with graphics and present capabilities
        QueueFamilyIndices queueFamilyIndices = getQueueFamilies(device, surface);
        // Check if device supports all required extensions (from deviceExtensions)
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const VkExtensionProperties& extension : availableExtensions)
            requiredExtensions.erase(extension.extensionName);
        // Check if swapchain adequate
        bool adequateSwapchain = false;
        if (requiredExtensions.empty()) {
            SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device, surface);
            adequateSwapchain = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
        }
        return queueFamilyIndices.isComplete() && requiredExtensions.empty() && adequateSwapchain;
    }

    // Creates and returns the Vulkan instance. Sets the application name and version 
    // using parameters passed to the function.
    VkInstance createVkInstance(const char* applicationName, int version[3]) {
        // Create application info struct
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = applicationName;
        appInfo.applicationVersion = VK_MAKE_VERSION(version[0], version[1], version[2]);
        appInfo.pEngineName = "No engine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        std::vector<const char*> extensions;
        // Get instance extensions required by GLFW
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        for (size_t i = 0; i < glfwExtensionCount; i++)
            extensions.push_back(glfwExtensions[i]);
        #ifdef TALOS_ENABLE_DEBUG
        printf("GLFW Required Extensions:\n");
        for (int i = 0; i < glfwExtensionCount; i++) printf("\t%s\n", glfwExtensions[i]);
        // Get available extensions
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
        printf("Available extensions\n");
        for (const VkExtensionProperties& ext : extensions) printf("\t%s\n", ext.extensionName);
        #endif
        // Create instance creation info struct
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = 0;
        // Create instance
        VkInstance instance;
        VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create vulkan instance!");
        return instance;
    }

    // Select physical device 
    VkPhysicalDevice selectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, std::vector<const char*> extensions) {
        // Get number of physical devices
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        if (device_count == 0)
            throw std::runtime_error("Couldn't find any GPUs with Vulkan support!");
        // Enumerate all found physical devices
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
        // Pick a suitable device
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        for (const VkPhysicalDevice device : devices)
            if (isDeviceSuitable(device, surface, extensions)) { physicalDevice = device; break; }
        if (physicalDevice == VK_NULL_HANDLE)
            throw std::runtime_error("Failed to find a suitable GPU!");
        // Retrieve device properties, report selected device
        VkPhysicalDeviceProperties device_props;
        vkGetPhysicalDeviceProperties(physicalDevice, &device_props);
        printf("Selected GPU '%s'.\n", device_props.deviceName);
        #ifdef TALOS_ENABLE_DEBUG
        string t;
        switch (device_props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_CPU: t = "CPU";
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: t = "Discrete GPU";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: t = "Integrated GPU";
            case VK_PHYSICAL_DEVICE_TYPE_OTHER: t = "Other";
            case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM: t = "Error";
        }
        printf("\tType: %s\n", t);
        printf("\tAPI version: %d\n", device_props.apiVersion);
        pritnf("\tDriver version: %d\n", device_props.driverVersion);
        #endif
        return physicalDevice;
    }

    // Creates logical device along with graphics and presentation queue, which are
    // written to the addresses passed in the function parameters.
    VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<const char*> deviceExtensions, VkQueue* graphicsQueue, VkQueue* presentQueue) {
        QueueFamilyIndices queueFamilyIndices = getQueueFamilies(physicalDevice, surface);
        float queuePriority = 1.0f;
        // Create graphics queue creation info struct
        VkDeviceQueueCreateInfo gfxQueueCreateInfo{};
        gfxQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        gfxQueueCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        gfxQueueCreateInfo.queueCount = 1;
        gfxQueueCreateInfo.pQueuePriorities = &queuePriority;
        // Create presentation queue creation info struct
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
        VkDevice logicalDevice;
        VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create logical device!");
        // Get graphics queue, write to addresses passed into function parameters
        vkGetDeviceQueue(logicalDevice, queueFamilyIndices.graphicsFamily.value(), 0, graphicsQueue);
        vkGetDeviceQueue(logicalDevice, queueFamilyIndices.presentFamily.value(), 0, presentQueue);
        return logicalDevice;
    }

    VkImageView createImageView(VkDevice logicalDevice, VkImage image, VkFormat imageFormat) {
        // Create swapchain image views
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        VkImageView imageView;
        VkResult res = vkCreateImageView(logicalDevice, &createInfo, nullptr, &imageView);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create swapchain image view!");
        return imageView;
    }

    SwapchainDetails createSwapchain(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface, GLFWwindow* window) {
        SwapchainSupportDetails swapchainSupport = querySwapchainSupport(physicalDevice, surface);
        // Select swapchain format
        VkSurfaceFormatKHR surfaceFormat = swapchainSupport.formats[0];
        for (const VkSurfaceFormatKHR availableFormat : swapchainSupport.formats)
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
                && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                surfaceFormat = availableFormat;
        // Select presentation mode
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const VkPresentModeKHR availablePresentMode : swapchainSupport.presentModes)
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
                presentMode = availablePresentMode;
        // Select swap extent
        VkExtent2D extent = swapchainSupport.capabilities.currentExtent;
        if (extent.width == std::numeric_limits<uint32_t>::max()) {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            extent.width = std::clamp((uint32_t)width, swapchainSupport.capabilities.minImageExtent.width, swapchainSupport.capabilities.maxImageExtent.width);
            extent.height = std::clamp((uint32_t)height, swapchainSupport.capabilities.minImageExtent.height, swapchainSupport.capabilities.maxImageExtent.height);
        }
        // Query reasonable image count
        uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
        if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount)
            imageCount = swapchainSupport.capabilities.maxImageCount;
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
        createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        // Check queue indices for sharing mode
        QueueFamilyIndices indices = getQueueFamilies(physicalDevice, surface);
        if (indices.graphicsFamily != indices.presentFamily) {
            uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;

        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        // Create swapchain
        SwapchainDetails swapchainDetails;
        VkResult res = vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapchainDetails.swapchain);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create swapchain!");
        // Retrieve handles to swapchain images
        vkGetSwapchainImagesKHR(logicalDevice, swapchainDetails.swapchain, &imageCount, nullptr);
        swapchainDetails.images.resize(imageCount);
        vkGetSwapchainImagesKHR(logicalDevice, swapchainDetails.swapchain, &imageCount, swapchainDetails.images.data());
        // Save swapchain image format and extent
        swapchainDetails.imageFormat = surfaceFormat.format;
        swapchainDetails.extent = extent;
        // Create swapchain image views
        swapchainDetails.imageViews.resize(swapchainDetails.images.size());
        for (size_t i = 0; i < swapchainDetails.images.size(); i++)
            swapchainDetails.imageViews[i] = createImageView(logicalDevice, swapchainDetails.images[i], swapchainDetails.imageFormat);      
        return swapchainDetails;
    }

    VkRenderPass createRenderPass(VkDevice logicalDevice, VkFormat imageFormat) {
        // Create render pass color attachment description
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = imageFormat;
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
        VkRenderPass renderPass;
        VkResult res = vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create render pass!");
        return renderPass;
    }
}

#endif