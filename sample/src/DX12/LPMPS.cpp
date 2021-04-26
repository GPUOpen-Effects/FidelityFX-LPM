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
#include "Base/FreesyncHDR.h"
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

        m_lpm.OnCreate(pDevice, "LPMPS.hlsl", pResourceViewHeaps, pStaticBufferPool, 1, 1, &SamplerDesc, outFormat, 1, NULL, NULL, 1);
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

    void LPMPS::UpdatePipeline(DXGI_FORMAT outFormat, DisplayMode displayMode, ColorSpace colorSpace,
        bool shoulder,
        float softGap,
        float hdrMax,
        float lpmExposure,
        float contrast,
        float shoulderContrast,
        float saturation[3],
        float crosstalk[3])
    {
        m_lpm.UpdatePipeline(outFormat);

        m_displayMode = displayMode;

        SetupGamutMapperMatrices(
            ColorSpace_REC709,
            colorSpace,
            &m_inputToOutputMatrix
        );

        varAF2(fs2R);
        varAF2(fs2G);
        varAF2(fs2B);
        varAF2(fs2W);
        varAF2(displayMinMaxLuminance);
        if (displayMode != DISPLAYMODE_SDR)
        {
            const DXGI_OUTPUT_DESC1 *displayInfo = GetDisplayInfo();

            // Only used in fs2 modes
            fs2R[0] = displayInfo->RedPrimary[0];
            fs2R[1] = displayInfo->RedPrimary[1];
            fs2G[0] = displayInfo->GreenPrimary[0];
            fs2G[1] = displayInfo->GreenPrimary[1];
            fs2B[0] = displayInfo->BluePrimary[0];
            fs2B[1] = displayInfo->BluePrimary[1];
            fs2W[0] = displayInfo->WhitePoint[0];
            fs2W[1] = displayInfo->WhitePoint[1];
            // Only used in fs2 modes

            displayMinMaxLuminance[0] = displayInfo->MinLuminance;
            displayMinMaxLuminance[1] = displayInfo->MaxLuminance;
        }
        m_shoulder = shoulder;
        m_softGap = softGap;
        m_hdrMax = hdrMax;
        m_exposure = lpmExposure;
        m_contrast = contrast;
        m_shoulderContrast = shoulderContrast;
        m_saturation[0] = saturation[0]; m_saturation[1] = saturation[1]; m_saturation[2] = saturation[2];
        m_crosstalk[0] = crosstalk[0]; m_crosstalk[1] = crosstalk[1]; m_crosstalk[2] = crosstalk[2];

        switch (colorSpace)
        {
        case ColorSpace_REC709:
        {
            switch (displayMode)
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

    void LPMPS::Draw(ID3D12GraphicsCommandList* pCommandList, CBV_SRV_UAV *pSRV)
    {
        UserMarker marker(pCommandList, "LPM Tonemapper");

        D3D12_GPU_VIRTUAL_ADDRESS cbLPMHandle;
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

        m_lpm.Draw(pCommandList, 1, pSRV, cbLPMHandle);
    }
}