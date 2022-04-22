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

struct QueueFamilyIndices {
    optional<uint32_t> computeFamily = nullopt;
    optional<uint32_t> presentFamily = nullopt;
    QueueFamilyIndices(VkPhysicalDevice dev, VkSurfaceKHR surface) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0)
            return;
        vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());
        for (uint32_t i = queueFamilyCount - 1; i > 0; i--) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                computeFamily = make_optional(i);
            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if (present)
                presentFamily = make_optional(i);
        }
    }
    bool complete() { return computeFamily.has_value() && presentFamily.has_value(); }
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
    VkQueue computeQueue;
    VkQueue presentQueue;
    Swapchain swapchain;
    VkCommandPool commandPool;
    VkImage srcImage = VK_NULL_HANDLE;
    VkDeviceMemory srcImageMemory;
    VkImageView srcImageView;
    VkSampler textureSampler;
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
        vector<VkDeviceQueueCreateInfo> queueCreateInfos{ cmpQueueCreateInfo, prsQueueCreateInfo };
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
        if (indices.computeFamily != indices.presentFamily) {
            vector<uint32_t> queueFamilyIndices { indices.computeFamily.value(), indices.presentFamily.value() };
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = (uint32_t)queueFamilyIndices.size();
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        } else {
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
    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices(physicalDevice, surface);
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();
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
    void loadSrcImage(string filename) {
        int srcWidth, srcHeight, srcChannels;
        stbi_uc* pixels = stbi_load(filename.c_str(), &srcWidth, &srcHeight, &srcChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = srcWidth * srcHeight * 4;
        if (!pixels)
            throw runtime_error("Failed to load texture '" + filename + "'!");


    }
    void initialize() {
        createInstance();
        createSurface();
        selectPhysicalDevice();
        createDeviceInterface();
        createSwapchain();
        createCommandPool();
    }
    void compute() {
        
    }
    void present() {
        if (srcImage != VK_NULL_HANDLE) {
            printf("Src image loaded!\n");
        }
    }
    void cleanup() {
        vkDeviceWaitIdle(device);
        vkDestroyCommandPool(device, commandPool, nullptr);
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
    app.loadSrcImage("textures/l'ete.jpg");
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
