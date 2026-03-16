#version 450

layout(set = 0, binding = 0) uniform MVP {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(set = 1, binding = 0) uniform Count {
    uint count;
} count;

layout(set = 1, binding = 1) uniform Num {
    uint num;
} num;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor * vec3(abs(sin(count.count * num.num * 0.01)), 1, 1);
}