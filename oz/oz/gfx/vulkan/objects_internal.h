#pragma once

#include "oz/gfx/vulkan/common.h"

namespace oz::gfx::vk {

struct IObject {
    virtual ~IObject()                   = default;
    virtual void free(VkDevice vkDevice) = 0;
};

struct ShaderObject final : IObject {
    ShaderStage stage;

    VkShaderModule                  vkShaderModule                  = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo = {};

    void free(VkDevice vkDevice) override { vkDestroyShaderModule(vkDevice, vkShaderModule, nullptr); }
};

struct RenderPassObject final : IObject {
    VkRenderPass               vkRenderPass       = VK_NULL_HANDLE;
    VkPipelineLayout           vkPipelineLayout   = VK_NULL_HANDLE;
    VkPipeline                 vkGraphicsPipeline = VK_NULL_HANDLE;
    VkExtent2D                 vkExtent           = {};
    std::vector<VkFramebuffer> vkFrameBuffers;

    void free(VkDevice vkDevice) override {
        vkDestroyPipeline(vkDevice, vkGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, nullptr);
        vkDestroyRenderPass(vkDevice, vkRenderPass, nullptr);

        for (auto framebuffer : vkFrameBuffers) {
            vkDestroyFramebuffer(vkDevice, framebuffer, nullptr);
        }
    }
};

struct SemaphoreObject final : IObject {
    VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    // TODO: only vkSemaphore is supported
    void free(VkDevice vkDevice) override { vkDestroySemaphore(vkDevice, vkSemaphore, nullptr); }
};

struct FenceObject final : IObject {
    VkFence vkFence = VK_NULL_HANDLE;
    // TODO: only vkFence is supported
    void free(VkDevice vkDevice) override { vkDestroyFence(vkDevice, vkFence, nullptr); }
};

struct WindowObject final : IObject {
    GLFWwindow*  vkWindow  = nullptr;
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

    VkSwapchainKHR           vkSwapChain            = VK_NULL_HANDLE;
    VkExtent2D               vkSwapChainExtent      = {};
    VkFormat                 vkSwapChainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage>     vkSwapChainImages;
    std::vector<VkImageView> vkSwapChainImageViews;

    VkQueue vkPresentQueue = VK_NULL_HANDLE;

    VkInstance vkInstance = VK_NULL_HANDLE; // referenced to used on free

    void free(VkDevice vkDevice) override {
        // swap chain
        vkDestroySwapchainKHR(vkDevice, vkSwapChain, nullptr);

        // image views
        for (auto imageView : vkSwapChainImageViews) {
            vkDestroyImageView(vkDevice, imageView, nullptr);
        }

        // images
        // for (auto image : window->vkSwapChainImages) {
        //    vkDestroyImage(vkDevice, image, nullptr);
        // }

        // surface
        if (vkSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
        }

        // window
        if (vkWindow != nullptr) {
            glfwDestroyWindow(vkWindow);
        }
    }
};

struct CommandBufferObject final : IObject {
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;

    void free(VkDevice vkDevice) override {}
};

struct BufferObject final : IObject {
    VkBuffer       vkBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vkMemory = VK_NULL_HANDLE;
    void*          data     = nullptr;

    void free(VkDevice vkDevice) override {
        vkDestroyBuffer(vkDevice, vkBuffer, nullptr);
        vkFreeMemory(vkDevice, vkMemory, nullptr);
        data = nullptr;
    }
};

struct DescriptorSetLayoutObject final : IObject {
    VkDescriptorSetLayout vkDescriptorSetLayout = VK_NULL_HANDLE;

    void free(VkDevice vkDevice) override { vkDestroyDescriptorSetLayout(vkDevice, vkDescriptorSetLayout, nullptr); }
};

struct DescriptorSetObject final : IObject {
    VkDescriptorSet vkDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;
    void free(VkDevice vkDevice) override {
        if (vkDescriptorSet != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(vkDevice, vkDescriptorPool, 1, &vkDescriptorSet);
        }
    }
};

#define OZ_CREATE_VK_OBJECT(TYPE) new TYPE##Object

#define OZ_FREE_VK_OBJECT(vkDevice, vkObject) \
    if (vkObject) {                           \
        vkObject->free(vkDevice);             \
        delete vkObject;                      \
        vkObject = nullptr;                   \
    } //\

} // namespace oz::gfx::vk 