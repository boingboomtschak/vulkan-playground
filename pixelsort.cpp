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
#include <fstream>

#pragma warning(disable : 26812)

using std::vector;
using std::runtime_error;
using std::nullopt;
using std::make_optional;

typedef std::optional<uint32_t> opt_uint32_t;

const uint32_t WIN_WIDTH = 800;
const uint32_t WIN_HEIGHT = 800;
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

GLFWwindow* window;
bool framebufferResized = false;

struct QueueFamilyIndices {
    opt_uint32_t computeFamily = nullopt;
    opt_uint32_t presentFamily = nullopt;
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
        
        return true;
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
    void createLogicalDevice() {
        
    }
    void initialize() {
        createInstance();
        createSurface();
        selectPhysicalDevice();
        createLogicalDevice();
    }
    void compute() {
        
    }
    void present() {
        
    }
    void cleanup() {
        //vkDeviceWaitIdle(device);
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
