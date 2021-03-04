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

#pragma once

#include <vector>

#include "AutoBuffer.h"
#include "Common.h"
#include "Material.h"
#include "MemoryAllocator.h"
#include "VertexCollectorFilterType.h"

namespace RTGL1
{

struct ShGeometryInstance;

class GeomInfoManager
{
public:
    explicit GeomInfoManager(
        VkDevice device,
        std::shared_ptr<MemoryAllocator> allocator);
    ~GeomInfoManager();

    GeomInfoManager(const GeomInfoManager &other) = delete;
    GeomInfoManager(GeomInfoManager &&other) noexcept = delete;
    GeomInfoManager & operator=(const GeomInfoManager &other) = delete;
    GeomInfoManager & operator=(GeomInfoManager &&other) noexcept = delete;

    uint32_t WriteGeomInfo(
        uint32_t frameIndex, 
        uint32_t localGeomIndex, 
        VertexCollectorFilterTypeFlags flags,
        const ShGeometryInstance &src);

    void WriteStaticGeomInfoMaterials(uint32_t globalGeomIndex, uint32_t layer, const MaterialTextures &src);
    void WriteStaticGeomInfoTransform(uint32_t globalGeomIndex, const RgTransform &src);

    bool CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex, bool insertBarrier = true);
    void ResetOnlyDynamic(uint32_t frameIndex);
    void ResetWithStatic();

    uint32_t GetCount() const;
    uint32_t GetStaticCount() const;
    uint32_t GetDynamicCount() const;
    VkBuffer GetBuffer() const;
    uint32_t GetStaticGeomBaseVertexIndex(uint32_t globalGeomIndex);

private:
    ShGeometryInstance *GetGeomInfoAddress(uint32_t frameIndex, uint32_t globalGeomIndex);
    ShGeometryInstance *GetGeomInfoAddress(uint32_t frameIndex, uint32_t localGeomIndex, VertexCollectorFilterTypeFlags flags);

    // Mark memory to be copied to device local buffer
    void MarkGeomInfoIndexToCopy(uint32_t frameIndex, uint32_t localGeomIndex, VertexCollectorFilterTypeFlags flags);

private:
    VkDevice device;

    // Dynamic geoms must be added only after static ones
    // so the variable "staticGeomCount" is used to "protect" static geoms
    // from deletion as dynamic geoms are readded every frame,
    // but static ones are added very infrequently, e.g. on level load
    uint32_t staticGeomCount;
    uint32_t dynamicGeomCount;

    // buffer for getting info for geometry in BLAS
    std::shared_ptr<AutoBuffer> buffer;

    uint32_t copyRegionLowerBound[MAX_FRAMES_IN_FLIGHT][36];
    uint32_t copyRegionUpperBound[MAX_FRAMES_IN_FLIGHT][36];

    // each geometry has its type as they're can be in different filters
    std::vector<VertexCollectorFilterTypeFlags> geomType;

    // geometry index in its filter's space, i.e.
    // geomIndex = ToOffset(geomType) * MAX_BLAS_GEOMS + geomLocalIndex
    std::vector<uint32_t> globalToLocalIndex;
};

}