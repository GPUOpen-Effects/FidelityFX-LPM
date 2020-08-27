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

#include "stdafx.h"
#include "base\ShaderCompilerHelper.h"
#include "FlipBookAnimation.h"

namespace CAULDRON_DX12
{
    void FlipBookAnimation::OnCreate(
        Device* pDevice,
        UploadHeap* pUploadHeap,
        ResourceViewHeaps *pResourceViewHeaps,
        DynamicBufferRing *pDynamicBufferRing,
        StaticBufferPool *pStaticBufferPool,
        UINT sampleDescCount,
        DXGI_FORMAT outFormat,
        UINT numRows, UINT numColms, std::string flipBookAnimationTexture, XMMATRIX worldMatrix)
    {
        m_pDevice = pDevice;
        m_pResourceViewHeaps = pResourceViewHeaps;
        m_pDynamicBufferRing = pDynamicBufferRing;
        m_pStaticBufferPool = pStaticBufferPool;
        m_sampleDescCount = sampleDescCount;
        m_numRows = numRows;
        m_numColms = numColms;
        m_worldMatrix = worldMatrix;
        m_NumIndices = 6;

        short indices[] =
        {
            0, 1, 2,
            2, 3, 0
        };
        m_pStaticBufferPool->AllocIndexBuffer(m_NumIndices, sizeof(short), indices, &m_IBV);

        float vertices[] =
        {
            // pos                      // uv
            -1.0f,  1.0f, 0.0f, 1.0f,   0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f,   1.0f, 0.0f,
             1.0f, -1.0f, 0.0f, 1.0f,   1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 1.0f,   0.0f, 1.0f
        };
        m_pStaticBufferPool->AllocVertexBuffer(4, 6 * sizeof(float), vertices, &m_VBV);

        D3D12_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  4 * sizeof(float), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        m_pFlipBookTexture.InitFromFile(m_pDevice, pUploadHeap, flipBookAnimationTexture.c_str(), true);
        pUploadHeap->FlushAndFinish();
        m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_FlipBookTextureSRV);
        m_pFlipBookTexture.CreateSRV(0, &m_FlipBookTextureSRV);

        // Compile shaders
        //
        D3D12_SHADER_BYTECODE shaderVert, shaderPixel;
        {
            DefineList defines;
            CompileShaderFromFile("FlipBookAnimationVS.hlsl", &defines, "mainVS", "-T vs_6_0", &shaderVert);
            CompileShaderFromFile("FlipBookAnimationPS.hlsl", &defines, "mainPS", "-T ps_6_0", &shaderPixel);
        }

