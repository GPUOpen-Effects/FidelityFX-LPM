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
#include "Base/UploadHeap.h"
#include "Base/Freesync2.h"
#include "Misc/ColorConversion.h"
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

namespace CAULDRON_DX12
{
    void LPMPS::OnCreate(Device* pDevice, ResourceViewHeaps *pResourceViewHeaps, DynamicBufferRing *pDynamicBufferRing, StaticBufferPool  *pStaticBufferPool, DXGI_FORMAT outFormat)
    {
        m_pDynamicBufferRing = pDynamicBufferRing;

        D3D12_STATIC_SAMPLER_DESC SamplerDesc = {};
        SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        SamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        SamplerDesc.MinLOD = 0.0f;
        SamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        SamplerDesc.MipLODBias = 0;
        SamplerDesc.MaxAnisotropy = 1;
        SamplerDesc.ShaderRegister = 0;
        SamplerDesc.RegisterSpace = 0;
        SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        m_lpm.OnCreate(pDevice, "LPMPS.hlsl", pResourceViewHeaps, pStaticBufferPool, 1, 1, &SamplerDesc, outFormat, 1, NULL, NULL, 1, "vs_6_0", "ps_6_0");
    }

    void LPMPS::OnDestroy()
    {
        m_lpm.OnDestroy();
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

    void LPMPS::UpdatePipeline(DXGI_FORMAT outFormat, DisplayModes displayMode, ColorSpace colorSpace)
    {
        m_lpm.UpdatePipeline(outFormat);

        m_displayMode = displayMode;
        m_colorSpace = colorSpace;

        SetupGamutMapperMatrices(
            ColorSpace_REC709,
            m_colorSpace,
            &m_inputToOutputMatrix
        );

        varAF2(fs2R);
        varAF2(fs2G);
        varAF2(fs2B);
        varAF2(fs2W);
        varAF2(displayMinMaxLuminance);
        if (displayMode != DISPLAYMODE_SDR)
        {
            const AGSDisplayInfo *agsDisplayInfo = fs2GetDisplayInfo();

            // Only used in fs2 modes
            fs2R[0] = (float) agsDisplayInfo->chromaticityRedX;
            fs2R[1] = (float) agsDisplayInfo->chromaticityRedY;
            fs2G[0] = (float) agsDisplayInfo->chromaticityGreenX;
            fs2G[1] = (float) agsDisplayInfo->chromaticityGreenY;
            fs2B[0] = (float) agsDisplayInfo->chromaticityBlueX;
            fs2B[1] = (float) agsDisplayInfo->chromaticityBlueY;
            fs2W[0] = (float) agsDisplayInfo->chromaticityWhitePointX;
            fs2W[1] = (float) agsDisplayInfo->chromaticityWhitePointY;
            // Only used in fs2 modes

            displayMinMaxLuminance[0] = (float) agsDisplayInfo->minLuminance;
            displayMinMaxLuminance[1] = (float) agsDisplayInfo->maxLuminance;
        }
        m_shoulder = 0;
        m_softGap = 1.0f / 32.0f;
        m_hdrMax = 256.0f; // Controls brightness. Need to tune according to display mode
        m_exposure = 8.0f; // Controls brightness. Need to tune according to display mode
        m_contrast = 0.3f;
        m_shoulderContrast = 1.0f;
        m_saturation[0] = 0.0f; m_saturation[1] = 0.0f; m_saturation[2] = 0.0f;
        m_crosstalk[0] = 1.0f; m_crosstalk[1] = 1.0f / 2.0f; m_crosstalk[2] = 1.0f / 32.0f;

        switch (m_colorSpace)
        {
        case ColorSpace_REC709:
        {
            switch (m_displayMode)
            {
            case DISPLAYMODE_SDR:
                SetLPMConfig(LPM_CONFIG_709_709);
                SetLPMColors(LPM_COLORS_709_709);
                break;

            case DISPLAYMODE_FS2_Gamma22:
                SetLPMConfig(LPM_CONFIG_FS2RAW_709);
                SetLPMColors(LPM_COLORS_FS2RAW_709);
                break;

            case DISPLAYMODE_FS2_SCRGB:
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

            case DISPLAYMODE_FS2_Gamma22:
                SetLPMConfig(LPM_CONFIG_FS2RAW_P3);
                SetLPMColors(LPM_COLORS_FS2RAW_P3);
                break;

            case DISPLAYMODE_FS2_SCRGB:
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

            case DISPLAYMODE_FS2_Gamma22:
                SetLPMConfig(LPM_CONFIG_FS2RAW_2020);
                SetLPMColors(LPM_COLORS_FS2RAW_2020);
                break;

            case DISPLAYMODE_FS2_SCRGB:
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

    void LPMPS::Draw(ID3D12GraphicsCommandList* pCommandList, CBV_SRV_UAV *pSRV)
    {
        UserMarker marker(pCommandList, "LPM Tonemapper");

        D3D12_GPU_VIRTUAL_ADDRESS cbLPMHandle;
        LPMConsts *pLPM;
        m_pDynamicBufferRing->AllocConstantBuffer(sizeof(LPMConsts), (void **)&pLPM, &cbLPMHandle);
        pLPM->inputToOutputMatrix = m_inputToOutputMatrix;
        pLPM->displayMode = m_displayMode;
        pLPM->colorSpaces = m_colorSpace;
        pLPM->shoulder = m_shoulder;
        for (int i = 0; i < 4 * 24; ++i)
        {
            pLPM->ctl[i] = ctl[i];
        }

        m_lpm.Draw(pCommandList, 1, pSRV, cbLPMHandle);
    }
}