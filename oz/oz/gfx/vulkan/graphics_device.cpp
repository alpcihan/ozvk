#include "oz/gfx/vulkan/graphics_device.h"
#include "oz/core/file.h"

#include <cstring>

namespace oz::gfx::vk {

#define OZ_VK_ASSERT(result) assert(result == VK_SUCCESS)

#if defined(__APPLE__)
#define OZ_REQUIRES_VK_PORTABILITY_SUBSET
#endif

namespace {
static constexpr int FRAMES_IN_FLIGHT = 1;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void*                                       pUserData) {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    }

    return VK_FALSE;
}

} // namespace

GraphicsDevice::GraphicsDevice(const bool enableValidationLayers) {
    // init glfw
    // TODO: seperate glfw logic
    glfwInit();
    uint32_t     extensionCount = 0;
    const char** extensions     = glfwGetRequiredInstanceExtensions(&extensionCount);

    // populate required extensions
    const std::vector<const char*> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,

#ifdef OZ_REQUIRES_VK_PORTABILITY_SUBSET
        "VK_KHR_portability_subset",
#endif
    };
    // populate required instance extensions
    std::vector<const char*> requiredInstanceExtensions(extensions, extensions + extensionCount);
    {
        if (enableValidationLayers) {
            requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        requiredInstanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    // populate layers
    std::vector<const char*> layers = enableValidationLayers ? std::vector<const char*>{"VK_LAYER_KHRONOS_validation"} : std::vector<const char*>{};

    // check if layers are supported
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        std::vector<const char*> supportedLayers;
        for (const char* layerName : layers) {
            bool layerFound = false;
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (layerFound) {
                supportedLayers.push_back(layerName);
            } else {
                std::cerr << "Warning: Validation layer " << layerName << " not available, skipping." << std::endl;
            }
        }
        layers = supportedLayers;
    }

    // populate debug messenger create info
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    {
        debugCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
    }

    // create instance
    {
        VkApplicationInfo appInfo{};
        appInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "oz";
        appInfo.pEngineName      = "oz";
        appInfo.apiVersion       = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredInstanceExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();
        createInfo.enabledLayerCount       = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames     = layers.data();
        createInfo.pNext                   = enableValidationLayers ? &debugCreateInfo : nullptr;
        createInfo.flags                   = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        OZ_VK_ASSERT(vkCreateInstance(&createInfo, nullptr, &m_instance));
    }

    // create debug messenger
    if (enableValidationLayers) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        OZ_VK_ASSERT(func(m_instance, &debugCreateInfo, nullptr, &m_debugMessenger));
    }

    // pick physical device
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        assert(deviceCount > 0);
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data());

        std::cout << "Found " << deviceCount << " physical device(s):\n";

        bool isGPUFound = false;
        for (const auto& physicalDevice : physicalDevices) {
            // Query and print device properties
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

            std::string deviceTypeStr;
            switch (deviceProperties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                deviceTypeStr = "Discrete GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                deviceTypeStr = "Integrated GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                deviceTypeStr = "Virtual GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                deviceTypeStr = "CPU";
                break;
            default:
                deviceTypeStr = "Other";
                break;
            }

            std::cout << "  - " << deviceProperties.deviceName << " (" << deviceTypeStr << ")\n";

            // check queue family support
            std::optional<uint32_t> graphicsFamily;
            uint32_t                queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
            m_queueFamilies.resize(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, m_queueFamilies.data());
            for (int i = 0; i < m_queueFamilies.size(); i++) {
                const auto& queueFamily = m_queueFamilies[i];
                if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    graphicsFamily = i;
                    break;
                }
            }
            bool areQueueFamiliesSupported = graphicsFamily.has_value();

            // check extension support
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());
            uint32_t extensionMatchCount = 0;
            for (const auto& availableExtension : availableExtensions) {
                for (const auto& requiredExtension : requiredExtensions) {
                    if (strcmp(requiredExtension, availableExtension.extensionName) == 0) {
                        extensionMatchCount++;
                    }
                }
            }
            bool areExtensionsSupported = extensionMatchCount == (uint32_t)requiredExtensions.size();

            // check if the GPU is suitable
            bool isSuitable = areExtensionsSupported && areQueueFamiliesSupported;

            if (isSuitable) {
                std::cout << "  -> Selected device: " << deviceProperties.deviceName << "\n";
                m_graphicsFamily = graphicsFamily.value();
                m_physicalDevice = physicalDevice;
                isGPUFound       = true;
                break;
            }
        }
        assert(isGPUFound);
    }
    // create logical device
    {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float                                queuePriority = 1.0f;
        for (uint32_t queueFamily : std::vector<uint32_t>{m_graphicsFamily}) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount       = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos       = queueCreateInfos.data();
        createInfo.enabledLayerCount       = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames     = layers.data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        createInfo.pEnabledFeatures        = &deviceFeatures;

        OZ_VK_ASSERT(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));
    }

    // get device queues
    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);

    // create a command pool
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_graphicsFamily;

        OZ_VK_ASSERT(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool));
    }

    // create a descriptor pool
    {
        const uint32_t DESCRIPTOR_POOL_SIZE = 1024;

        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = DESCRIPTOR_POOL_SIZE;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = DESCRIPTOR_POOL_SIZE;

        OZ_VK_ASSERT(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool));
    }

    // init current frame
    m_currentFrame = 0;

    // create command buffers
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
        m_commandBuffers.emplace_back(std::move(createCommandBuffer()));

    // create synchronization objects
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_imageAvailableSemaphores.push_back(createSemaphore());
        m_renderFinishedSemaphores.push_back(createSemaphore());
        m_inFlightFences.push_back(createFence());
    }
}

