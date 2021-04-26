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

#include "PostProc\PostProcPS.h"

namespace CAULDRON_VK
{
    class TestImages
    {
    public:
        void OnCreate(Device* pDevice, VkRenderPass renderPass, UploadHeap* pUploadHeap, ResourceViewHeaps *pResourceViewHeaps, StaticBufferPool  *pStaticBufferPool, DynamicBufferRing *pDynamicBufferRing);
        void OnDestroy();
        void Draw(VkCommandBuffer cmd_buf, int testPattern);

    private:
        struct TestImagesData {
            VkDescriptorSet m_descriptorSet;
            VkDescriptorSetLayout m_descriptorSetLayout;
            VkImageView m_testImageTextureSRV;
            Texture m_testImageTexture;
        };

        Device* m_pDevice;
        ResourceViewHeaps *m_pResourceViewHeaps;
        DynamicBufferRing *m_pDynamicBufferRing = NULL;
        VkSampler m_sampler;
        TestImagesData m_testImagesData[3];
        PostProcPS m_testImagePS;

        struct TestImagesConsts
        {
            int testPattern;
        };
    };
}