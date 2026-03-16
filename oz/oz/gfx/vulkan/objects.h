#pragma once

#include "oz/gfx/vulkan/common.h"

namespace oz::gfx::vk {

#define OZ_VK_OBJECT(NAME) typedef struct NAME##Object* NAME;

OZ_VK_OBJECT(Window);
OZ_VK_OBJECT(Shader);
OZ_VK_OBJECT(RenderPass);
OZ_VK_OBJECT(Fence);
OZ_VK_OBJECT(Semaphore);
OZ_VK_OBJECT(CommandBuffer);
OZ_VK_OBJECT(Buffer);
OZ_VK_OBJECT(DescriptorSetLayout);
OZ_VK_OBJECT(DescriptorSet);

} // namespace oz::gfx::vk 