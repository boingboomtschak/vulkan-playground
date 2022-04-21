#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VecMat.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <chrono>
#include <vector>
#include <set>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <optional>
#include <limits>
#include <fstream>

#pragma warning(disable : 26812) // Disable enum class warning from Vulkan enums

const uint32_t WIN_WIDTH = 800;
const uint32_t WIN_HEIGHT = 800;
const int MAX_FRAMES_IN_FLIGHT = 2;
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
const std::string TEX_FILENAME = "textures/etude-de-tete.jpg";

typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::seconds::period Period;

uint32_t currentFrame = 0;
time_t startTime = time(NULL);
bool framebufferResized = false;
float size = 0.5f;

GLFWwindow* window;
VkInstance instance;
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
VkDevice logicalDevice;
VkQueue graphicsQueue;
VkQueue presentQueue;
VkSurfaceKHR surface;
VkSwapchainKHR swapchain;
VkFormat swapchainImageFormat;
VkExtent2D swapchainExtent;
std::vector<VkImage> swapchainImages;
std::vector<VkImageView> swapchainImageViews;
std::vector<VkFramebuffer> swapchainFramebuffers;
VkRenderPass renderPass;
VkPipelineLayout pipelineLayout;
VkDescriptorSetLayout descriptorSetLayout;
VkPipeline graphicsPipeline;
VkCommandPool commandPool;
VkDescriptorPool descriptorPool;
std::vector<VkDescriptorSet> descriptorSets;
VkImage textureImage;
VkDeviceMemory textureImageMemory;
VkImageView textureImageView;
VkSampler textureSampler;
VkBuffer vertexBuffer;
VkDeviceMemory vertexBufferMemory;
VkBuffer indexBuffer;
VkDeviceMemory indexBufferMemory;
std::vector<VkBuffer> uniformBuffers;
std::vector<VkDeviceMemory> uniformBuffersMemory;
std::vector<VkCommandBuffer> commandBuffers;
std::vector<VkSemaphore> imageAvailableSemaphores;
std::vector<VkSemaphore> renderFinishedSemaphores;
std::vector<VkFence> inFlightFences;

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily = std::nullopt;
	std::optional<uint32_t> presentFamily = std::nullopt;
	bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
	vec3 pos;
	vec3 normal;
	vec4 color;
    vec2 uv;
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}
	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, color);
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, uv);
		return attributeDescriptions;
	}
};

struct UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 proj;
};

const std::vector<Vertex> vertices = {
    //   POS         NORMAL       COLOR        UV
    {{-1, -1, 1},  {0, 0, 1},  {1, 0, 0, 1}, {1, 1}},
    {{1, -1, 1},   {0, 0, 1},  {1, 0, 0, 1}, {0, 1}},
    {{1, 1, 1},    {0, 0, 1},  {1, 0, 0, 1}, {0, 0}},
    {{-1, 1, 1},   {0, 0, 1},  {1, 0, 0, 1}, {1, 0}},
    {{-1, -1, -1}, {0, 0, -1}, {0, 1, 0, 1}, {1, 1}},
    {{1, -1, -1},  {0, 0, -1}, {0, 1, 0, 1}, {0, 1}},
    {{1, 1, -1},   {0, 0, -1}, {0, 1, 0, 1}, {0, 0}},
    {{-1, 1, -1},  {0, 0, -1}, {0, 1, 0, 1}, {1, 0}},
    {{-1, -1, 1},  {-1, 0, 0}, {0, 0, 1, 1}, {1, 1}},
    {{-1, -1, -1}, {-1, 0, 0}, {0, 0, 1, 1}, {0, 1}},
    {{-1, 1, -1},  {-1, 0, 0}, {0, 0, 1, 1}, {0, 0}},
    {{-1, 1, 1},   {-1, 0, 0}, {0, 0, 1, 1}, {1, 0}},
    {{1, -1, 1},   {1, 0, 0},  {1, 1, 0, 1}, {1, 1}},
    {{1, -1, -1},  {1, 0, 0},  {1, 1, 0, 1}, {0, 1}},
    {{1, 1, -1},   {1, 0, 0},  {1, 1, 0, 1}, {0, 0}},
    {{1, 1, 1},    {1, 0, 0},  {1, 1, 0, 1}, {1, 0}},
    {{-1, 1, 1},   {0, 1, 0},  {0, 1, 1, 1}, {1, 1}},
    {{1, 1, 1},    {0, 1, 0},  {0, 1, 1, 1}, {0, 1}},
    {{1, 1, -1},   {0, 1, 0},  {0, 1, 1, 1}, {0, 0}},
    {{-1, 1, -1},  {0, 1, 0},  {0, 1, 1, 1}, {1, 0}},
    {{-1, -1, 1},  {0, -1, 0}, {1, 0, 1, 1}, {1, 1}},
    {{1, -1, 1},   {0, -1, 0}, {1, 0, 1, 1}, {0, 1}},
    {{1, -1, -1},  {0, -1, 0}, {1, 0, 1, 1}, {0, 0}},
    {{-1, -1, -1}, {0, -1, 0}, {1, 0, 1, 1}, {1, 0}}
};

