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
#include "Misc/ColorConversion.h"
#include "ExposureMultiplierCS.h"

namespace CAULDRON_DX12
{
    void ExposureMultiplierCS::OnCreate(Device* pDevice, ResourceViewHeaps *pResourceViewHeaps, DynamicBufferRing *pDynamicBufferRing)
    {
        m_pDynamicBufferRing = pDynamicBufferRing;

        m_ExposureMultiplier.OnCreate(pDevice, pResourceViewHeaps, "ExposureMultiplierCS.hlsl", "main",  1, 0, 8, 8, 1);
    }

    void ExposureMultiplierCS::OnDestroy()
    {
        m_ExposureMultiplier.OnDestroy();
    }

    void ExposureMultiplierCS::Draw(ID3D12GraphicsCommandList* pCommandList, CBV_SRV_UAV *pHDRSRV, float exposure, int width, int height)
    {
        UserMarker marker(pCommandList, "ExposureMultiplierCS");

        D3D12_GPU_VIRTUAL_ADDRESS cbExposureMultiplierHandle;
        ExposureMultiplierConsts *pExposureMultiplierConsts;
        m_pDynamicBufferRing->AllocConstantBuffer(sizeof(ExposureMultiplierConsts), (void **)&pExposureMultiplierConsts, &cbExposureMultiplierHandle);
        pExposureMultiplierConsts->m_exposure = exposure;

        m_ExposureMultiplier.Draw(pCommandList, cbExposureMultiplierHandle, pHDRSRV, NULL, (width + 7) / 8, (height + 7) / 8, 1);
    }
}