GraphicsDevice::~GraphicsDevice() {
    // drain all resource pools (before destroying pools/device)
    auto cleanup = [&](auto& pool) { pool.forEachAlive([&](auto& obj) { obj.free(m_device); }); };
    cleanup(m_descriptorSets);
    cleanup(m_descriptorSetLayouts);
    cleanup(m_renderPasses);
    cleanup(m_buffers);
    cleanup(m_shaders);
    cleanup(m_cmdBuffers);

    // destroy synchronization objects
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        free(m_renderFinishedSemaphores[i]);
        free(m_imageAvailableSemaphores[i]);
        free(m_inFlightFences[i]);
    }
    m_renderFinishedSemaphores.clear();
    m_imageAvailableSemaphores.clear();
    m_inFlightFences.clear();

    cleanup(m_semaphores);
    cleanup(m_fences);
    cleanup(m_windows);

    // destroy descriptor pool
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

    // destroy command pool
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    m_commandPool = VK_NULL_HANDLE;

    // destroy device
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;

    // destroy debug messenger
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }

    // destroy instance
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;

    // destroy glfw
    glfwTerminate();
}

Window GraphicsDevice::createWindow(const uint32_t width, const uint32_t height, const char* name) {
    // create window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* vkWindow = glfwCreateWindow(width, height, name, nullptr, nullptr);

    // create surface
    VkSurfaceKHR vkSurface;
    OZ_VK_ASSERT(glfwCreateWindowSurface(m_instance, vkWindow, nullptr, &vkSurface));

    // create swap chain
    VkSwapchainKHR           vkSwapChain;
    VkExtent2D               vkSwapChainExtent;
    VkFormat                 vkSwapChainImageFormat;
    std::vector<VkImage>     vkSwapChainImages;
    std::vector<VkImageView> vkSwapChainImageViews;
    VkQueue                  vkPresentQueue;
    {
        // query swap chain support
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
        {
            // get physical device surface capabilities
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, vkSurface, &capabilities);

            // get physical device surface formats
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, vkSurface, &formatCount, nullptr);
            if (formatCount != 0) {
                formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, vkSurface, &formatCount, formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, vkSurface, &presentModeCount, nullptr);
            if (presentModeCount != 0) {
                presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, vkSurface, &presentModeCount, presentModes.data());
            }
        }

        // choose surface format
        VkSurfaceFormatKHR surfaceFormat;
        {
            surfaceFormat = formats[0];

            for (const auto& availableFormat : formats) {
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    surfaceFormat = availableFormat;
                }
            }

            vkSwapChainImageFormat = surfaceFormat.format;
        }

        // choose present mode
        VkPresentModeKHR presentMode;
        {
            presentMode = VK_PRESENT_MODE_FIFO_KHR;

            for (const auto& availablePresentMode : presentModes) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    presentMode = availablePresentMode;
                }
            }
        }

        // choose extent
        {
            vkSwapChainExtent = capabilities.currentExtent;

            if (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
                int width, height;
                glfwGetFramebufferSize(vkWindow, &width, &height);

                VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

                actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
                actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

                vkSwapChainExtent = actualExtent;
            }
        }

        // image count
        uint32_t imageCount = capabilities.minImageCount + 1;
        {
            if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
                imageCount = capabilities.maxImageCount;
            }
        }

        // get present queue and create swap chain
        {
            uint32_t presentFamily      = VK_QUEUE_FAMILY_IGNORED;
            bool     presentFamilyFound = false;
            for (int i = 0; i < m_queueFamilies.size(); i++) {
                const auto& queueFamily = m_queueFamilies[i];

                // present family
                // TODO: compare same family with graphics queue
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, vkSurface, &presentSupport);
                if (presentSupport) {
                    presentFamily      = i;
                    presentFamilyFound = true;
                    break;
                }
            }

            assert(presentFamilyFound);

            // getpresent queue
            vkGetDeviceQueue(m_device, presentFamily, 0, &vkPresentQueue);

            // create swap chain
            {
                VkSwapchainCreateInfoKHR createInfo{};
                createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
                createInfo.surface          = vkSurface;
                createInfo.minImageCount    = imageCount;
                createInfo.imageFormat      = surfaceFormat.format;
                createInfo.imageColorSpace  = surfaceFormat.colorSpace;
                createInfo.imageExtent      = vkSwapChainExtent;
                createInfo.imageArrayLayers = 1;
                createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                createInfo.preTransform     = capabilities.currentTransform;
                createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                createInfo.presentMode      = presentMode;
                createInfo.clipped          = VK_TRUE;
                createInfo.oldSwapchain     = VK_NULL_HANDLE;

                uint32_t queueFamilyIndices[] = {m_graphicsFamily, presentFamily};

                if (m_graphicsFamily != presentFamily) {
                    createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
                    createInfo.queueFamilyIndexCount = 2;
                    createInfo.pQueueFamilyIndices   = queueFamilyIndices;
                } else {
                    createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
                    createInfo.queueFamilyIndexCount = 0;       // optional
                    createInfo.pQueueFamilyIndices   = nullptr; // optional
                }

                OZ_VK_ASSERT(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &vkSwapChain));
            }
        }

        // get swap chain images
        vkGetSwapchainImagesKHR(m_device, vkSwapChain, &imageCount, nullptr);
        vkSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, vkSwapChain, &imageCount, vkSwapChainImages.data());

        // create image views
        vkSwapChainImageViews.resize(vkSwapChainImages.size());
        for (size_t i = 0; i < vkSwapChainImageViews.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image                           = vkSwapChainImages[i];
            createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format                          = vkSwapChainImageFormat;
            createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel   = 0;
            createInfo.subresourceRange.levelCount     = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount     = 1;

            OZ_VK_ASSERT(vkCreateImageView(m_device, &createInfo, nullptr, &vkSwapChainImageViews[i]));
        }
    }

    // create window object
    WindowObject obj{};
    obj.vkWindow               = vkWindow;
    obj.vkSurface              = vkSurface;
    obj.vkSwapChain            = vkSwapChain;
    obj.vkSwapChainExtent      = vkSwapChainExtent;
    obj.vkSwapChainImageFormat = vkSwapChainImageFormat;
    obj.vkSwapChainImages      = std::move(vkSwapChainImages);
    obj.vkSwapChainImageViews  = std::move(vkSwapChainImageViews);
    obj.vkPresentQueue         = vkPresentQueue;
    obj.vkInstance             = m_instance;

    return Window{m_windows.add(std::move(obj))};
}

