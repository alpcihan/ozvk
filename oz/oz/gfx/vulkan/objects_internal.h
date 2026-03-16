#pragma once

#include "oz/gfx/vulkan/common.h"
#include "oz/gfx/vulkan/enums.h"

namespace oz::gfx::vk {

struct ShaderObject {
    ShaderStage stage;

    VkShaderModule                  vkShaderModule                  = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo = {};

    void free(VkDevice vkDevice) { vkDestroyShaderModule(vkDevice, vkShaderModule, nullptr); }
};

struct RenderPassObject {
    VkRenderPass               vkRenderPass       = VK_NULL_HANDLE;
    VkPipelineLayout           vkPipelineLayout   = VK_NULL_HANDLE;
    VkPipeline                 vkGraphicsPipeline = VK_NULL_HANDLE;
    VkExtent2D                 vkExtent           = {};
    std::vector<VkFramebuffer> vkFrameBuffers;

    void free(VkDevice vkDevice) {
        vkDestroyPipeline(vkDevice, vkGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, nullptr);
        vkDestroyRenderPass(vkDevice, vkRenderPass, nullptr);

        for (auto framebuffer : vkFrameBuffers) {
            vkDestroyFramebuffer(vkDevice, framebuffer, nullptr);
        }
    }
};

struct SemaphoreObject {
    VkSemaphore vkSemaphore = VK_NULL_HANDLE;

    void free(VkDevice vkDevice) { vkDestroySemaphore(vkDevice, vkSemaphore, nullptr); }
};

struct FenceObject {
    VkFence vkFence = VK_NULL_HANDLE;

    void free(VkDevice vkDevice) { vkDestroyFence(vkDevice, vkFence, nullptr); }
};

struct WindowObject {
    GLFWwindow*  vkWindow  = nullptr;
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

    VkSwapchainKHR           vkSwapChain            = VK_NULL_HANDLE;
    VkExtent2D               vkSwapChainExtent      = {};
    VkFormat                 vkSwapChainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage>     vkSwapChainImages;
    std::vector<VkImageView> vkSwapChainImageViews;

    VkQueue vkPresentQueue = VK_NULL_HANDLE;

    VkInstance vkInstance = VK_NULL_HANDLE; // referenced to used on free

    void free(VkDevice vkDevice) {
        vkDestroySwapchainKHR(vkDevice, vkSwapChain, nullptr);

        for (auto imageView : vkSwapChainImageViews) {
            vkDestroyImageView(vkDevice, imageView, nullptr);
        }

        if (vkSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
        }

        if (vkWindow != nullptr) {
            glfwDestroyWindow(vkWindow);
        }
    }
};

struct CommandBufferObject {
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;

    void free(VkDevice vkDevice) {}
};

struct BufferObject {
    VkBuffer       vkBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vkMemory = VK_NULL_HANDLE;
    void*          data     = nullptr;

    void free(VkDevice vkDevice) {
        vkDestroyBuffer(vkDevice, vkBuffer, nullptr);
        vkFreeMemory(vkDevice, vkMemory, nullptr);
        data = nullptr;
    }
};

struct DescriptorSetLayoutObject {
    VkDescriptorSetLayout vkDescriptorSetLayout = VK_NULL_HANDLE;

    void free(VkDevice vkDevice) { vkDestroyDescriptorSetLayout(vkDevice, vkDescriptorSetLayout, nullptr); }
};

struct DescriptorSetObject {
    VkDescriptorSet  vkDescriptorSet  = VK_NULL_HANDLE;
    VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;

    void free(VkDevice vkDevice) {
        if (vkDescriptorSet != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(vkDevice, vkDescriptorPool, 1, &vkDescriptorSet);
        }
    }
};

} // namespace oz::gfx::vk
