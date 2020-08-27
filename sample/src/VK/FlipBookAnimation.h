// AMD AMDUtils code
// 
// Copyright(c) 2019 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include "base\UploadHeap.h"
#include "base\ResourceViewHeaps.h"
#include "base\StaticBufferPool.h"
#include "base\DynamicBufferRing.h"
#include "base\Texture.h"

namespace CAULDRON_VK
{
    class FlipBookAnimation
    {
    public:
        void OnCreate(
            Device* pDevice,
            VkRenderPass renderPass,
            UploadHeap* pUploadHeap,
            ResourceViewHeaps *pResourceViewHeaps,
            DynamicBufferRing *pDynamicBufferRing,
            StaticBufferPool *pStaticBufferPool,
            VkSampleCountFlagBits sampleDescCount,
            UINT numRows, UINT numColms, std::string flipBookAnimationTexture, XMMATRIX worldMatrix);
        void OnDestroy();
        void Draw(VkCommandBuffer cmd_buf, float time, XMVECTOR camPos, XMMATRIX viewProjMat);

    private:
        Device * m_pDevice;
        VkRenderPass m_renderPass;

        VkIndexType m_indexType;
        UINT m_NumIndices;
        VkDescriptorBufferInfo m_IBV;
        VkDescriptorBufferInfo m_VBV;

        VkPipeline m_pipeline;
        VkPipelineLayout m_pipelineLayout;

        VkDescriptorSet m_descriptorSet;
        VkDescriptorSetLayout m_descriptorSetLayout;

        VkSampler m_sampler;
        VkSampleCountFlagBits m_sampleDescCount;
        Texture m_FlipBookTexture;
        VkImageView m_FlipBookTextureSRV;

        StaticBufferPool *m_pStaticBufferPool;
        DynamicBufferRing *m_pDynamicBufferRing;
        ResourceViewHeaps *m_pResourceViewHeaps;

        UINT m_numRows;
        UINT m_numColms;

        XMMATRIX m_worldMatrix;

        struct FlipBookAnimationCBuffer {
            UINT row;
            UINT cols;
            float time;
            float angle;
            XMVECTOR camPos;
            XMMATRIX worldMat;
            XMMATRIX viewProjMat;
        };
    };
}