CommandBuffer GraphicsDevice::createCommandBuffer() {
    // allocate command buffer
    VkCommandBuffer vkCommandBuffer;
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = m_commandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        OZ_VK_ASSERT(vkAllocateCommandBuffers(m_device, &allocInfo, &vkCommandBuffer));
    }

    CommandBufferObject obj{};
    obj.vkCommandBuffer = vkCommandBuffer;
    return CommandBuffer{m_cmdBuffers.add(std::move(obj))};
}

Shader GraphicsDevice::createShader(const std::string& path, ShaderStage stage) {
    std::string absolutePath = file::getBuildPath() + "/resources/shaders/";
    absolutePath += path + ".spv";
    auto code = file::readFile(absolutePath);

    // create shader module
    VkShaderModule shaderModule;
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

        OZ_VK_ASSERT(vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule));
    }

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage  = (VkShaderStageFlagBits)stage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName  = "main";

    ShaderObject obj{};
    obj.stage                           = stage;
    obj.vkShaderModule                  = shaderModule;
    obj.vkPipelineShaderStageCreateInfo = shaderStageInfo;

    return Shader{m_shaders.add(std::move(obj))};
}

RenderPass GraphicsDevice::createRenderPass(Shader                                  vertexShader,
                                            Shader                                  fragmentShader,
                                            Window                                  window,
                                            const VertexLayoutInfo&                 vertexLayout,
                                            const std::vector<DescriptorSetLayout>& descriptorSetLayouts) {
    auto& windowObj = m_windows.get(window.id);
    auto& vertObj   = m_shaders.get(vertexShader.id);
    auto& fragObj   = m_shaders.get(fragmentShader.id);

    // create render pass
    VkRenderPass vkRenderPass;
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = windowObj.vkSwapChainImageFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments    = &colorAttachment;
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        OZ_VK_ASSERT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &vkRenderPass));
    }

    // create pipeline layout
    VkPipelineLayout                   vkPipelineLayout;
    std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    for (const auto& layout : descriptorSetLayouts) {
        vkDescriptorSetLayouts.push_back(m_descriptorSetLayouts.get(layout.id).vkDescriptorSetLayout);
    }

    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = vkDescriptorSetLayouts.size();
        pipelineLayoutInfo.pSetLayouts            = vkDescriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges    = nullptr;

        OZ_VK_ASSERT(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &vkPipelineLayout));
    }

    // create vertex state input info
    VkPipelineVertexInputStateCreateInfo           vertexInputInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkVertexInputBindingDescription                bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(vertexLayout.vertexLayoutAttributes.size());
    {
        if (vertexLayout.vertexLayoutAttributes.size() == 0) {
            vertexInputInfo.vertexBindingDescriptionCount   = 0;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions      = nullptr;
            vertexInputInfo.pVertexAttributeDescriptions    = nullptr;
        } else {
            for (int i = 0; i < vertexLayout.vertexLayoutAttributes.size(); i++) {
                auto& vkAttributeDescription = attributeDescriptions[i];
                auto& attribute              = vertexLayout.vertexLayoutAttributes[i];

                vkAttributeDescription.binding  = 0;
                vkAttributeDescription.location = i;
                vkAttributeDescription.format   = (VkFormat)attribute.format;
                vkAttributeDescription.offset   = attribute.offset;
            }
            bindingDescription.binding   = 0;
            bindingDescription.stride    = vertexLayout.vertexSize;
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            vertexInputInfo.vertexBindingDescriptionCount   = 1;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
            vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();
        }
    }

    // create graphics pipeline
    VkPipeline vkGraphicsPipeline;
    {
        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(windowObj.vkSwapChainExtent.width);
        viewport.height   = static_cast<float>(windowObj.vkSwapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = windowObj.vkSwapChainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth               = 1.0f;
        rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable         = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable         = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable     = VK_FALSE;
        colorBlending.logicOp           = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount   = 1;
        colorBlending.pAttachments      = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;

        VkPipelineShaderStageCreateInfo stages[] = {vertObj.vkPipelineShaderStageCreateInfo, fragObj.vkPipelineShaderStageCreateInfo};
        pipelineInfo.pStages                     = stages;
        pipelineInfo.pVertexInputState           = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState         = &inputAssembly;
        pipelineInfo.pViewportState              = &viewportState;
        pipelineInfo.pRasterizationState         = &rasterizer;
        pipelineInfo.pMultisampleState           = &multisampling;
        pipelineInfo.pColorBlendState            = &colorBlending;
        pipelineInfo.pDynamicState               = &dynamicState;
        pipelineInfo.layout                      = vkPipelineLayout;
        pipelineInfo.renderPass                  = vkRenderPass;
        pipelineInfo.subpass                     = 0;

        OZ_VK_ASSERT(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkGraphicsPipeline));
    }

    // create frame buffers
    std::vector<VkFramebuffer> vkFrameBuffers(windowObj.vkSwapChainImageViews.size());
    for (size_t i = 0; i < windowObj.vkSwapChainImageViews.size(); i++) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = vkRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments    = &windowObj.vkSwapChainImageViews[i];
        framebufferInfo.width           = windowObj.vkSwapChainExtent.width;
        framebufferInfo.height          = windowObj.vkSwapChainExtent.height;
        framebufferInfo.layers          = 1;

        OZ_VK_ASSERT(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &vkFrameBuffers[i]));
    }

    RenderPassObject obj{};
    obj.vkRenderPass       = vkRenderPass;
    obj.vkPipelineLayout   = vkPipelineLayout;
    obj.vkGraphicsPipeline = vkGraphicsPipeline;
    obj.vkExtent           = windowObj.vkSwapChainExtent;
    obj.vkFrameBuffers     = std::move(vkFrameBuffers);

    return RenderPass{m_renderPasses.add(std::move(obj))};
}

