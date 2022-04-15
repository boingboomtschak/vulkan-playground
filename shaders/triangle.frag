#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 light = vec3(2, 1, 0);
    float amb = 0.2, dif = 0.4, spc = 0.4;
    vec3 N = normalize(inNormal);       // surface normal
    vec3 L = normalize(light-inPosition);  // light vector
    vec3 E = normalize(inPosition);        // eye vertex
    vec3 R = reflect(L, N);            // highlight vector
    float d = dif*max(0, dot(N, L)); // one-sided Lambert
    float h = max(0, dot(R, E)); // highlight term
    float s = spc*pow(h, 100); // specular term
    float intensity = clamp(amb+d+s, 0, 1);
    outColor = intensity * inColor;
}
