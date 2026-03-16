#pragma once

#include "oz/gfx/vulkan/common.h"
#include "oz/gfx/vulkan/enums.h"

namespace oz::gfx::vk {

#define OZ_CHAINED_SETTER(FUNCNAME, TYPE, NAME) \
    auto& FUNCNAME(TYPE _##NAME) {              \
        NAME = _##NAME;                         \
        return *this;                           \
    }

// Vertex Info

struct VertexLayoutAttributeInfo final {
    size_t offset;
    Format format;

    VertexLayoutAttributeInfo(size_t _offset, Format _format) : offset(_offset), format(_format) {}
};

struct VertexLayoutInfo final {
    uint32_t                               vertexSize;
    std::vector<VertexLayoutAttributeInfo> vertexLayoutAttributes;

    VertexLayoutInfo(uint32_t _vertexSize, std::vector<VertexLayoutAttributeInfo> const& _vertexLayoutAttributes)
        : vertexSize(_vertexSize), vertexLayoutAttributes(_vertexLayoutAttributes) {}
};

// Descriptor Set Layout Info

struct DescriptorSetLayoutBindingInfo {
    BindingType type;

    DescriptorSetLayoutBindingInfo(BindingType _type) : type(_type) {}
};

struct DescriptorSetLayoutInfo {
    std::vector<DescriptorSetLayoutBindingInfo> bindings;

    DescriptorSetLayoutInfo(std::vector<DescriptorSetLayoutBindingInfo> const& _bindings) : bindings(_bindings) {}
};

// Descriptor Set Info

struct DescriptorSetBufferInfo {
    Buffer buffer;
    size_t range;

    DescriptorSetBufferInfo(Buffer _buffer, size_t _range) : buffer(_buffer), range(_range) {}
};  

struct DescriptorSetBindingInfo {
    DescriptorSetBufferInfo bufferInfo;

    DescriptorSetBindingInfo(DescriptorSetBufferInfo _bufferInfo) : bufferInfo(_bufferInfo) {}
};

struct DescriptorSetInfo {
    std::vector<DescriptorSetBindingInfo> bindings;

    DescriptorSetInfo(std::vector<DescriptorSetBindingInfo> const& _bindings) : bindings(_bindings) {}
};

}; // namespace oz::gfx::vk