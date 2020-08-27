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

#include "stdafx.h"

#include "SampleRenderer.h"

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnCreate(Device* pDevice, SwapChain *pSwapChain)
{
    m_pDevice = pDevice;

    // Initialize helpers

     // Create all the heaps for the resources views
    const uint32_t cbvDescriptorCount = 2000;
    const uint32_t srvDescriptorCount = 2000;
    const uint32_t uavDescriptorCount = 10;
    const uint32_t dsvDescriptorCount = 3;
    const uint32_t rtvDescriptorCount = 60;
    const uint32_t samplerDescriptorCount = 20;
    m_resourceViewHeaps.OnCreate(pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, dsvDescriptorCount, rtvDescriptorCount, samplerDescriptorCount);

    // Create a commandlist ring for the Direct queue
    // We are queuing (backBufferCount + 0.5) frames, so we need to triple buffer the command lists
    uint32_t commandListsPerBackBuffer = 8;
    m_CommandListRing.OnCreate(pDevice, backBufferCount, commandListsPerBackBuffer, pDevice->GetGraphicsQueue()->GetDesc());

    // Create a 'dynamic' constant buffer
    const uint32_t constantBuffersMemSize = 20 * 1024 * 1024;
    m_ConstantBufferRing.OnCreate(pDevice, backBufferCount, constantBuffersMemSize, &m_resourceViewHeaps);

    // Create a 'static' pool for vertices, indices and constant buffers
    const uint32_t staticGeometryMemSize = 128 * 1024 * 1024;
    m_VidMemBufferPool.OnCreate(pDevice, staticGeometryMemSize, USE_VID_MEM, "StaticGeom");

    // initialize the GPU time stamps module
    m_GPUTimer.OnCreate(pDevice, backBufferCount);

    // Quick helper to upload resources, it has it's own commandList and uses suballocation.
    // for 4K textures we'll need 100Megs
    const uint32_t uploadHeapMemSize = 1000 * 1024 * 1024;
    m_UploadHeap.OnCreate(pDevice, uploadHeapMemSize);    // initialize an upload heap (uses suballocation for faster results)

    // Create the depth buffer view
    m_resourceViewHeaps.AllocDSVDescriptor(1, &m_DepthBufferDSV);

    // Create a Shadowmap atlas to hold 4 cascades/spotlights
    m_shadowMap.InitDepthStencil(pDevice, "m_pShadowMap", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, 2 * 1024, 2 * 1024, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
    m_resourceViewHeaps.AllocDSVDescriptor(1, &m_ShadowMapDSV);
    m_shadowMap.CreateDSV(0, &m_ShadowMapDSV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_ShadowMapSRV);
    m_shadowMap.CreateSRV(0, &m_ShadowMapSRV);

    XMMATRIX campfireWorldMatrix = XMMATRIX(0.5f, 0.0f, 0.0f, 0.0f,
                                            0.0f, 0.5f, 0.0f, 0.0f,
                                            0.0f, 0.0f, 0.5f, 0.0f,
                                           -47.3f, -26.5f, -40.425f, 1.0f);
    m_CampfireAnimation.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, 4, DXGI_FORMAT_R16G16B16A16_FLOAT, 12, 12, "..\\media\\FlipBookAnimationTextures\\cf_loop_comp_v1_mosaic.png", campfireWorldMatrix);

    m_downSample.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_bloom.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);

    m_testImages.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing, DXGI_FORMAT_R16G16B16A16_FLOAT);

    // Create tonemapping pass
    m_exposureMultiplierCS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing);
    m_lpmPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());
    m_toneMappingPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());
    m_toneMappingCS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing);
    m_colorConversionPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, DXGI_FORMAT_R16G16B16A16_FLOAT);

    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_HDRRTV);
    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_HDRRTVMSAA);

    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_HDRSRV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_HDRUAV);

    // Make sure upload heap has finished uploading before continuing
#if (USE_VID_MEM==true)
    m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    m_UploadHeap.FlushAndFinish();
#endif
}