Semaphore GraphicsDevice::createSemaphore() {
    VkSemaphore vkSemaphore;
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        OZ_VK_ASSERT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &vkSemaphore));
    }

    SemaphoreObject obj{};
    obj.vkSemaphore = vkSemaphore;
    return Semaphore{m_semaphores.add(std::move(obj))};
}

Buffer GraphicsDevice::createBuffer(BufferType bufferType, uint64_t size, const void* data) {
    // init buffer info and buffer flags
    bool                  persistent = false;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkBufferCreateInfo    bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = 0;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    switch (bufferType) {
    case BufferType::Vertex:
        bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    case BufferType::Index:
        bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    case BufferType::Staging:
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
    case BufferType::Uniform:
        bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        persistent = true;
        break;
    default:
        throw std::runtime_error("Not supported buffer type!");
        break;
    }

    // create buffer
    VkBuffer vkBuffer;
    OZ_VK_ASSERT(vkCreateBuffer(m_device, &bufferInfo, nullptr, &vkBuffer));

    // find suitable memory and allocate
    VkDeviceMemory vkBufferMemory;
    {
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device, vkBuffer, &memRequirements);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

        uint32_t i = 0;
        for (; i < memProperties.memoryTypeCount; i++) {
            if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                break;
            }
        }
        if (i >= memProperties.memoryTypeCount) {
            throw std::runtime_error("Failed to find suitable memory type!");
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = i;

        OZ_VK_ASSERT(vkAllocateMemory(m_device, &allocInfo, nullptr, &vkBufferMemory));
    }

    // bind memory
    vkBindBufferMemory(m_device, vkBuffer, vkBufferMemory, 0);

    // copy the data to the buffer memory
    void* pData = nullptr;
    if (data == nullptr) {
        if (persistent) {
            vkMapMemory(m_device, vkBufferMemory, 0, size, 0, &pData);
        }
    } else {
        vkMapMemory(m_device, vkBufferMemory, 0, size, 0, &pData);
        memcpy(pData, data, (size_t)size);
        if (!persistent) {
            vkUnmapMemory(m_device, vkBufferMemory);
        }
    }

    BufferObject obj{};
    obj.vkBuffer = vkBuffer;
    obj.vkMemory = vkBufferMemory;
    obj.data     = pData;

    return Buffer{m_buffers.add(std::move(obj))};
}