        // Create root signature
        //
        {
            int numParams = 0;

            CD3DX12_DESCRIPTOR_RANGE DescRange[3];
            DescRange[numParams++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);             // b0 <- per frame
            DescRange[numParams++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);				// t0 <- per material
            DescRange[numParams++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);	        // s0 <- samplers


            CD3DX12_ROOT_PARAMETER RTSlot[3];
            RTSlot[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
            for (int i = 1; i < numParams; i++)
            RTSlot[i].InitAsDescriptorTable(1, &DescRange[i], D3D12_SHADER_VISIBILITY_PIXEL);

            // the root signature contains 3 slots to be used
            CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = CD3DX12_ROOT_SIGNATURE_DESC();
            descRootSignature.NumParameters = numParams;
            descRootSignature.pParameters = RTSlot;
            descRootSignature.NumStaticSamplers = 0;
            descRootSignature.pStaticSamplers = NULL;

            // deny uneccessary access to certain pipeline stages   
            descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ID3DBlob *pOutBlob, *pErrorBlob = NULL;
            ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob));
            ThrowIfFailed(
                m_pDevice->GetDevice()->CreateRootSignature(0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature))
            );
            m_RootSignature->SetName(L"FlipBookAnimation");

            pOutBlob->Release();
            if (pErrorBlob)
                pErrorBlob->Release();
        }

        D3D12_RENDER_TARGET_BLEND_DESC blendingBlend = D3D12_RENDER_TARGET_BLEND_DESC
        {
            TRUE,
            FALSE,
            D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        // Create a PSO description
        //
        D3D12_GRAPHICS_PIPELINE_STATE_DESC descPso = {};
        descPso.InputLayout = { layout, 2 };
        descPso.pRootSignature = m_RootSignature;
        descPso.VS = shaderVert;
        descPso.PS = shaderPixel;
        descPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        descPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        descPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        descPso.BlendState.RenderTarget[0] = blendingBlend;
        descPso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        descPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        descPso.SampleMask = UINT_MAX;
        descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        descPso.NumRenderTargets = 1;
        descPso.RTVFormats[0] = outFormat;
        descPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        descPso.SampleDesc.Count = m_sampleDescCount;
        descPso.NodeMask = 0;
        ThrowIfFailed(
            m_pDevice->GetDevice()->CreateGraphicsPipelineState(&descPso, IID_PPV_ARGS(&m_PipelineRender))
        );

        // create samplers if not initialized (this should happen once)
        m_pResourceViewHeaps->AllocSamplerDescriptor(1, &m_sampler);

        D3D12_SAMPLER_DESC SamplerDesc;
        ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
        SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        SamplerDesc.BorderColor[0] = 0.0f;
        SamplerDesc.BorderColor[1] = 0.0f;
        SamplerDesc.BorderColor[2] = 0.0f;
        SamplerDesc.BorderColor[3] = 0.0f;
        SamplerDesc.MinLOD = 0.0f;
        SamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        SamplerDesc.MipLODBias = 0;
        SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        SamplerDesc.MaxAnisotropy = 1;
        m_pDevice->GetDevice()->CreateSampler(&SamplerDesc, m_sampler.GetCPU(0));
    }

    void FlipBookAnimation::OnDestroy()
    {
        m_PipelineRender->Release();

        m_RootSignature->Release();

        m_pFlipBookTexture.OnDestroy();
    }

    void FlipBookAnimation::Draw(ID3D12GraphicsCommandList* pCommandList, float time, XMVECTOR camPos, XMMATRIX viewProjMat)
    {
        D3D12_GPU_VIRTUAL_ADDRESS cbFlipBookAnimation;
        FlipBookAnimationCBuffer *pFlipBookAnimation;
        m_pDynamicBufferRing->AllocConstantBuffer(sizeof(FlipBookAnimationCBuffer), (void **)&pFlipBookAnimation, &cbFlipBookAnimation);
        pFlipBookAnimation->row = m_numRows;
        pFlipBookAnimation->cols = m_numColms;
        pFlipBookAnimation->time = time;

        XMFLOAT4 dist;
        XMStoreFloat4(&dist, camPos - m_worldMatrix.r[3]);
        float angle = atan2(dist.x, dist.z);

        pFlipBookAnimation->angle = angle;
        pFlipBookAnimation->camPos = camPos;
        pFlipBookAnimation->worldMat = m_worldMatrix;
        pFlipBookAnimation->viewProjMat = viewProjMat;

        ID3D12DescriptorHeap *pDescriptorHeaps[] = { m_pResourceViewHeaps->GetCBV_SRV_UAVHeap(), m_pResourceViewHeaps->GetSamplerHeap() };
        pCommandList->SetDescriptorHeaps(2, pDescriptorHeaps);
        pCommandList->SetGraphicsRootSignature(m_RootSignature);
        pCommandList->SetGraphicsRootConstantBufferView(0, cbFlipBookAnimation);
        pCommandList->SetGraphicsRootDescriptorTable(1, m_FlipBookTextureSRV.GetGPU());
        pCommandList->SetGraphicsRootDescriptorTable(2, m_sampler.GetGPU());

        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCommandList->IASetIndexBuffer(&m_IBV);
        pCommandList->IASetVertexBuffers(0, 1, &m_VBV);
        pCommandList->SetPipelineState(m_PipelineRender);

        pCommandList->DrawIndexedInstanced(m_NumIndices, 1, 0, 0, 0);
    }
}