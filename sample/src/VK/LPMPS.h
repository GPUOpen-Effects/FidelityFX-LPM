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
#pragma once

#include "PostProc/PostProcPS.h"
#include "Base/ResourceViewHeaps.h"
#include "Misc/ColorConversion.h"

namespace CAULDRON_VK
{
    class LPMPS
    {
    public:
        void OnCreate(Device* pDevice, VkRenderPass renderPass, ResourceViewHeaps *pResourceViewHeaps, StaticBufferPool  *pStaticBufferPool, DynamicBufferRing *pDynamicBufferRing);
        void OnDestroy();

        void UpdatePipelines(VkRenderPass renderPass, DisplayModes displayMode, ColorSpace colorSpace);

        void Draw(VkCommandBuffer cmd_buf, VkImageView HDRSRV);

    private:
        // LPM Only
        void SetLPMConfig(bool con, bool soft, bool con2, bool clip, bool scaleOnly);
        void SetLPMColors(
            float xyRedW[2], float xyGreenW[2], float xyBlueW[2], float xyWhiteW[2],
            float xyRedO[2], float xyGreenO[2], float xyBlueO[2], float xyWhiteO[2],
            float xyRedC[2], float xyGreenC[2], float xyBlueC[2], float xyWhiteC[2],
            float scaleC
        );

        bool m_shoulder; // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
        bool m_con; // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
        bool m_soft; // Use soft gamut mapping.
        bool m_con2; // Use last RGB conversion matrix.
        bool m_clip; // Use clipping in last conversion matrix.
        bool m_scaleOnly; // Scale only for last conversion matrix (used for 709 HDR to scRGB).
        float m_xyRedW[2]; float m_xyGreenW[2]; float m_xyBlueW[2]; float m_xyWhiteW[2]; // Chroma coordinates for working color space.
        float m_xyRedO[2]; float m_xyGreenO[2]; float m_xyBlueO[2]; float m_xyWhiteO[2]; // For the output color space.
        float m_xyRedC[2]; float m_xyGreenC[2]; float m_xyBlueC[2]; float m_xyWhiteC[2]; float m_scaleC; // For the output container color space (if con2).
        float m_softGap; // Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.
        float m_hdrMax; // Maximum input value.
        float m_exposure; // Number of stops between 'hdrMax' and 18% mid-level on input.
        float m_contrast; // Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
        float m_shoulderContrast; // Shoulder shaping, 1.0 = no change (fast path).
        float m_saturation[3]; // A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
        float m_crosstalk[3]; // One channel must be 1.0, the rest can be <= 1.0 but not zero.
        // LPM Only

        Device* m_pDevice;
        ResourceViewHeaps *m_pResourceViewHeaps;

        PostProcPS m_lpm;
        DynamicBufferRing *m_pDynamicBufferRing = NULL;

        VkSampler m_sampler;

        uint32_t              m_descriptorIndex;
        static const uint32_t s_descriptorBuffers = 10;

        VkDescriptorSet       m_descriptorSet[s_descriptorBuffers];
        VkDescriptorSetLayout m_descriptorSetLayout;

        DisplayModes m_displayMode;
        XMMATRIX m_inputToOutputMatrix;

        struct LPMConsts {
            UINT shoulder;
            UINT con;
            UINT soft;
            UINT con2;
            UINT clip;
            UINT scaleOnly;
            UINT displayMode;
            UINT pad;
            XMMATRIX inputToOutputMatrix;
            uint32_t ctl[24 * 4];
        };
    };
}
