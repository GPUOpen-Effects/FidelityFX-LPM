// AMD LPMSample sample code
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

#include "Renderer.h"
#include "UI.h"

#include <stdlib.h>

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void Renderer::OnCreate(Device* pDevice, SwapChain* pSwapChain, float fontSize)
{
    m_pDevice = pDevice;

    // Initialize helpers

    // Create all the heaps for the resources views
    const uint32_t cbvDescriptorCount = 4000;
    const uint32_t srvDescriptorCount = 8000;
    const uint32_t uavDescriptorCount = 10;
    const uint32_t dsvDescriptorCount = 10;
    const uint32_t rtvDescriptorCount = 60;
    const uint32_t samplerDescriptorCount = 20;
    m_resourceViewHeaps.OnCreate(pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, dsvDescriptorCount, rtvDescriptorCount, samplerDescriptorCount);

    // Create a commandlist ring for the Direct queue
    uint32_t commandListsPerBackBuffer = 8;
    m_CommandListRing.OnCreate(pDevice, backBufferCount, commandListsPerBackBuffer, pDevice->GetGraphicsQueue()->GetDesc());

    // Create a 'dynamic' constant buffer
    const uint32_t constantBuffersMemSize = 200 * 1024 * 1024;
    m_ConstantBufferRing.OnCreate(pDevice, backBufferCount, constantBuffersMemSize, &m_resourceViewHeaps);

    // Create a 'static' pool for vertices, indices and constant buffers
    const uint32_t staticGeometryMemSize = (5 * 128) * 1024 * 1024;
    m_VidMemBufferPool.OnCreate(pDevice, staticGeometryMemSize, true, "StaticGeom");

    // initialize the GPU time stamps module
    m_GPUTimer.OnCreate(pDevice, backBufferCount);

    // Quick helper to upload resources, it has it's own commandList and uses suballocation.
    // for 4K textures we'll need 100Megs
    const uint32_t uploadHeapMemSize = 1000 * 1024 * 1024;
    m_UploadHeap.OnCreate(pDevice, uploadHeapMemSize);    // initialize an upload heap (uses suballocation for faster results)

    // Create GBuffer and render passes
    //
    {
        m_GBuffer.OnCreate(
            pDevice,
            &m_resourceViewHeaps,
            {
                { GBUFFER_DEPTH, DXGI_FORMAT_D32_FLOAT},
                { GBUFFER_FORWARD, DXGI_FORMAT_R16G16B16A16_FLOAT},
            },
            1
        );

        GBufferFlags fullGBuffer = GBUFFER_DEPTH | GBUFFER_FORWARD;
        m_renderPassFullGBuffer.OnCreate(&m_GBuffer, fullGBuffer);
        m_renderPassJustDepthAndHdr.OnCreate(&m_GBuffer, GBUFFER_DEPTH | GBUFFER_FORWARD);
    }

    m_downSample.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_bloom.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_magnifierPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);

    // Create tonemapping pass
    m_toneMappingPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());
    m_toneMappingCS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing);
    m_colorConversionPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, DXGI_FORMAT_R16G16B16A16_FLOAT, fontSize);

    math::Matrix4 campfireWorldMatrix = math::Matrix4(math::Vector4(0.5f, 0.0f, 0.0f, 0.0f),
                                                      math::Vector4(0.0f, 0.5f, 0.0f, 0.0f),
                                                      math::Vector4(0.0f, 0.0f, 0.5f, 0.0f),
                                                      math::Vector4(-47.3f, -26.5f, -40.425f, 1.0f));
    m_CampfireAnimation.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, 12, 12, "..\\media\\FlipBookAnimationTextures\\cf_loop_comp_v1_mosaic.png", campfireWorldMatrix);

    m_testImages.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing, DXGI_FORMAT_R16G16B16A16_FLOAT);

    // Create tonemapping pass
    m_exposureMultiplierCS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing);
    m_lpmPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());

    // Make sure upload heap has finished uploading before continuing
    m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    m_UploadHeap.FlushAndFinish();
}