const std::vector<uint32_t> indices = {
	0, 1, 2, 2, 3, 0, // front
	6, 5, 4, 4, 7, 6, // back
	10, 9, 8, 8, 11, 10, // left
	12, 13, 14, 14, 15, 12, // right
	16, 17, 18, 18, 19, 16, // top
	22, 21, 20, 20, 23, 22 // bottom
};

float clamp(float val, float min, float max) { float _val = val < min ? min : val; return _val > max ? max : _val; }
uint32_t clamp(uint32_t val, uint32_t min, uint32_t max) { uint32_t _val = val < min ? min : val; return _val > max ? max : _val; }

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

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	framebufferResized = true;
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	size = clamp((float)(size + 0.01 * yoffset), 0.01f, 1.0f);
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
			queueFamilyIndices.graphicsFamily = std::make_optional(i);
		VkBool32 present = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
		if (present)
			queueFamilyIndices.presentFamily = std::make_optional(i);
	}
	return queueFamilyIndices;
}

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) {
	SwapchainSupportDetails details;
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
		SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device);
		adequateSwapChain = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
	}
    // Get device features and check if anisotropy supported
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
	return queueFamilyIndices.isComplete() && requiredExtensions.empty() && adequateSwapChain && supportedFeatures.samplerAnisotropy;
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) 
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) 
			return i;
	throw std::runtime_error("Failed to find suitable memory type!");
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
	for (uint32_t i = 0; i < glfwExtensionCount; i++) printf("\t%s\n", glfwExtensions[i]);
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
    deviceFeatures.samplerAnisotropy = VK_TRUE;
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
	SwapchainSupportDetails swapChainSupport = querySwapchainSupport(physicalDevice);
	// Select swapchain format
	VkSurfaceFormatKHR surfaceFormat = swapChainSupport.formats[0];
    for (const VkSurfaceFormatKHR availableFormat : swapChainSupport.formats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
			&& availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			surfaceFormat = availableFormat;
    }
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
		extent.width = clamp((uint32_t)width, swapChainSupport.capabilities.minImageExtent.width, swapChainSupport.capabilities.maxImageExtent.width);
		extent.height = clamp((uint32_t)height, swapChainSupport.capabilities.minImageExtent.height, swapChainSupport.capabilities.maxImageExtent.height);
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
	VkResult res = vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapchain);
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to create swapchain!");
	// Retrieve handles to swapchain images
	vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, swapchainImages.data());
	// Save swapchain image format and extent
	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;
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
    VkResult res = vkCreateImageView(logicalDevice, &viewInfo, nullptr, &imageView);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view!");
    return imageView;
}

void createImageViews() {
	// Reserve space for image views
	swapchainImageViews.resize(swapchainImages.size());
	// Create swapchain image views
	for (size_t i = 0; i < swapchainImages.size(); i++)
        swapchainImageViews[i] = createImageView(swapchainImages[i], swapchainImageFormat);
}

