#include "oz/oz.h"
using namespace oz::gfx::vk;

// vertex data
struct Vertex {
    glm::vec2 pos;
    glm::vec3 col;
};

const std::vector<Vertex> vertices       = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                            {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                            {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                            {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
VkDeviceSize              vertBufferSize = sizeof(vertices[0]) * vertices.size();

// index data
const std::vector<uint16_t> indices       = {0, 1, 2, 2, 3, 0};
VkDeviceSize                idxBufferSize = sizeof(indices[0]) * indices.size();

// uniform buffer data
struct MVP {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

int main() {
    GraphicsDevice device(true);
    Window         window = device.createWindow(800, 600, "oz");

    // create shaders
    Shader vertShader = device.createShader("uniform.vert", ShaderStage::Vertex);
    Shader fragShader = device.createShader("default.frag", ShaderStage::Fragment);

    // create vertex buffer
    Buffer vertStageBuffer = device.createBuffer(BufferType::Staging, vertBufferSize, vertices.data());
    Buffer vertexBuffer    = device.createBuffer(BufferType::Vertex, vertBufferSize);
    device.copyBuffer(vertStageBuffer, vertexBuffer, vertBufferSize);
    device.free(vertStageBuffer);

    // create index buffer
    Buffer idxStageBuffer = device.createBuffer(BufferType::Staging, idxBufferSize, indices.data());
    Buffer indexBuffer    = device.createBuffer(BufferType::Index, idxBufferSize);
    device.copyBuffer(idxStageBuffer, indexBuffer, idxBufferSize);
    device.free(idxStageBuffer);

    // create uniform buffers
    Buffer mvpBuffer   = device.createBuffer(BufferType::Uniform, sizeof(MVP));
    Buffer countBuffer = device.createBuffer(BufferType::Uniform, sizeof(uint32_t));
    Buffer numBuffer   = device.createBuffer(BufferType::Uniform, sizeof(uint32_t));

    // Create descriptor set layouts
    DescriptorSetLayout mvpLayout = device.createDescriptorSetLayout(DescriptorSetLayoutInfo({
        DescriptorSetLayoutBindingInfo(BindingType::Uniform),
    }));

    DescriptorSetLayout countLayout = device.createDescriptorSetLayout(DescriptorSetLayoutInfo({
        DescriptorSetLayoutBindingInfo(BindingType::Uniform),
        DescriptorSetLayoutBindingInfo(BindingType::Uniform),
    }));

    // Create descriptor sets
    DescriptorSet mvpSet = device.createDescriptorSet(mvpLayout,
                                                      DescriptorSetInfo({
                                                          DescriptorSetBindingInfo(DescriptorSetBufferInfo(mvpBuffer, sizeof(MVP))),
                                                      }));

    DescriptorSet countSet = device.createDescriptorSet(countLayout,
                                                        DescriptorSetInfo({
                                                            DescriptorSetBindingInfo(DescriptorSetBufferInfo(countBuffer, sizeof(uint32_t))),
                                                            DescriptorSetBindingInfo(DescriptorSetBufferInfo(numBuffer, sizeof(uint32_t))),
                                                        }));

    // create render pass
    RenderPass renderPass = device.createRenderPass(vertShader,
                                                    fragShader,
                                                    window,
                                                    VertexLayoutInfo(sizeof(Vertex),
                                                                     {VertexLayoutAttributeInfo(offsetof(Vertex, pos), Format::R32G32_SFLOAT),
                                                                      VertexLayoutAttributeInfo(offsetof(Vertex, col), Format::R32G32B32_SFLOAT)}),
                                                    {mvpLayout, countLayout});

    device.free(mvpLayout);
    device.free(countLayout);

    uint32_t frameCount = 0;
    uint32_t num   = 1;
    // render loop
    while (device.isWindowOpen(window)) {
        uint32_t      imageIndex = device.getCurrentImage(window);
        CommandBuffer cmd        = device.getCurrentCommandBuffer();
        uint32_t      frame      = device.getCurrentFrame();

        // update ubo
        {
            static auto startTime   = std::chrono::high_resolution_clock::now();
            auto        currentTime = std::chrono::high_resolution_clock::now();
            float       time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
            MVP         mvp{};
            mvp.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            mvp.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            mvp.proj  = glm::perspective(glm::radians(45.0f), 800 / (float)600, 0.1f, 10.0f);
            mvp.proj[1][1] *= -1;
            device.updateBuffer(mvpBuffer, &mvp, sizeof(mvp));
            device.updateBuffer(countBuffer, &frameCount, sizeof(frameCount));
            device.updateBuffer(numBuffer, &num, sizeof(num));
        }

        device.beginCmd(cmd);
        device.beginRenderPass(cmd, renderPass, imageIndex);
        device.bindVertexBuffer(cmd, vertexBuffer);
        device.bindIndexBuffer(cmd, indexBuffer);
        device.bindDescriptorSet(cmd, renderPass, mvpSet, 0);
        device.bindDescriptorSet(cmd, renderPass, countSet, 1);

        device.drawIndexed(cmd, indices.size());
        device.endRenderPass(cmd);
        device.endCmd(cmd);

        device.submitCmd(cmd);
        device.presentImage(window, imageIndex);

        frameCount++;
    }
    device.waitIdle();

    // free resources
    device.free(vertShader);
    device.free(fragShader);
    device.free(window);
    device.free(renderPass);
    device.free(vertexBuffer);
    device.free(indexBuffer);
    device.free(mvpBuffer);
    device.free(countBuffer);
    device.free(numBuffer);

    return 0;
}