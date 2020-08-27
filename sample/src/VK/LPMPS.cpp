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
#include "Base/ShaderCompiler.h"
#include "Base/ExtDebugMarkers.h"
#include "Base/UploadHeap.h"
#include "Base/FreeSyncHDR.h"
#include "Base/Helper.h"
#include "LPMPS.h"

// LPM
#include <stdint.h>
#define A_CPU 1
#include "../../../ffx-lpm/ffx_a.h"
A_STATIC AF1 fs2S;
A_STATIC AF1 hdr10S;
A_STATIC AU1 ctl[24 * 4];

A_STATIC void LpmSetupOut(AU1 i, inAU4 v)
{
    for (int j = 0; j < 4; ++j) { ctl[i * 4 + j] = v[j]; }
}
#include "../../../ffx-lpm/ffx_lpm.h"

namespace CAULDRON_VK
{
    void LPMPS::OnCreate(Device* pDevice, VkRenderPass renderPass, ResourceViewHeaps *pResourceViewHeaps, StaticBufferPool  *pStaticBufferPool, DynamicBufferRing *pDynamicBufferRing)
    {
        m_pDevice = pDevice;
        m_pDynamicBufferRing = pDynamicBufferRing;
        m_pResourceViewHeaps = pResourceViewHeaps;

        {
            VkSamplerCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            info.magFilter = VK_FILTER_LINEAR;
            info.minFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            info.minLod = -1000;
            info.maxLod = 1000;
            info.maxAnisotropy = 1.0f;
            VkResult res = vkCreateSampler(m_pDevice->GetDevice(), &info, NULL, &m_sampler);
            assert(res == VK_SUCCESS);
        }

        std::vector<VkDescriptorSetLayoutBinding> layoutBindings(2);
        layoutBindings[0].binding = 0;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        layoutBindings[0].descriptorCount = 1;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        layoutBindings[0].pImmutableSamplers = NULL;

        layoutBindings[1].binding = 1;
        layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutBindings[1].descriptorCount = 1;
        layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layoutBindings[1].pImmutableSamplers = NULL;

        m_pResourceViewHeaps->CreateDescriptorSetLayout(&layoutBindings, &m_descriptorSetLayout);

        m_lpm.OnCreate(m_pDevice, renderPass, "LPMPS.glsl", "main", "", pStaticBufferPool, pDynamicBufferRing, m_descriptorSetLayout, NULL, VK_SAMPLE_COUNT_1_BIT);

        m_descriptorIndex = 0;
        for (int i = 0; i < s_descriptorBuffers; i++)
            m_pResourceViewHeaps->AllocDescriptor(m_descriptorSetLayout, &m_descriptorSet[i]);
    }