//--------------------------------------------------------------------------------------
//
// OnDestroy 
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnDestroy()
{
    m_colorConversionPS.OnDestroy();
    m_toneMappingCS.OnDestroy();
    m_toneMappingPS.OnDestroy();
    m_exposureMultiplierCS.OnDestroy();
    m_lpmPS.OnDestroy();
    m_ImGUI.OnDestroy();
    m_testImages.OnDestroy();
    m_bloom.OnDestroy();
    m_downSample.OnDestroy();
    m_CampfireAnimation.OnDestroy();
    m_shadowMap.OnDestroy();

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
void SampleRenderer::OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height, State *pState)
{
    m_Width = Width;
    m_Height = Height;

    // Set the viewport
    //
    m_ViewPort = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };

    // Create scissor rectangle
    //
    m_RectScissor = { 0, 0, (LONG)Width, (LONG)Height };

    // Create depth buffer    
    //
    m_depthBuffer.InitDepthStencil(m_pDevice, "depthbuffer", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, Width, Height, 1, 1, 4, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));
    m_depthBuffer.CreateDSV(0, &m_DepthBufferDSV);

    // Create Texture + RTV with x4 MSAA
    //
    CD3DX12_RESOURCE_DESC RDescMSAA = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, Width, Height, 1, 1, 4, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    m_HDRMSAA.InitRenderTarget(m_pDevice, "HDRMSAA", &RDescMSAA, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_HDRMSAA.CreateRTV(0, &m_HDRRTVMSAA);

    // Create Texture + RTV, to hold the resolved scene 
    //
    CD3DX12_RESOURCE_DESC RDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_HDR.InitRenderTarget(m_pDevice, "HDR", &RDesc, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_HDR.CreateSRV(0, &m_HDRSRV);
    m_HDR.CreateUAV(0, &m_HDRUAV);
    m_HDR.CreateRTV(0, &m_HDRRTV);

    // update bloom and downscaling effect
    //
    {
        m_downSample.OnCreateWindowSizeDependentResources(m_Width, m_Height, &m_HDR, 6); //downsample the HDR texture 6 times
        m_bloom.OnCreateWindowSizeDependentResources(m_Width / 2, m_Height / 2, m_downSample.GetTexture(), 6, &m_HDR);
    }

    // update the pipelines if the swapchain render pass has changed (for example when the format of the swapchain changes)
    //
    m_colorConversionPS.UpdatePipelines(pSwapChain->GetFormat(), pSwapChain->GetDisplayMode());
    m_toneMappingPS.UpdatePipelines(pSwapChain->GetFormat());
    m_lpmPS.UpdatePipeline(pSwapChain->GetFormat(), pSwapChain->GetDisplayMode(), pState->colorSpace);
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnDestroyWindowSizeDependentResources()
{
    m_bloom.OnDestroyWindowSizeDependentResources();
    m_downSample.OnDestroyWindowSizeDependentResources();

    m_HDR.OnDestroy();
    m_HDRMSAA.OnDestroy();
    m_depthBuffer.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// LoadScene
//
//--------------------------------------------------------------------------------------
int SampleRenderer::LoadScene(GLTFCommon *pGLTFCommon, int stage)
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
    	m_pGLTFTexturesAndBuffers->LoadTextures();
    }
    else if (stage == 7)
    {
    	Profile p("m_gltfDepth->OnCreate");
    	//create the glTF's textures, VBs, IBs, shaders and descriptors    
    	m_gltfDepth = new GltfDepthPass();
    	m_gltfDepth->OnCreate(
    		m_pDevice,
    		&m_UploadHeap,
    		&m_resourceViewHeaps,
    		&m_ConstantBufferRing,
    		&m_VidMemBufferPool,
    		m_pGLTFTexturesAndBuffers
    	);
    }
    else if (stage == 8)
    {
    	Profile p("m_gltfPBR->OnCreate");
    	m_gltfPBR = new GltfPbrPass();
    	m_gltfPBR->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            nullptr,
            false,
            USE_SHADOWMASK,
            m_HDRMSAA.GetFormat(), // forward pass channel
            DXGI_FORMAT_UNKNOWN,   // specular-roughness channel
            DXGI_FORMAT_UNKNOWN,   // diffuse channel
            DXGI_FORMAT_UNKNOWN,   // normal channel
            4
        );
#if (USE_VID_MEM==true)
        // we are borrowing the upload heap command list for uploading to the GPU the IBs and VBs
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
#endif
    }
    else if (stage == 10)
    {
        Profile p("Flush");
        m_UploadHeap.FlushAndFinish();

#if (USE_VID_MEM==true)
        //once everything is uploaded we dont need he upload heaps anymore
        m_VidMemBufferPool.FreeUploadHeap();
#endif    

        // tell caller that we are done loading the map
        return -1;
	}

    stage++;
    return stage;
}

//--------------------------------------------------------------------------------------
//
// UnloadScene
//
//--------------------------------------------------------------------------------------
void SampleRenderer::UnloadScene()
{
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

}

