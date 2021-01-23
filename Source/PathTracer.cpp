// Copyright (c) 2020-2021 Sultim Tsyrendashiev
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "PathTracer.h"

PathTracer::PathTracer(VkDevice _device, std::shared_ptr<RayTracingPipeline> _rtPipeline)
    : device(_device), rtPipeline(_rtPipeline)
{}

PathTracer::~PathTracer()
{}

void PathTracer::Trace(
    VkCommandBuffer cmd, uint32_t frameIndex, uint32_t width, uint32_t height,
    const std::shared_ptr<ASManager> &asManager,
    const std::shared_ptr<GlobalUniform> &uniform,
    const std::shared_ptr<TextureManager> &textureMgr,
    VkDescriptorSet imagesDescSet)
{
    rtPipeline->Bind(cmd);

    VkDescriptorSet sets[] = {
        // ray tracing acceleration structures
        asManager->GetTLASDescSet(frameIndex),
        // storage images
        imagesDescSet,
        // uniform
        uniform->GetDescSet(frameIndex),
        // vertex data
        asManager->GetBuffersDescSet(frameIndex),
        // textures
        textureMgr->GetDescSet(frameIndex)
    };
    const uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipeline->GetLayout(),
                            0, setCount, sets,
                            0, nullptr);

    VkStridedDeviceAddressRegionKHR raygenEntry, missEntry, hitEntry, callableEntry;
    rtPipeline->GetEntries(raygenEntry, missEntry, hitEntry, callableEntry);

    svkCmdTraceRaysKHR(
        cmd,
        &raygenEntry, &missEntry, &hitEntry, &callableEntry,
        width, height, 1);
}