    void LPMPS::OnDestroy()
    {
        m_lpm.OnDestroy();

        for (int i = 0; i < s_descriptorBuffers; i++)
            m_pResourceViewHeaps->FreeDescriptor(m_descriptorSet[i]);

        vkDestroySampler(m_pDevice->GetDevice(), m_sampler, nullptr);

        vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_descriptorSetLayout, NULL);
    }

    void LPMPS::SetLPMConfig(bool con, bool soft, bool con2, bool clip, bool scaleOnly)
    {
        m_con = con;
        m_soft = soft;
        m_con2 = con2;
        m_clip = clip;
        m_scaleOnly = scaleOnly;
    }

    void LPMPS::SetLPMColors(
        float xyRedW[2], float xyGreenW[2], float xyBlueW[2], float xyWhiteW[2],
        float xyRedO[2], float xyGreenO[2], float xyBlueO[2], float xyWhiteO[2],
        float xyRedC[2], float xyGreenC[2], float xyBlueC[2], float xyWhiteC[2],
        float scaleC
    )
    {
        m_xyRedW[0] = xyRedW[0]; m_xyRedW[1] = xyRedW[1];
        m_xyGreenW[0] = xyGreenW[0]; m_xyGreenW[1] = xyGreenW[1];
        m_xyBlueW[0] = xyBlueW[0]; m_xyBlueW[1] = xyBlueW[1];
        m_xyWhiteW[0] = xyWhiteW[0]; m_xyWhiteW[1] = xyWhiteW[1];

        m_xyRedO[0] = xyRedO[0]; m_xyRedO[1] = xyRedO[1];
        m_xyGreenO[0] = xyGreenO[0]; m_xyGreenO[1] = xyGreenO[1];
        m_xyBlueO[0] = xyBlueO[0]; m_xyBlueO[1] = xyBlueO[1];
        m_xyWhiteO[0] = xyWhiteO[0]; m_xyWhiteO[1] = xyWhiteO[1];

        m_xyRedC[0] = xyRedC[0]; m_xyRedC[1] = xyRedC[1];
        m_xyGreenC[0] = xyGreenC[0]; m_xyGreenC[1] = xyGreenC[1];
        m_xyBlueC[0] = xyBlueC[0]; m_xyBlueC[1] = xyBlueC[1];
        m_xyWhiteC[0] = xyWhiteC[0]; m_xyWhiteC[1] = xyWhiteC[1];

        m_scaleC = scaleC;
    }
    void LPMPS::UpdatePipelines(VkRenderPass renderPass, DisplayModes displayMode, ColorSpace colorSpace)
    {
        m_lpm.UpdatePipeline(renderPass, NULL, VK_SAMPLE_COUNT_1_BIT);

        m_displayMode = displayMode;

        SetupGamutMapperMatrices(
            ColorSpace_REC709,
            colorSpace,
            &m_inputToOutputMatrix
        );

        // LPM Only setup
        varAF2(fs2R);
        varAF2(fs2G);
        varAF2(fs2B);
        varAF2(fs2W);
        varAF2(displayMinMaxLuminance);
        if (m_displayMode != DISPLAYMODE_SDR)
        {
            const VkHdrMetadataEXT *pHDRMetatData = fsHdrGetDisplayInfo();

            // Only used in fs2 modes
            fs2R[0] = pHDRMetatData->displayPrimaryRed.x;
            fs2R[1] = pHDRMetatData->displayPrimaryRed.y;
            fs2G[0] = pHDRMetatData->displayPrimaryGreen.x;
            fs2G[1] = pHDRMetatData->displayPrimaryGreen.y;
            fs2B[0] = pHDRMetatData->displayPrimaryBlue.x;
            fs2B[1] = pHDRMetatData->displayPrimaryBlue.y;
            fs2W[0] = pHDRMetatData->whitePoint.x;
            fs2W[1] = pHDRMetatData->whitePoint.y;
            // Only used in fs2 modes

            displayMinMaxLuminance[0] = pHDRMetatData->minLuminance;
            displayMinMaxLuminance[1] = pHDRMetatData->maxLuminance;
        }
        m_shoulder = 0;
        m_softGap = 1.0f / 32.0f;
        m_hdrMax = 256.0f; // Controls brightness. Need to tune according to display mode
        m_exposure = 8.0f; // Controls brightness. Need to tune according to display mode
        m_contrast = 0.3f;
        m_shoulderContrast = 1.0f;
        m_saturation[0] = 0.0f; m_saturation[1] = 0.0f; m_saturation[2] = 0.0f;
        m_crosstalk[0] = 1.0f; m_crosstalk[1] = 1.0f / 2.0f; m_crosstalk[2] = 1.0f / 32.0f;

        switch (colorSpace)
        {
        case ColorSpace_REC709:
        {
            switch (m_displayMode)
            {
            case DISPLAYMODE_SDR:
                SetLPMConfig(LPM_CONFIG_709_709);
                SetLPMColors(LPM_COLORS_709_709);
                break;

            case DISPLAYMODE_FSHDR_Gamma22:
                SetLPMConfig(LPM_CONFIG_FS2RAW_709);
                SetLPMColors(LPM_COLORS_FS2RAW_709);
                break;

            case DISPLAYMODE_FSHDR_SCRGB:
                fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_FS2SCRGB_709);
                SetLPMColors(LPM_COLORS_FS2SCRGB_709);
                break;

            case DISPLAYMODE_HDR10_2084:
                hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_HDR10RAW_709);
                SetLPMColors(LPM_COLORS_HDR10RAW_709);
                break;

            case DISPLAYMODE_HDR10_SCRGB:
                hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_HDR10SCRGB_709);
                SetLPMColors(LPM_COLORS_HDR10SCRGB_709);
                break;

            default:
                break;
            }
            break;
        }

        case ColorSpace_P3:
        {
            switch (displayMode)
            {
            case DISPLAYMODE_SDR:
                SetLPMConfig(LPM_CONFIG_709_P3);
                SetLPMColors(LPM_COLORS_709_P3);
                break;

            case DISPLAYMODE_FSHDR_Gamma22:
                SetLPMConfig(LPM_CONFIG_FS2RAW_P3);
                SetLPMColors(LPM_COLORS_FS2RAW_P3);
                break;

            case DISPLAYMODE_FSHDR_SCRGB:
                fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_FS2SCRGB_P3);
                SetLPMColors(LPM_COLORS_FS2SCRGB_P3);
                break;

            case DISPLAYMODE_HDR10_2084:
                hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_HDR10RAW_P3);
                SetLPMColors(LPM_COLORS_HDR10RAW_P3);
                break;

            case DISPLAYMODE_HDR10_SCRGB:
                hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_HDR10SCRGB_P3);
                SetLPMColors(LPM_COLORS_HDR10SCRGB_P3);
                break;

            default:
                break;
            }
            break;
        }

        case ColorSpace_REC2020:
        {
            switch (displayMode)
            {
            case DISPLAYMODE_SDR:
                SetLPMConfig(LPM_CONFIG_709_2020);
                SetLPMColors(LPM_COLORS_709_2020);
                break;

            case DISPLAYMODE_FSHDR_Gamma22:
                SetLPMConfig(LPM_CONFIG_FS2RAW_2020);
                SetLPMColors(LPM_COLORS_FS2RAW_2020);
                break;

            case DISPLAYMODE_FSHDR_SCRGB:
                fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_FS2SCRGB_2020);
                SetLPMColors(LPM_COLORS_FS2SCRGB_2020);
                break;

            case DISPLAYMODE_HDR10_2084:
                hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_HDR10RAW_2020);
                SetLPMColors(LPM_COLORS_HDR10RAW_2020);
                break;

            case DISPLAYMODE_HDR10_SCRGB:
                hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
                SetLPMConfig(LPM_CONFIG_HDR10SCRGB_2020);
                SetLPMColors(LPM_COLORS_HDR10SCRGB_2020);
                break;

            default:
                break;
            }
            break;
        }

        default:
            break;
        }

        LpmSetup(m_shoulder, m_con, m_soft, m_con2, m_clip, m_scaleOnly,
            m_xyRedW, m_xyGreenW, m_xyBlueW, m_xyWhiteW,
            m_xyRedO, m_xyGreenO, m_xyBlueO, m_xyWhiteO,
            m_xyRedC, m_xyGreenC, m_xyBlueC, m_xyWhiteC,
            m_scaleC,
            m_softGap, m_hdrMax, m_exposure, m_contrast, m_shoulderContrast,
            m_saturation, m_crosstalk);
    }

    void LPMPS::Draw(VkCommandBuffer cmd_buf, VkImageView HDRSRV)
    {
        SetPerfMarkerBegin(cmd_buf, "LPM Tonemapper");

        VkDescriptorBufferInfo cbLPMHandle;
        LPMConsts *pLPM;
        m_pDynamicBufferRing->AllocConstantBuffer(sizeof(LPMConsts), (void **)&pLPM, &cbLPMHandle);
        pLPM->shoulder = m_shoulder;
        pLPM->con = m_con;
        pLPM->soft = m_soft;
        pLPM->con2 = m_con2;
        pLPM->clip = m_clip;
        pLPM->scaleOnly = m_scaleOnly;
        pLPM->displayMode = m_displayMode;
        pLPM->pad = 0;
        pLPM->inputToOutputMatrix = m_inputToOutputMatrix;
        for (int i = 0; i < 4 * 24; ++i)
        {
            pLPM->ctl[i] = ctl[i];
        }

        // We'll be modifying the descriptor set(DS), to prevent writing on a DS that is in use we 
        // need to do some basic buffering. Just to keep it safe and simple we'll have 10 buffers.
        VkDescriptorSet descriptorSet = m_descriptorSet[m_descriptorIndex];
        m_descriptorIndex = (m_descriptorIndex + 1) % s_descriptorBuffers;

        // modify Descriptor set
        SetDescriptorSet(m_pDevice->GetDevice(), 1, HDRSRV, &m_sampler, descriptorSet);
        m_pDynamicBufferRing->SetDescriptorSet(0, sizeof(LPMConsts), descriptorSet);

        // Draw!
        m_lpm.Draw(cmd_buf, cbLPMHandle, descriptorSet);

        SetPerfMarkerEnd(cmd_buf);
    }
}