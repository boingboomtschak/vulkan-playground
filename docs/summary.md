# Vulkan





### Objects

Short descriptions of some of the relevant objects to a Vulkan program. Objects are sorted in the order of their likely usage (ex. logical devices are created after physical devices, which are queried from an instance).

##### Instance (VkInstance)

An instance of the Vulkan API context. Created using some information about the application (such as name, version, API version, enabled extensions) and a call to `vkCreateInstance`, allows calls to be made to the Vulkan API. Multiple API instances can be created, but this usually not a particularly useful action. GLFW requires certain instance extensions queried by `glfwGetRequiredInstanceExtensions`, which should be attached to the `ppEnabledExtensionNames` field of the struct used to create the instance. Must be destroyed at the end of the program execution with a call to `vkDestroyInstance`. 

##### Physical Device (VkPhysicalDevice)

A class representing a compatible physical device available on the system. Queried from the system with a call to `vkEnumeratePhysicalDevices`, available devices should be queried for proper support before selecting one to use. Multiple devices can be used (for example, one for graphics and one for compute), but this is not a common use-case. To render to the screen, a device should at minimum support any required extensions (GLFW requires some which can be queried with, and utilizing a swapchain requires one)

##### Logical Device (VkDevice)



##### Queue (VkQueue)



##### Surface (VkSurfaceKHR)



##### Swapchain (VkSwapchainKHR)



##### Format (VkFormat)



##### Extent (VkExtent2D/VkExtent3D)



##### Image (VkImage)



##### Image View (VkImageView)



##### Framebuffer (VkFramebuffer)



##### Render Pass (VkRenderPass)



##### Pipeline Layout (VkPipelineLayout)



##### Pipeline (VkPipeline)



##### Command Pool (VkCommandPool)



##### Command Buffer (VkCommandBuffer)



##### Semaphore (VkSemaphore)



##### Fence (VkFence)



### Useful Tips

- Copying directly to a vertex buffer set to be host-visible and host-coherent with the `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` and `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` may not offer the best performance when the device has to read vertices. Rather, it may be more effective to create a temporary staging buffer with the usage flag `VK_BUFFER_USAGE_TRANSFER_SRC_BIT` and host-visible memory, copy vertices to that buffer, and then copy to a device local (`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`) vertex buffer with usage flag `VK_BUFFER_USAGE_TRANSFER_DST_BIT`. That way, once the vertices are copied, they are faster to read and display by the device.