//--------------------------------------------------------------------------------------
//
// OnDestroy 
//
//--------------------------------------------------------------------------------------
void Renderer::OnDestroy()
{
    m_lpmPS.OnDestroy();
    m_exposureMultiplierCS.OnDestroy();
    m_testImages.OnDestroy();
    m_CampfireAnimation.OnDestroy();

    m_asyncPool.Flush();

    m_ImGUI.OnDestroy();
    m_colorConversionPS.OnDestroy();
    m_toneMappingCS.OnDestroy();
    m_toneMappingPS.OnDestroy();
    m_bloom.OnDestroy();
    m_downSample.OnDestroy();
    m_magnifierPS.OnDestroy();
    m_GBuffer.OnDestroy();

    m_UploadHeap.OnDestroy();
    m_GPUTimer.OnDestroy();
    m_VidMemBufferPool.OnDestroy();
    m_ConstantBufferRing.OnDestroy();
    m_resourceViewHeaps.OnDestroy();
    m_CommandListRing.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnCreateWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height)
{
    m_Width = Width;
    m_Height = Height;

    // Set the viewport & scissors rect
    m_viewport = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
    m_rectScissor = { 0, 0, (LONG)Width, (LONG)Height };

    // Create GBuffer
    //
    m_GBuffer.OnCreateWindowSizeDependentResources(pSwapChain, Width, Height);
    m_renderPassFullGBuffer.OnCreateWindowSizeDependentResources(Width, Height);
    m_renderPassJustDepthAndHdr.OnCreateWindowSizeDependentResources(Width, Height);
    
    // update bloom and downscaling effect
    //
    m_downSample.OnCreateWindowSizeDependentResources(m_Width, m_Height, &m_GBuffer.m_HDR, 5); //downsample the HDR texture 5 times
    m_bloom.OnCreateWindowSizeDependentResources(m_Width / 2, m_Height / 2, m_downSample.GetTexture(), 5, &m_GBuffer.m_HDR);
    m_magnifierPS.OnCreateWindowSizeDependentResources(&m_GBuffer.m_HDR);
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnDestroyWindowSizeDependentResources()
{
    m_magnifierPS.OnDestroyWindowSizeDependentResources();
    m_bloom.OnDestroyWindowSizeDependentResources();
    m_downSample.OnDestroyWindowSizeDependentResources();
    m_GBuffer.OnDestroyWindowSizeDependentResources();
}

//--------------------------------------------------------------------------------------
//
// OnUpdateDisplayDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnUpdateDisplayDependentResources(SwapChain* pSwapChain, const UIState* pState)
{
    // update the pipelines if the swapchain render pass has changed (for example when the format of the swapchain changes)
    //
    m_colorConversionPS.UpdatePipelines(pSwapChain->GetFormat(), pSwapChain->GetDisplayMode());
    m_toneMappingPS.UpdatePipelines(pSwapChain->GetFormat());
    m_lpmPS.UpdatePipeline(pSwapChain->GetFormat(), pSwapChain->GetDisplayMode(), pState->ColorSpace,
        pState->bShoulder,
        pState->SoftGap,
        pState->HdrMax,
        pState->LpmExposure,
        pState->Contrast,
        pState->ShoulderContrast,
        (float *) pState->Saturation,
        (float *) pState->Crosstalk);
}

//--------------------------------------------------------------------------------------
//
// LoadScene
//
//--------------------------------------------------------------------------------------
int Renderer::LoadScene(GLTFCommon *pGLTFCommon, int stage)
{
    // show loading progress
    //
    ImGui::OpenPopup("Loading");
    if (ImGui::BeginPopupModal("Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        float progress = (float)stage / 10.0f;
        ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), NULL);
        ImGui::EndPopup();
    }

    // use multi threading
    AsyncPool *pAsyncPool = &m_asyncPool;

    // Loading stages
    //
    if (stage == 0)
    {
    }
    else if (stage == 5)
    {
        Profile p("m_pGltfLoader->Load");

        m_pGLTFTexturesAndBuffers = new GLTFTexturesAndBuffers();
        m_pGLTFTexturesAndBuffers->OnCreate(m_pDevice, pGLTFCommon, &m_UploadHeap, &m_VidMemBufferPool, &m_ConstantBufferRing);
    }
    else if (stage == 6)
    {
        Profile p("LoadTextures");

        // here we are loading onto the GPU all the textures and the inverse matrices
        // this data will be used to create the PBR and Depth passes       
        m_pGLTFTexturesAndBuffers->LoadTextures(pAsyncPool);
    }
    else if (stage == 7)
    {
        Profile p("m_gltfDepth->OnCreate");

        //create the glTF's textures, VBs, IBs, shaders and descriptors for this particular pass
        m_gltfDepth = new GltfDepthPass();
        m_gltfDepth->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            pAsyncPool
        );
    }
    else if (stage == 8)
    {
        Profile p("m_gltfPBR->OnCreate");

        // same thing as above but for the PBR pass
        m_gltfPBR = new GltfPbrPass();
        m_gltfPBR->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            m_pGLTFTexturesAndBuffers,
            nullptr,
            false,                  // use a SSAO mask
            false,
            &m_renderPassFullGBuffer,
            pAsyncPool
        );

        // we are borrowing the upload heap command list for uploading to the GPU the IBs and VBs
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    }
    else if (stage == 9)
    {
        Profile p("Flush");

        m_UploadHeap.FlushAndFinish();

        //once everything is uploaded we dont need he upload heaps anymore
        m_VidMemBufferPool.FreeUploadHeap();

        // tell caller that we are done loading the map
        return 0;
    }

    stage++;
    return stage;
}