void createRenderPass() {
	// Create render pass color attachment description
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchainImageFormat;
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

void createDescriptorSetLayout() {
	// Set up UBO descriptor set layout binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    // Set up sample descriptor set layout binding
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // Collect all bindings
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { uboLayoutBinding, samplerLayoutBinding };
	// Create descriptor set layout
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = (uint32_t)layoutBindings.size();
	layoutInfo.pBindings = layoutBindings.data();
	VkResult res = vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout);
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor set layout!");
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
	VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions = Vertex::getAttributeDescriptions();
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	// Create pipeline input assembly state info struct
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
	// Create viewport info
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) swapchainExtent.width;
	viewport.height = (float) swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	// Create scissor rectangle
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainExtent;
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
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	// Create multisampler state info struct
	VkPipelineMultisampleStateCreateInfo multisampler{};
	multisampler.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampler.sampleShadingEnable = VK_FALSE;
	multisampler.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	// Create color blend attachment state info struct
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
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
	swapchainFramebuffers.resize(swapchainImageViews.size());
	for (size_t i = 0; i < swapchainImageViews.size(); i++) {
		VkImageView attachments[] = { swapchainImageViews[i] };
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapchainExtent.width;
		framebufferInfo.height = swapchainExtent.height;
		framebufferInfo.layers = 1;
		VkResult res = vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapchainFramebuffers[i]);
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

void beginOneTimeCommands(VkCommandBuffer& commandBuffer) {
    // Allocate new command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);
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
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	VkResult res;
	// Create buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	res = vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer);
	if (res != VK_SUCCESS) 
		throw std::runtime_error("Failed to create buffer!");
	// Get memory requirements of buffer
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);
	// Allocate buffer memory
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
	res = vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory);
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate buffer memory!");
	// Bind memory to buffer
	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
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
    region.imageOffset = {0, 0, 0};
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
    res = vkCreateImage(logicalDevice, &imageInfo, nullptr, &image);
    if (res != VK_SUCCESS)
        throw std::runtime_error("failed to create image!");
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    res = vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory);
    if (res != VK_SUCCESS)
        throw std::runtime_error("failed to allocate image memory!");
    vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

void createTextureImage() {
    // Reading image from file
    int tWidth, tHeight, tChannels;
    stbi_uc* pixels = stbi_load(TEX_FILENAME.c_str(), &tWidth, &tHeight, &tChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = tWidth * tHeight * 4;
    if (!pixels)
        throw std::runtime_error("Failed to load texture '" + TEX_FILENAME + "'!");
    // Creating staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    // Copy pixels to staging buffer
    void* data;
    vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, (size_t)imageSize);
    vkUnmapMemory(logicalDevice, stagingBufferMemory);
    stbi_image_free(pixels);
    // Create image
    createImage(tWidth, tHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
    // Transition image layout to transfer destination, copy staging buffer to image, transition to shader read only layout
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, textureImage, (uint32_t)tWidth, (uint32_t)tHeight);
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // Clean up staging buffer
    vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
    // Create image view
    textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB);
}

void createTextureSampler() {
    // Get device properties for max anisotropy
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    // Create texture image sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    VkResult res = vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &textureSampler);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler!");
}

void createVertexBuffer() {
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();
	// Create staging buffer visible to host
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	// Copy vertices to staging buffer as transfer source buffer, with host visible and coherent memory
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);
	// Create vertex buffer as transfer destination buffer, with device local memory
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
	// Copy from staging to vertex buffer 
	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
	// Destroy staging buffer and free memory
	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void createIndexBuffer() {
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();
	// Create staging buffer visible to host
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	// Copy indices to staging buffer as transfer soruce buffer, with host visible and coherent memory
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);
	// Create index buffer as transfer destination buffer, with device local memory
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
	// Copy from staging to index buffer
	copyBuffer(stagingBuffer, indexBuffer, bufferSize);
	// Destroy staging buffer and free memory
	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void createUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
}

void createDescriptorPool() {
    std::vector<VkDescriptorPoolSize> poolSizes(2);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = (uint32_t)MAX_FRAMES_IN_FLIGHT;
	VkResult res = vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool);
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor pool!");
}

void allocateDescriptorSets() {
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts.data();
	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	VkResult res = vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data());
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate descriptor sets!");
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // Create UBO descriptor info
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);
        // Create sampler descriptor info
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;
        // Create descriptor writes for descriptor set allocations
        std::vector<VkWriteDescriptorSet> descriptorWrites(2);
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(logicalDevice, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}
}

void createSyncObjects() {
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create image available semaphore!");
		if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create render finished semaphore!");
		if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create in flight fence!");
	}
}

void allocateCommandBuffers() {
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
	VkResult res = vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data());
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to create command buffers!");
}