DescriptorSetLayout GraphicsDevice::createDescriptorSetLayout(const DescriptorSetLayoutInfo& setLayout) {
    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings(setLayout.bindings.size());
    for (int bindingIdx = 0; bindingIdx < setLayout.bindings.size(); bindingIdx++) {
        const DescriptorSetLayoutBindingInfo& setLayoutBinding = setLayout.bindings[bindingIdx];

        descriptorSetLayoutBindings[bindingIdx].binding            = bindingIdx;
        descriptorSetLayoutBindings[bindingIdx].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorSetLayoutBindings[bindingIdx].descriptorCount    = 1;
        descriptorSetLayoutBindings[bindingIdx].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
        descriptorSetLayoutBindings[bindingIdx].pImmutableSamplers = nullptr;
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = descriptorSetLayoutBindings.size();
    layoutInfo.pBindings    = descriptorSetLayoutBindings.data();

    VkDescriptorSetLayout vkDescriptorSetLayout;
    OZ_VK_ASSERT(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &vkDescriptorSetLayout));

    DescriptorSetLayoutObject obj{};
    obj.vkDescriptorSetLayout = vkDescriptorSetLayout;

    return DescriptorSetLayout{m_descriptorSetLayouts.add(std::move(obj))};
}

DescriptorSet GraphicsDevice::createDescriptorSet(DescriptorSetLayout descriptorSetLayout, const DescriptorSetInfo& descriptorSetInfo) {
    auto& layoutObj = m_descriptorSetLayouts.get(descriptorSetLayout.id);

    VkDescriptorSetAllocateInfo allocInfo{};
    {
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &layoutObj.vkDescriptorSetLayout;
    }

    VkDescriptorSet vkDescriptorSet;
    OZ_VK_ASSERT(vkAllocateDescriptorSets(m_device, &allocInfo, &vkDescriptorSet));

    for (int bindingIdx = 0; bindingIdx < descriptorSetInfo.bindings.size(); bindingIdx++) {
        const DescriptorSetBindingInfo& descriptorSetBinding = descriptorSetInfo.bindings[bindingIdx];

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_buffers.get(descriptorSetBinding.bufferInfo.buffer.id).vkBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range  = descriptorSetBinding.bufferInfo.range;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet           = vkDescriptorSet;
        descriptorWrite.dstBinding       = bindingIdx;
        descriptorWrite.dstArrayElement  = 0;
        descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount  = 1;
        descriptorWrite.pBufferInfo      = &bufferInfo;
        descriptorWrite.pImageInfo       = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
    }

    DescriptorSetObject obj{};
    obj.vkDescriptorSet  = vkDescriptorSet;
    obj.vkDescriptorPool = m_descriptorPool;

    return DescriptorSet{m_descriptorSets.add(std::move(obj))};
}