//--------------------------------------------------------------------------------------
//
// OnRender
//
//--------------------------------------------------------------------------------------
void SampleRenderer::OnRender(State *pState, SwapChain *pSwapChain)
{
    // Timing values
    //
    UINT64 gpuTicksPerSecond;
    m_pDevice->GetGraphicsQueue()->GetTimestampFrequency(&gpuTicksPerSecond);
    
    // Let our resource managers do some house keeping 
    //
    m_ConstantBufferRing.OnBeginFrame();
    m_GPUTimer.OnBeginFrame(gpuTicksPerSecond, &m_TimeStamps);

    per_frame *pPerFrame = NULL;
    if (m_pGLTFTexturesAndBuffers)
    {
        pPerFrame = m_pGLTFTexturesAndBuffers->m_pGLTFCommon->SetPerFrameData(pState->camera);

        //override gltf camera with ours
        pPerFrame->mCameraViewProj = pState->camera.GetView() * pState->camera.GetProjection();
        pPerFrame->cameraPos = pState->camera.GetPosition();
        pPerFrame->emmisiveFactor = pState->emmisiveFactor;

        pPerFrame->lights[0].intensity = pState->lightIntensity;

        // Up to 4 spotlights can have shadowmaps. Each spot the light has a shadowMap index which is used to find the sadowmap in the atlas
        uint32_t shadowMapIndex = 0;
        for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
        {
            if ((shadowMapIndex < 4) && (pPerFrame->lights[i].type == LightType_Spot))
            {
                pPerFrame->lights[i].shadowMapIndex = shadowMapIndex++; //set the shadowmap index so the color pass knows which shadow map to use
                pPerFrame->lights[i].depthBias = 70.0f / 100000.0f;
            }
        }

        m_pGLTFTexturesAndBuffers->SetPerFrameConstants();

        m_pGLTFTexturesAndBuffers->SetSkinningMatricesForSkeletons();
    }

    // command buffer calls
    //    
    ID3D12GraphicsCommandList* pCmdLst1 = m_CommandListRing.GetNewCommandList();

    m_GPUTimer.GetTimeStamp(pCmdLst1, "Begin Frame");

    // Clear GBuffer and depth stencil
    //
    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clears -----------------------------------------------------------------------
    //
    pCmdLst1->ClearDepthStencilView(m_ShadowMapDSV.GetCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear Shadow Map");

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCmdLst1->ClearRenderTargetView(m_HDRRTVMSAA.GetCPU(), clearColor, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear HDR");

    pCmdLst1->ClearDepthStencilView(m_DepthBufferDSV.GetCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear Depth");

    // Render to shadow map atlas ---------------------------------------------------
    //
    if (m_gltfDepth && pPerFrame != NULL)
    {
        uint32_t shadowMapIndex = 0;
        for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
        {
            if (pPerFrame->lights[i].type != LightType_Spot)
                continue;

            // Set the RT's quadrant where to render the shadomap (these viewport offsets need to match the ones in shadowFiltering.h)
            uint32_t viewportOffsetsX[4] = { 0, 1, 0, 1 };
            uint32_t viewportOffsetsY[4] = { 0, 0, 1, 1 };
            uint32_t viewportWidth = m_shadowMap.GetWidth() / 2;
            uint32_t viewportHeight = m_shadowMap.GetHeight() / 2;
            SetViewportAndScissor(pCmdLst1, viewportOffsetsX[i] * viewportWidth, viewportOffsetsY[i] * viewportHeight, viewportWidth, viewportHeight);
            pCmdLst1->OMSetRenderTargets(0, NULL, true, &m_ShadowMapDSV.GetCPU());

            GltfDepthPass::per_frame *cbDepthPerFrame = m_gltfDepth->SetPerFrameConstants();
            cbDepthPerFrame->mViewProj = pPerFrame->lights[i].mLightViewProj;

            m_gltfDepth->Draw(pCmdLst1);

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Shadow map");
            shadowMapIndex++;
        }
    }
    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    // Render Scene to the MSAA HDR RT ------------------------------------------------
    //
    pCmdLst1->RSSetViewports(1, &m_ViewPort);
    pCmdLst1->RSSetScissorRects(1, &m_RectScissor);
    pCmdLst1->OMSetRenderTargets(1, &m_HDRRTVMSAA.GetCPU(), true, &m_DepthBufferDSV.GetCPU());

    if (pPerFrame != NULL)
    {
        // Render scene to color buffer
        //    
        if (m_gltfPBR && pPerFrame != NULL)
        {
            //set per frame constant buffer values
            m_gltfPBR->Draw(pCmdLst1, &m_ShadowMapSRV);
            m_CampfireAnimation.Draw(pCmdLst1, pState->time, pState->camera.GetPosition(), pState->camera.GetView() * pState->camera.GetProjection());
        }
    }
    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    m_GPUTimer.GetTimeStamp(pCmdLst1, "Rendering Scene");

    // Resolve MSAA ------------------------------------------------------------------------
    //
    {
        UserMarker marker(pCmdLst1, "Resolving MSAA");

        D3D12_RESOURCE_BARRIER preResolve[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST),
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDRMSAA.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
        };
        pCmdLst1->ResourceBarrier(2, preResolve);

        pCmdLst1->ResolveSubresource(m_HDR.GetResource(), 0, m_HDRMSAA.GetResource(), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);

        D3D12_RESOURCE_BARRIER postResolve[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDRMSAA.GetResource(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
        pCmdLst1->ResourceBarrier(2, postResolve);

        m_GPUTimer.GetTimeStamp(pCmdLst1, "Resolve MSAA");
    }

    // Post proc---------------------------------------------------------------------------
    //
    {
        m_downSample.Draw(pCmdLst1);
        //m_downSample.Gui();
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Downsample");
        
        m_bloom.Draw(pCmdLst1, &m_HDR);
        //m_bloom.Gui();
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Bloom");
    }
    
    // Render TestPattern  ------------------------------------------------------------------------
    //
    {
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        if (pState->testPattern)
        {
            pCmdLst1->RSSetViewports(1, &m_ViewPort);
            pCmdLst1->RSSetScissorRects(1, &m_RectScissor);
            pCmdLst1->OMSetRenderTargets(1, &m_HDRRTV.GetCPU(), true, NULL);

            m_testImages.Draw(pCmdLst1, pState->testPattern);
            m_GPUTimer.GetTimeStamp(pCmdLst1, "Test Pattern Rendering");
        }
    }

    // Apply Exposure  ------------------------------------------------------------------------
    //
    {
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

        m_exposureMultiplierCS.Draw(pCmdLst1, &m_HDRUAV, pState->exposure, m_Width, m_Height);

        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET));

        m_GPUTimer.GetTimeStamp(pCmdLst1, "Apply Exposure");
    }

    // Render HUD  ------------------------------------------------------------------------
    //
    {
        pCmdLst1->RSSetViewports(1, &m_ViewPort);
        pCmdLst1->RSSetScissorRects(1, &m_RectScissor);
        pCmdLst1->OMSetRenderTargets(1, &m_HDRRTV.GetCPU(), true, NULL);

        m_ImGUI.Draw(pCmdLst1);

        m_GPUTimer.GetTimeStamp(pCmdLst1, "ImGUI Rendering");

        D3D12_RESOURCE_BARRIER hdrToSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        pCmdLst1->ResourceBarrier(1, &hdrToSRV);
    }

    // If using FreeSync2 we need to to the tonemapping in-place and then later we'll apply the color conversion into the swapchain
    // We need to skip this step if using LPM tonemapper that does the color conversion along with tone and gamut mapping
    if (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR && pState->toneMapper < 6)
    {
        D3D12_RESOURCE_BARRIER hdrToUAV = CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        pCmdLst1->ResourceBarrier(1, &hdrToUAV);

        m_toneMappingCS.Draw(pCmdLst1, &m_HDRUAV, pState->unusedExposure, pState->toneMapper, m_Width, m_Height);

        D3D12_RESOURCE_BARRIER hdrToSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        pCmdLst1->ResourceBarrier(1, &hdrToSRV);

        m_GPUTimer.GetTimeStamp(pCmdLst1, "Tone mapping");
    }

    // submit command buffer
    ThrowIfFailed(pCmdLst1->Close());
    ID3D12CommandList* CmdListList1[] = { pCmdLst1 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList1);

    // Wait for swapchain (we are going to render to it) -----------------------------------
    //
    pSwapChain->WaitForSwapChain();

    m_CommandListRing.OnBeginFrame();

    ID3D12GraphicsCommandList* pCmdLst2 = m_CommandListRing.GetNewCommandList();

    pCmdLst2->RSSetViewports(1, &m_ViewPort);
    pCmdLst2->RSSetScissorRects(1, &m_RectScissor);
    pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

    // Tonemapping LPM ------------------------------------------------------------------------
    //
    if (pState->toneMapper == 6)
    {
        m_lpmPS.Draw(pCmdLst2, &m_HDRSRV);
        m_GPUTimer.GetTimeStamp(pCmdLst2, "Tone mapping LPM");
    }
    else
    {
        if (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR)
        {
            // FS2 mode! Apply color conversion now.
            //
            m_colorConversionPS.Draw(pCmdLst2, &m_HDRSRV);
        }
        else
        {
            // non FS2 mode, that is SDR, here we apply the tonemapping from the HDR into the swapchain
            //
            // Tonemapping ------------------------------------------------------------------------
            //
            m_toneMappingPS.Draw(pCmdLst2, &m_HDRSRV, pState->unusedExposure, pState->toneMapper);
            m_GPUTimer.GetTimeStamp(pCmdLst2, "Tone mapping");
        }
    }

    pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Transition swapchain into present mode
    pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    m_GPUTimer.OnEndFrame();

    m_GPUTimer.CollectTimings(pCmdLst2);

    // Close & Submit the command list ----------------------------------------------------
    //
    ThrowIfFailed(pCmdLst2->Close());

    ID3D12CommandList* CmdListList2[] = { pCmdLst2 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList2);
}