void recreateSwapchain() {
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}
	// Wait for device to idle
	vkDeviceWaitIdle(logicalDevice);
	// Clean up previous swapchain related resources
	for (size_t i = 0; i < swapchainFramebuffers.size(); i++)
		vkDestroyFramebuffer(logicalDevice, swapchainFramebuffers[i], nullptr);
	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
	for (size_t i = 0; i < swapchainImageViews.size(); i++)
		vkDestroyImageView(logicalDevice, swapchainImageViews[i], nullptr);
	vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
	for (size_t i = 0; i < uniformBuffers.size(); i++) {
		vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
	}
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	// Recreate swapchain and dependent resources
	createSwapchain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createUniformBuffers();
	createDescriptorPool();
	allocateDescriptorSets();
	allocateCommandBuffers();
}

void copyUniformBuffer(UniformBufferObject ubo, VkDeviceMemory bufferMem) {
	void* data;
	vkMapMemory(logicalDevice, bufferMem, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(logicalDevice, bufferMem);
}

void drawFrame() {
	VkResult res;
	VkDeviceSize offset = 0;
	// Wait for frame to stop being in flight
	vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	// Acquire next swapchain image
	uint32_t imageIndex;
	res = vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebufferResized) {
		framebufferResized = false;
		recreateSwapchain();
		return;
	} else if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to acquire swapchain image!");
	// Reset in flight fences so next frame can begin rendering
	vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
	// Setup command buffer to begin
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;
	// Setup render pass
	VkClearValue clearColor = { {{ 0.0f, 0.0f, 0.0f, 1.0f }} };
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapchainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapchainExtent;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;
	// Create and copy UBO
    static Clock::time_point startTime = Clock::now();
    Clock::time_point now = Clock::now();
    float dt = std::chrono::duration<float, Period>(now - startTime).count();
    UniformBufferObject ubo{};
	ubo.model = Scale(size, size, size) * RotateY(dt * 90.0f) * RotateX(dt * 90.0f);
	ubo.view = Transpose(LookAt(vec3(2, 2, 2), vec3(0, 0, 0), vec3(0, 0, 1)));
	ubo.proj = Transpose(Perspective(45, swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.0f));
	ubo.proj[1][1] *= -1;
	copyUniformBuffer(ubo, uniformBuffersMemory[currentFrame]);
	// Bind graphics pipeline and other drawing resources
	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	res = vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo);
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to begin recording to command buffer!");
	vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, &vertexBuffer, &offset);
	vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
	vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdDrawIndexed(commandBuffers[currentFrame], (uint32_t)indices.size(), 1, 0, 0, 0);
	vkCmdEndRenderPass(commandBuffers[currentFrame]);
	res = vkEndCommandBuffer(commandBuffers[currentFrame]);
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to finalize recording command buffer!");
	// Submit command buffer
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
	if (res != VK_SUCCESS)
		throw std::runtime_error("Failed to submit draw command buffer!");
	// Present result to swapchain
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;
	vkQueuePresentKHR(presentQueue, &presentInfo);
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

int main() {
	// GLFW setup
	if (!glfwInit()) { printf("Error initializing GLFW! Exiting...\n"); return 1; }
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Vulkan Playground", nullptr, nullptr);
	if (!window) { printf("Failed to create GLFW window! Exiting...\n"); glfwTerminate(); return 1; }
	glfwSetKeyCallback(window, kbdCallback);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetScrollCallback(window, scrollCallback);
	// Vulkan setup
	createVkInstance();
	createSurface();
	selectPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
    createTextureImage();
    createTextureSampler();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	allocateDescriptorSets();
	allocateCommandBuffers();
	createSyncObjects();
	// Render loop
	while(!glfwWindowShouldClose(window)) {
		drawFrame();
		glfwPollEvents();
	}
	// Vulkan cleanup
	vkDeviceWaitIdle(logicalDevice);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
		vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
	}
	vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
	vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
	vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);
    vkDestroySampler(logicalDevice, textureSampler, nullptr);
    vkDestroyImageView(logicalDevice, textureImageView, nullptr);
    vkDestroyImage(logicalDevice, textureImage, nullptr);
    vkFreeMemory(logicalDevice, textureImageMemory, nullptr);
	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
	for (VkFramebuffer framebuffer : swapchainFramebuffers)
		vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
	for (VkImageView imageView : swapchainImageViews)
		vkDestroyImageView(logicalDevice, imageView, nullptr);
	vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
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
