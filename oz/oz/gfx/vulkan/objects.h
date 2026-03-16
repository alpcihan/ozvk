#pragma once

#include <cstdint>

namespace oz::gfx::vk {

static constexpr uint32_t INVALID_HANDLE = UINT32_MAX;

#define OZ_HANDLE(NAME)                                                     \
    struct NAME {                                                           \
        uint32_t id = INVALID_HANDLE;                                       \
        bool     isValid() const { return id != INVALID_HANDLE; }           \
        bool     operator==(const NAME& other) const { return id == other.id; } \
        bool     operator!=(const NAME& other) const { return id != other.id; } \
    };

OZ_HANDLE(Window)
OZ_HANDLE(Shader)
OZ_HANDLE(RenderPass)
OZ_HANDLE(Fence)
OZ_HANDLE(Semaphore)
OZ_HANDLE(CommandBuffer)
OZ_HANDLE(Buffer)
OZ_HANDLE(DescriptorSetLayout)
OZ_HANDLE(DescriptorSet)

#undef OZ_HANDLE

} // namespace oz::gfx::vk