//--------------------------------------------------------------------------------------
//
// UnloadScene
//
//--------------------------------------------------------------------------------------
void Renderer::UnloadScene()
{
    // wait for all the async loading operations to finish
    m_asyncPool.Flush();

    m_pDevice->GPUFlush();

    if (m_gltfPBR)
    {
        m_gltfPBR->OnDestroy();
        delete m_gltfPBR;
        m_gltfPBR = NULL;
    }

    if (m_gltfDepth)
    {
        m_gltfDepth->OnDestroy();
        delete m_gltfDepth;
        m_gltfDepth = NULL;
    }

    if (m_pGLTFTexturesAndBuffers)
    {
        m_pGLTFTexturesAndBuffers->OnDestroy();
        delete m_pGLTFTexturesAndBuffers;
        m_pGLTFTexturesAndBuffers = NULL;
    }

    while (!m_shadowMapPool.empty())
    {
        m_shadowMapPool.back().ShadowMap.OnDestroy();
        m_shadowMapPool.pop_back();
    }
}

void Renderer::AllocateShadowMaps(GLTFCommon* pGLTFCommon)
{
    // Go through the lights and allocate shadow information
    uint32_t NumShadows = 0;
    for (int i = 0; i < pGLTFCommon->m_lightInstances.size(); ++i)
    {
        const tfLight& lightData = pGLTFCommon->m_lights[pGLTFCommon->m_lightInstances[i].m_lightId];
        if (lightData.m_shadowResolution)
        {
            SceneShadowInfo ShadowInfo;
            ShadowInfo.ShadowResolution = lightData.m_shadowResolution;
            ShadowInfo.ShadowIndex = NumShadows++;
            ShadowInfo.LightIndex = i;
            m_shadowMapPool.push_back(ShadowInfo);
        }
    }

    if (NumShadows > MaxShadowInstances)
    {
        Trace("Number of shadows has exceeded maximum supported. Please grow value in gltfCommon.h/perFrameStruct.h");
        throw;
    }

    // If we had shadow information, allocate all required maps and bindings
    if (!m_shadowMapPool.empty())
    {
        m_resourceViewHeaps.AllocDSVDescriptor((uint32_t)m_shadowMapPool.size(), &m_ShadowMapPoolDSV);
        m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor((uint32_t)m_shadowMapPool.size(), &m_ShadowMapPoolSRV);

        std::vector<SceneShadowInfo>::iterator CurrentShadow = m_shadowMapPool.begin();
        for( uint32_t i = 0; CurrentShadow < m_shadowMapPool.end(); ++i, ++CurrentShadow)
        {
            CurrentShadow->ShadowMap.InitDepthStencil(m_pDevice, "m_pShadowMap", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, CurrentShadow->ShadowResolution, CurrentShadow->ShadowResolution, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
            CurrentShadow->ShadowMap.CreateDSV(CurrentShadow->ShadowIndex, &m_ShadowMapPoolDSV);
            CurrentShadow->ShadowMap.CreateSRV(CurrentShadow->ShadowIndex, &m_ShadowMapPoolSRV);
        }
    }
}

//--------------------------------------------------------------------------------------
//
// OnRender
//
//--------------------------------------------------------------------------------------
void Renderer::OnRender(const UIState* pState, const Camera& cam, SwapChain* pSwapChain, float time)
{
    // Timing values
    UINT64 gpuTicksPerSecond;
    m_pDevice->GetGraphicsQueue()->GetTimestampFrequency(&gpuTicksPerSecond);

    // Let our resource managers do some house keeping
    m_CommandListRing.OnBeginFrame();
    m_ConstantBufferRing.OnBeginFrame();
    m_GPUTimer.OnBeginFrame(gpuTicksPerSecond, &m_TimeStamps);

    // Sets the perFrame data 
    per_frame *pPerFrame = NULL;
    if (m_pGLTFTexturesAndBuffers)
    {
        // fill as much as possible using the GLTF (camera, lights, ...)
        pPerFrame = m_pGLTFTexturesAndBuffers->m_pGLTFCommon->SetPerFrameData(cam);

        // Set some lighting factors
        pPerFrame->emmisiveFactor = pState->EmissiveFactor;
        pPerFrame->invScreenResolution[0] = 1.0f / ((float)m_Width);
        pPerFrame->invScreenResolution[1] = 1.0f / ((float)m_Height);

        m_pGLTFTexturesAndBuffers->SetPerFrameConstants();
        m_pGLTFTexturesAndBuffers->SetSkinningMatricesForSkeletons();
    }

    // command buffer calls
    ID3D12GraphicsCommandList* pCmdLst1 = m_CommandListRing.GetNewCommandList();

    m_GPUTimer.GetTimeStamp(pCmdLst1, "Begin Frame");

    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Render shadow maps
    std::vector<CD3DX12_RESOURCE_BARRIER> ShadowReadBarriers;
    std::vector<CD3DX12_RESOURCE_BARRIER> ShadowWriteBarriers;
    if (m_gltfDepth && pPerFrame != NULL)
    {
        std::vector<SceneShadowInfo>::iterator ShadowMap = m_shadowMapPool.begin();
        while (ShadowMap < m_shadowMapPool.end())
        {
            pCmdLst1->ClearDepthStencilView(m_ShadowMapPoolDSV.GetCPU(ShadowMap->ShadowIndex), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            ++ShadowMap;
        }
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear shadow maps");

        // Render all shadows
        ShadowMap = m_shadowMapPool.begin();
        while (ShadowMap < m_shadowMapPool.end())
        {
            SetViewportAndScissor(pCmdLst1, 0, 0, ShadowMap->ShadowResolution, ShadowMap->ShadowResolution);
            pCmdLst1->OMSetRenderTargets(0, NULL, false, &m_ShadowMapPoolDSV.GetCPU(ShadowMap->ShadowIndex));

            GltfDepthPass::per_frame* cbDepthPerFrame = m_gltfDepth->SetPerFrameConstants();
            cbDepthPerFrame->mViewProj = pPerFrame->lights[ShadowMap->LightIndex].mLightViewProj;

            m_gltfDepth->Draw(pCmdLst1);

            // Push a barrier
            ShadowReadBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(ShadowMap->ShadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
            ShadowWriteBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(ShadowMap->ShadowMap.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Shadow map");
            ++ShadowMap;
        }

        // Transition all shadow map barriers
        pCmdLst1->ResourceBarrier((UINT)ShadowReadBarriers.size(), ShadowReadBarriers.data());
    }

    // Render Scene to the GBuffer ------------------------------------------------
    if (pPerFrame != NULL)
    {
        pCmdLst1->RSSetViewports(1, &m_viewport);
        pCmdLst1->RSSetScissorRects(1, &m_rectScissor);

        if (m_gltfPBR)
        {
            std::vector<GltfPbrPass::BatchList> opaque, transparent;
            m_gltfPBR->BuildBatchLists(&opaque, &transparent);

            // Render opaque geometry
            {
                m_renderPassFullGBuffer.BeginPass(pCmdLst1, true);
                m_gltfPBR->DrawBatchList(pCmdLst1, &m_ShadowMapPoolSRV, &opaque);
                m_GPUTimer.GetTimeStamp(pCmdLst1, "PBR Opaque");
                m_renderPassFullGBuffer.EndPass();
            }

            // Draw campfire
            m_CampfireAnimation.Draw(pCmdLst1, time, cam.GetPosition(), cam.GetProjection() * cam.GetView());

            // draw transparent geometry
            {
                m_renderPassFullGBuffer.BeginPass(pCmdLst1, false);

                std::sort(transparent.begin(), transparent.end());
                m_gltfPBR->DrawBatchList(pCmdLst1, &m_ShadowMapPoolSRV, &transparent);
                m_GPUTimer.GetTimeStamp(pCmdLst1, "PBR Transparent");

                m_renderPassFullGBuffer.EndPass();
            }
        }
    }

    if (ShadowWriteBarriers.size())
        pCmdLst1->ResourceBarrier((UINT)ShadowWriteBarriers.size(), ShadowWriteBarriers.data());

    D3D12_RESOURCE_BARRIER preResolve[1] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    };
    pCmdLst1->ResourceBarrier(1, preResolve);

    // Post proc---------------------------------------------------------------------------

    // Bloom, takes HDR as input and applies bloom to it.
    {
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = { m_GBuffer.m_HDRRTV.GetCPU() };
        pCmdLst1->OMSetRenderTargets(ARRAYSIZE(renderTargets), renderTargets, false, NULL);

        m_downSample.Draw(pCmdLst1);
        //m_downSample.Gui();
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Downsample");

        m_bloom.Draw(pCmdLst1, &m_GBuffer.m_HDR);
        //m_bloom.Gui();
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Bloom");
    }

    // Render TestPattern  ------------------------------------------------------------------------
    //
    {
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        if (pState->TestPattern)
        {
            pCmdLst1->RSSetViewports(1, &m_viewport);
            pCmdLst1->RSSetScissorRects(1, &m_rectScissor);
            pCmdLst1->OMSetRenderTargets(1, &m_GBuffer.m_HDRRTV.GetCPU(), true, NULL);

            m_testImages.Draw(pCmdLst1, pState->TestPattern);
            m_GPUTimer.GetTimeStamp(pCmdLst1, "Test Pattern Rendering");
        }
    }

    // Apply Exposure  ------------------------------------------------------------------------
    //
    {
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

        m_exposureMultiplierCS.Draw(pCmdLst1, &m_GBuffer.m_HDRUAV, pState->Exposure, m_Width, m_Height);

        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET));

        m_GPUTimer.GetTimeStamp(pCmdLst1, "Apply Exposure");
    }

    // Render HUD  ------------------------------------------------------------------------
    //
    {
        pCmdLst1->RSSetViewports(1, &m_viewport);
        pCmdLst1->RSSetScissorRects(1, &m_rectScissor);
        pCmdLst1->OMSetRenderTargets(1, &m_GBuffer.m_HDRRTV.GetCPU(), true, NULL);

        m_ImGUI.Draw(pCmdLst1);

        m_GPUTimer.GetTimeStamp(pCmdLst1, "ImGUI Rendering");

        D3D12_RESOURCE_BARRIER hdrToSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        pCmdLst1->ResourceBarrier(1, &hdrToSRV);
    }

    // Magnifier Pass: m_HDR as input, pass' own output
    //
    if (pState->bUseMagnifier)
    {
        pCmdLst1->RSSetViewports(1, &m_viewport);
        pCmdLst1->RSSetScissorRects(1, &m_rectScissor);

        // Note: assumes m_GBuffer.HDR is in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        m_magnifierPS.Draw(pCmdLst1, pState->MagnifierParams, m_GBuffer.m_HDRSRV);
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Magnifier");
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_magnifierPS.GetPassOutputResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));

        pCmdLst1->CopyResource(m_GBuffer.m_HDR.GetResource(), m_magnifierPS.GetPassOutputResource());

        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_magnifierPS.GetPassOutputResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
    }

    // If using FreeSync HDR we need to do the tonemapping in-place and then later we'll apply the color conversion into the swapchain
    // We need to skip this step if using LPM tonemapper as that does the color conversion along with tone and gamut mapping
    const bool bHDR = pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR;
    if (bHDR && pState->SelectedTonemapperIndex > 0)
    {
        D3D12_RESOURCE_BARRIER hdrToUAV = CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        pCmdLst1->ResourceBarrier(1, &hdrToUAV);

        m_toneMappingCS.Draw(pCmdLst1, &m_GBuffer.m_HDRUAV, pState->UnusedExposure, pState->SelectedTonemapperIndex - 1, m_Width, m_Height);

        D3D12_RESOURCE_BARRIER hdrToSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        pCmdLst1->ResourceBarrier(1, &hdrToSRV);

        m_GPUTimer.GetTimeStamp(pCmdLst1, "Tone mapping");
    }

    // submit command buffer #1
    ThrowIfFailed(pCmdLst1->Close());
    ID3D12CommandList* CmdListList1[] = { pCmdLst1 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList1);

    // Wait for swapchain (we are going to render to it) -----------------------------------
    pSwapChain->WaitForSwapChain();


    ID3D12GraphicsCommandList* pCmdLst2 = m_CommandListRing.GetNewCommandList();

    pCmdLst2->RSSetViewports(1, &m_viewport);
    pCmdLst2->RSSetScissorRects(1, &m_rectScissor);
    pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

    // Tonemapping LPM ------------------------------------------------------------------------
    //
    if (pState->SelectedTonemapperIndex == 0)
    {
        m_lpmPS.Draw(pCmdLst2, &m_GBuffer.m_HDRSRV);
        m_GPUTimer.GetTimeStamp(pCmdLst2, "Tone mapping LPM");
    }
    else
    {
        if (bHDR)
        {
            // FS2 mode! Apply color conversion now.
            //
            m_colorConversionPS.Draw(pCmdLst2, &m_GBuffer.m_HDRSRV);
        }
        else
        {
            // non FS2 mode, that is SDR, here we apply the tonemapping from the HDR into the swapchain

            // Tonemapping ------------------------------------------------------------------------
            m_toneMappingPS.Draw(pCmdLst2, &m_GBuffer.m_HDRSRV, pState->UnusedExposure, pState->SelectedTonemapperIndex - 1);
            m_GPUTimer.GetTimeStamp(pCmdLst2, "Tone mapping");
        }
    }

    pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    const bool bTakeScreenshot = !m_ScreenShotName.empty();
    if (bTakeScreenshot)
    {
        m_saveTexture.CopyRenderTargetIntoStagingTexture(m_pDevice->GetDevice(), pCmdLst2, pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    // Transition swapchain into present mode
    pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    m_GPUTimer.OnEndFrame();

    m_GPUTimer.CollectTimings(pCmdLst2);

    // Close & Submit the command list #2 -------------------------------------------------
    ThrowIfFailed(pCmdLst2->Close());

    ID3D12CommandList* CmdListList2[] = { pCmdLst2 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList2);

    if (bTakeScreenshot)
    {
        m_saveTexture.SaveStagingTextureAsJpeg(m_pDevice->GetDevice(), m_pDevice->GetGraphicsQueue(), m_ScreenShotName.c_str());
        m_ScreenShotName = "";
    }
}