void GraphicsDevice::waitIdle() const { vkDeviceWaitIdle(m_device); }

Fence GraphicsDevice::createFence() {
    VkFence vkFence;
    {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        OZ_VK_ASSERT(vkCreateFence(m_device, &fenceInfo, nullptr, &vkFence));
    }

    FenceObject obj{};
    obj.vkFence = vkFence;
    return Fence{m_fences.add(std::move(obj))};
}

void GraphicsDevice::waitFences(Fence fence, uint32_t fenceCount, bool waitAll) const {
    vkWaitForFences(m_device, fenceCount, &m_fences.get(fence.id).vkFence, waitAll ? VK_TRUE : VK_FALSE, UINT64_MAX);
}

void GraphicsDevice::resetFences(Fence fence, uint32_t fenceCount) const { vkResetFences(m_device, fenceCount, &m_fences.get(fence.id).vkFence); }

CommandBuffer GraphicsDevice::getCurrentCommandBuffer() const { return m_commandBuffers[m_currentFrame]; }

uint32_t GraphicsDevice::getCurrentImage(Window window) const {
    glfwPollEvents();

    waitFences(m_inFlightFences[m_currentFrame], 1);
    resetFences(m_inFlightFences[m_currentFrame], 1);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_device,
                          m_windows.get(window.id).vkSwapChain,
                          UINT64_MAX,
                          m_semaphores.get(m_imageAvailableSemaphores[m_currentFrame].id).vkSemaphore,
                          VK_NULL_HANDLE,
                          &imageIndex);

    return imageIndex;
}

