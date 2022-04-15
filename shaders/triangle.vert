#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outColor;

void main() {
    outColor = inColor;
    outPosition = (ubo.view * ubo.model * vec4(inPosition, 1)).xyz;
    outNormal = (ubo.view * ubo.model * vec4(inNormal, 0)).xyz;
    gl_Position = ubo.proj * vec4(outPosition, 1);
}
