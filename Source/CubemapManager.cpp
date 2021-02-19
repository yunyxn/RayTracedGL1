// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include "CubemapManager.h"

#include "Generated/ShaderCommonC.h"
#include "Const.h"

constexpr uint32_t MAX_CUBEMAP_COUNT = 32;

RTGL1::CubemapManager::CubemapManager(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> _allocator,
    std::shared_ptr<SamplerManager> _samplerManager,
    const std::shared_ptr<CommandBufferManager> &_cmdManager)
:
    device(_device),
    allocator(std::move(_allocator)),
    samplerManager(std::move(_samplerManager)),
    cubemaps(MAX_CUBEMAP_COUNT),
    emptyCubemapInfo{}
{
    cubemapDesc = std::make_shared<TextureDescriptors>(device, MAX_CUBEMAP_COUNT, BINDING_CUBEMAPS);
    cubemapUploader = std::make_shared<TextureUploader>(device, allocator);

    VkCommandBuffer cmd = _cmdManager->StartGraphicsCmd();
    CreateEmptyCubemap(cmd);
    _cmdManager->Submit(cmd);
    _cmdManager->WaitGraphicsIdle();
}

void RTGL1::CubemapManager::CreateEmptyCubemap(VkCommandBuffer cmd)
{
    uint32_t whitePixel = 0xFFFFFFFF;

    RgCubemapCreateInfo info = {};
    info.sideSize = 1;
    info.useMipmaps = 0;
    info.isSRGB = false;
    info.disableOverride = true;
    info.filter = RG_SAMPLER_FILTER_NEAREST;

    for (uint32_t i = 0; i < 6; i++)
    {
        info.data[i] = &whitePixel;
    }

    uint32_t index = CreateCubemap(cmd, 0, info);
    assert(index == RG_EMPTY_CUBEMAP);

    cubemapDesc->SetEmptyTextureInfo(cubemaps[RG_EMPTY_CUBEMAP].view, cubemaps[RG_EMPTY_CUBEMAP].sampler);
}

RTGL1::CubemapManager::~CubemapManager()
{
    std::vector<Texture> *arrays[MAX_FRAMES_IN_FLIGHT + 1] = {};
    arrays[MAX_FRAMES_IN_FLIGHT] = &cubemaps;
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        arrays[i] = &cubemapsToDestroy[i];
    }

    for (auto *arr : arrays)
    {
        for (auto &t : *arr)
        {
            assert((t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE && t.sampler == VK_NULL_HANDLE) ||
                   (t.image != VK_NULL_HANDLE && t.view != VK_NULL_HANDLE && t.sampler != VK_NULL_HANDLE));

            if (t.image != VK_NULL_HANDLE)
            {
                vkDestroyImage(device, t.image, nullptr);
                vkDestroyImageView(device, t.view, nullptr);
            }
        }
    }
}

uint32_t RTGL1::CubemapManager::CreateCubemap(VkCommandBuffer cmd, uint32_t frameIndex, const RgCubemapCreateInfo &info)
{
    auto f = std::find_if(cubemaps.begin(), cubemaps.end(), [] (const Texture &t)
    {
        // also check if texture's members are all empty or all filled
        assert((t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE && t.sampler == VK_NULL_HANDLE) ||
               (t.image != VK_NULL_HANDLE && t.view != VK_NULL_HANDLE && t.sampler != VK_NULL_HANDLE));

        return t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE && t.sampler == VK_NULL_HANDLE;
    });

    TextureUploader::UploadInfo upload = {};
    upload.cmd = cmd;
    upload.frameIndex = frameIndex;
    upload.size.width = upload.size.height = info.sideSize;
    upload.format = info.isSRGB ? TEXTURE_IMAGE_FORMAT_SRGB : TEXTURE_IMAGE_FORMAT_UNORM;
    upload.bytesPerPixel = TEXTURE_IMAGE_BYTES_PER_PIXEL;
    upload.generateMipmaps = info.useMipmaps;
    upload.isCubemap = true;
    upload.isDynamic = false;
    upload.debugName = nullptr;

    for (uint32_t i = 0; i < 6; i++)
    {
        upload.cubemap.faces[i] = info.data[i];
    }

    auto &i = cubemapUploader->UploadImage(upload);

    if (!i.wasUploaded)
    {
        return RG_EMPTY_CUBEMAP;
    }

    f->image = i.image;
    f->view = i.view;
    f->sampler = samplerManager->GetSampler(info.filter, RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    return std::distance(cubemaps.begin(), f);
}

void RTGL1::CubemapManager::DestroyCubemap(uint32_t frameIndex, uint32_t cubemapIndex)
{
    assert(cubemapIndex < MAX_CUBEMAP_COUNT);

    if (cubemapIndex >= MAX_CUBEMAP_COUNT || cubemaps[cubemapIndex].image == VK_NULL_HANDLE)
    {
        return;
    }

    Texture &t = cubemaps[cubemapIndex];

    // add to be destroyed later
    cubemapsToDestroy[frameIndex].push_back(t);

    // clear data
    t.image = VK_NULL_HANDLE;
    t.view = VK_NULL_HANDLE;
    t.sampler = VK_NULL_HANDLE;
}

VkDescriptorSetLayout RTGL1::CubemapManager::GetDescSetLayout() const
{
    return cubemapDesc->GetDescSetLayout();
}

VkDescriptorSet RTGL1::CubemapManager::GetDescSet(uint32_t frameIndex) const
{
    return cubemapDesc->GetDescSet(frameIndex);
}

void RTGL1::CubemapManager::PrepareForFrame(uint32_t frameIndex)
{    
    // destroy delayed textures
    for (auto &t : cubemapsToDestroy[frameIndex])
    {
        vkDestroyImage(device, t.image, nullptr);
        vkDestroyImageView(device, t.view, nullptr);
    }
    cubemapsToDestroy[frameIndex].clear();

    // clear staging buffer that are not in use
    cubemapUploader->ClearStaging(frameIndex);
}

void RTGL1::CubemapManager::SubmitDescriptors(uint32_t frameIndex)
{
    // update desc set with current values
    for (uint32_t i = 0; i < cubemaps.size(); i++)
    {
        if (cubemaps[i].image != VK_NULL_HANDLE)
        {
            cubemapDesc->UpdateTextureDesc(frameIndex, i, cubemaps[i].view, cubemaps[i].sampler);
        }
        else
        {
            // reset descriptor to empty texture
            cubemapDesc->ResetTextureDesc(frameIndex, i);
        }
    }

    cubemapDesc->FlushDescWrites();
}