uint32_t GraphicsDevice::getCurrentFrame() const { return m_currentFrame; }

bool GraphicsDevice::isWindowOpen(Window window) const { return !glfwWindowShouldClose(m_windows.get(window.id).vkWindow); }

void GraphicsDevice::presentImage(Window window, uint32_t imageIndex) {
    auto& windowObj = m_windows.get(window.id);
    {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &m_semaphores.get(m_renderFinishedSemaphores[m_currentFrame].id).vkSemaphore;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &windowObj.vkSwapChain;
        presentInfo.pImageIndices      = &imageIndex;
        presentInfo.pResults           = nullptr;

        OZ_VK_ASSERT(vkQueuePresentKHR(windowObj.vkPresentQueue, &presentInfo));
    }

    m_currentFrame = (m_currentFrame + 1) % FRAMES_IN_FLIGHT;
}

void GraphicsDevice::beginCmd(CommandBuffer cmd, bool isSingleUse) const {
    auto& cmdObj = m_cmdBuffers.get(cmd.id);
    vkResetCommandBuffer(cmdObj.vkCommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = isSingleUse ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
    beginInfo.pInheritanceInfo = nullptr;

    OZ_VK_ASSERT(vkBeginCommandBuffer(cmdObj.vkCommandBuffer, &beginInfo));
}

void GraphicsDevice::endCmd(CommandBuffer cmd) const { OZ_VK_ASSERT(vkEndCommandBuffer(m_cmdBuffers.get(cmd.id).vkCommandBuffer)); }

void GraphicsDevice::submitCmd(CommandBuffer cmd) const {
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &m_semaphores.get(m_imageAvailableSemaphores[m_currentFrame].id).vkSemaphore;
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &m_cmdBuffers.get(cmd.id).vkCommandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &m_semaphores.get(m_renderFinishedSemaphores[m_currentFrame].id).vkSemaphore;

        OZ_VK_ASSERT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_fences.get(m_inFlightFences[m_currentFrame].id).vkFence));
    }
}

