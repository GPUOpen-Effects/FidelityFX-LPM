// AMD AMDUtils code
// 
// Copyright(c) 2018 Advanced Micro Devices, Inc.All rights reserved.
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

#include "stdafx.h"
#include "Base/DynamicBufferRing.h"
#include "Base/StaticBufferPool.h"
#include "Base/ExtDebugMarkers.h"
#include "Base/UploadHeap.h"
#include "Base/Helper.h"
#include "ExposureMultiplierCS.h"

namespace CAULDRON_VK
{
    void ExposureMultiplierCS::OnCreate(Device* pDevice, ResourceViewHeaps *pResourceViewHeaps, DynamicBufferRing *pDynamicBufferRing)
    {
        m_pDevice = pDevice;
        m_pDynamicBufferRing = pDynamicBufferRing;
        m_pResourceViewHeaps = pResourceViewHeaps;

        std::vector<VkDescriptorSetLayoutBinding> layoutBindings(2);
        layoutBindings[0].binding = 0;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        layoutBindings[0].descriptorCount = 1;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[0].pImmutableSamplers = NULL;

        layoutBindings[1].binding = 1;
        layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layoutBindings[1].descriptorCount = 1;
        layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[1].pImmutableSamplers = NULL;

        m_pResourceViewHeaps->CreateDescriptorSetLayout(&layoutBindings, &m_descriptorSetLayout);

        m_ExposureMultiplier.OnCreate(pDevice, "ExposureMultiplierCS.glsl", "main", "", m_descriptorSetLayout, 8, 8, 1, NULL);

        m_descriptorIndex = 0;
        for (int i = 0; i < s_descriptorBuffers; i++)
            m_pResourceViewHeaps->AllocDescriptor(m_descriptorSetLayout, &m_descriptorSet[i]);
    }

    void ExposureMultiplierCS::OnDestroy()
    {
        m_ExposureMultiplier.OnDestroy();

        for (int i = 0; i < s_descriptorBuffers; i++)
            m_pResourceViewHeaps->FreeDescriptor(m_descriptorSet[i]);

        vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_descriptorSetLayout, NULL);
    }

    void ExposureMultiplierCS::Draw(VkCommandBuffer cmd_buf, VkImageView HDRSRV, float exposure, int width, int height)
    {
        SetPerfMarkerBegin(cmd_buf, "ExposureMultiplierCS");

        VkDescriptorBufferInfo cbExposureMultiplierHandle;
        ExposureMultiplierConsts *pExposureMultiplierConsts;
        m_pDynamicBufferRing->AllocConstantBuffer(sizeof(ExposureMultiplierConsts), (void **)&pExposureMultiplierConsts, &cbExposureMultiplierHandle);
        pExposureMultiplierConsts->m_exposure = exposure;

        // We'll be modifying the descriptor set(DS), to prevent writing on a DS that is in use we 
        // need to do some basic buffering. Just to keep it safe and simple we'll have 10 buffers.
        VkDescriptorSet descriptorSet = m_descriptorSet[m_descriptorIndex];
        m_descriptorIndex = (m_descriptorIndex + 1) % s_descriptorBuffers;

        SetDescriptorSet(m_pDevice->GetDevice(), 1, HDRSRV, descriptorSet);
        m_pDynamicBufferRing->SetDescriptorSet(0, sizeof(ExposureMultiplierConsts), descriptorSet);

        m_ExposureMultiplier.Draw(cmd_buf, cbExposureMultiplierHandle, descriptorSet, (width + 7) / 8, (height + 7) / 8, 1);
        
        SetPerfMarkerEnd(cmd_buf);
    }
}