void GraphicsDevice::beginRenderPass(CommandBuffer cmd, RenderPass renderPass, uint32_t imageIndex) const {
    auto& cmdObj = m_cmdBuffers.get(cmd.id);
    auto& rpObj  = m_renderPasses.get(renderPass.id);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = rpObj.vkRenderPass;
    renderPassInfo.framebuffer       = rpObj.vkFrameBuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = rpObj.vkExtent;
    VkClearValue clearColor          = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount   = 1;
    renderPassInfo.pClearValues      = &clearColor;

    vkCmdBeginRenderPass(cmdObj.vkCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmdObj.vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rpObj.vkGraphicsPipeline);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(rpObj.vkExtent.width);
    viewport.height   = static_cast<float>(rpObj.vkExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdObj.vkCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = rpObj.vkExtent;
    vkCmdSetScissor(cmdObj.vkCommandBuffer, 0, 1, &scissor);
}

void GraphicsDevice::endRenderPass(CommandBuffer cmd) const { vkCmdEndRenderPass(m_cmdBuffers.get(cmd.id).vkCommandBuffer); }

void GraphicsDevice::draw(CommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const {
    vkCmdDraw(m_cmdBuffers.get(cmd.id).vkCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void GraphicsDevice::drawIndexed(
    CommandBuffer cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance) const {
    vkCmdDrawIndexed(m_cmdBuffers.get(cmd.id).vkCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void GraphicsDevice::bindVertexBuffer(CommandBuffer cmd, Buffer vertexBuffer) {
    VkBuffer     vertexBuffers[] = {m_buffers.get(vertexBuffer.id).vkBuffer};
    VkDeviceSize offsets[]       = {0};
    vkCmdBindVertexBuffers(m_cmdBuffers.get(cmd.id).vkCommandBuffer, 0, 1, vertexBuffers, offsets);
}

void GraphicsDevice::bindIndexBuffer(CommandBuffer cmd, Buffer indexBuffer) {
    vkCmdBindIndexBuffer(m_cmdBuffers.get(cmd.id).vkCommandBuffer, m_buffers.get(indexBuffer.id).vkBuffer, 0, VK_INDEX_TYPE_UINT16);
}

void GraphicsDevice::bindDescriptorSet(CommandBuffer cmd, RenderPass renderPass, DescriptorSet descriptorSet, uint32_t setIndex) {
    vkCmdBindDescriptorSets(m_cmdBuffers.get(cmd.id).vkCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_renderPasses.get(renderPass.id).vkPipelineLayout,
                            setIndex,
                            1,
                            &(m_descriptorSets.get(descriptorSet.id).vkDescriptorSet),
                            0,
                            nullptr);
}

void GraphicsDevice::updateBuffer(Buffer buffer, const void* data, size_t size) { memcpy(m_buffers.get(buffer.id).data, data, size); }

void GraphicsDevice::copyBuffer(Buffer src, Buffer dst, uint64_t size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = size;
    vkCmdCopyBuffer(commandBuffer, m_buffers.get(src.id).vkBuffer, m_buffers.get(dst.id).vkBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

void GraphicsDevice::free(Window window) {
    m_windows.get(window.id).free(m_device);
    m_windows.remove(window.id);
}

void GraphicsDevice::free(Shader shader) {
    m_shaders.get(shader.id).free(m_device);
    m_shaders.remove(shader.id);
}

void GraphicsDevice::free(RenderPass renderPass) {
    m_renderPasses.get(renderPass.id).free(m_device);
    m_renderPasses.remove(renderPass.id);
}

void GraphicsDevice::free(Semaphore semaphore) {
    m_semaphores.get(semaphore.id).free(m_device);
    m_semaphores.remove(semaphore.id);
}

void GraphicsDevice::free(Fence fence) {
    m_fences.get(fence.id).free(m_device);
    m_fences.remove(fence.id);
}

void GraphicsDevice::free(CommandBuffer commandBuffer) {
    m_cmdBuffers.get(commandBuffer.id).free(m_device);
    m_cmdBuffers.remove(commandBuffer.id);
}

void GraphicsDevice::free(Buffer buffer) {
    m_buffers.get(buffer.id).free(m_device);
    m_buffers.remove(buffer.id);
}

void GraphicsDevice::free(DescriptorSetLayout descriptorSetLayout) {
    m_descriptorSetLayouts.get(descriptorSetLayout.id).free(m_device);
    m_descriptorSetLayouts.remove(descriptorSetLayout.id);
}

void GraphicsDevice::free(DescriptorSet descriptorSet) {
    m_descriptorSets.get(descriptorSet.id).free(m_device);
    m_descriptorSets.remove(descriptorSet.id);
}

} // namespace oz::gfx::vk
