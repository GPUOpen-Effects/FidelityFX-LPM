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
    const uint32_t cbvDescriptorCount = 2000;
    const uint32_t srvDescriptorCount = 8000;
    const uint32_t uavDescriptorCount = 10;
    const uint32_t samplerDescriptorCount = 20;
    m_resourceViewHeaps.OnCreate(pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, samplerDescriptorCount);

    // Create a commandlist ring for the Direct queue
    uint32_t commandListsPerBackBuffer = 8;
    m_CommandListRing.OnCreate(pDevice, backBufferCount, commandListsPerBackBuffer);

    // Create a 'dynamic' constant buffer
    const uint32_t constantBuffersMemSize = 200 * 1024 * 1024;
    m_ConstantBufferRing.OnCreate(pDevice, backBufferCount, constantBuffersMemSize, "Uniforms");

    // Create a 'static' pool for vertices and indices 
    const uint32_t staticGeometryMemSize = (1 * 128) * 1024 * 1024;
    m_VidMemBufferPool.OnCreate(pDevice, staticGeometryMemSize, true, "StaticGeom");

    // Create a 'static' pool for vertices and indices in system memory
    const uint32_t systemGeometryMemSize = 32 * 1024;
    m_SysMemBufferPool.OnCreate(pDevice, systemGeometryMemSize, false, "PostProcGeom");

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
                { GBUFFER_DEPTH, VK_FORMAT_D32_SFLOAT},
                { GBUFFER_FORWARD, VK_FORMAT_R16G16B16A16_SFLOAT},
            },
            1
        );

        GBufferFlags fullGBuffer = GBUFFER_DEPTH | GBUFFER_FORWARD;
        bool bClear = true;
        m_renderPassFullGBufferWithClear.OnCreate(&m_GBuffer, fullGBuffer, bClear,"m_renderPassFullGBufferWithClear");
        m_renderPassFullGBuffer.OnCreate(&m_GBuffer, fullGBuffer, !bClear, "m_renderPassFullGBuffer");
        m_renderPassJustDepthAndHdr.OnCreate(&m_GBuffer, GBUFFER_DEPTH | GBUFFER_FORWARD, !bClear, "m_renderPassJustDepthAndHdr");
    }

    // Create render pass shadow, will clear contents
    //
    {
        VkAttachmentDescription depthAttachments;
        AttachClearBeforeUse(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &depthAttachments);
        m_render_pass_shadow = CreateRenderPassOptimal(m_pDevice->GetDevice(), 0, NULL, &depthAttachments);
    }

    m_downSample.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_bloom.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_magnifierPS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);

    // Create tonemapping pass
    m_toneMappingCS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing);
    m_toneMappingPS.OnCreate(m_pDevice, pSwapChain->GetRenderPass(), &m_resourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);
    m_colorConversionPS.OnCreate(pDevice, pSwapChain->GetRenderPass(), &m_resourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(m_pDevice, m_renderPassJustDepthAndHdr.GetRenderPass(), &m_UploadHeap, &m_ConstantBufferRing, fontSize);

    math::Matrix4 campfireWorldMatrix = math::Matrix4(math::Vector4(0.5f, 0.0f, 0.0f, 0.0f),
                                                      math::Vector4(0.0f, 0.5f, 0.0f, 0.0f),
                                                      math::Vector4(0.0f, 0.0f, 0.5f, 0.0f),
                                                      math::Vector4(-47.3f, -26.5f, -40.425f, 1.0f));
    m_CampfireAnimation.OnCreate(pDevice, m_renderPassFullGBufferWithClear.GetRenderPass(), &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_SAMPLE_COUNT_1_BIT, 12, 12, "..\\media\\FlipBookAnimationTextures\\cf_loop_comp_v1_mosaic.png", campfireWorldMatrix);
    // Create test pattern render pass
    {
        m_render_pass_TestPattern = SimpleColorWriteRenderPass(m_pDevice->GetDevice(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    m_testImages.OnCreate(pDevice, m_render_pass_TestPattern, &m_UploadHeap, &m_resourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);

    // Create tonemapping pass
    m_exposureMultiplierCS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing);
    m_lpmPS.OnCreate(pDevice, pSwapChain->GetRenderPass(), &m_resourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);

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
    vkDestroyRenderPass(m_pDevice->GetDevice(), m_render_pass_TestPattern, nullptr);
    m_CampfireAnimation.OnDestroy();

    m_asyncPool.Flush();

    m_ImGUI.OnDestroy();
    m_colorConversionPS.OnDestroy();
    m_toneMappingPS.OnDestroy();
    m_toneMappingCS.OnDestroy();
    m_bloom.OnDestroy();
    m_downSample.OnDestroy();
    m_magnifierPS.OnDestroy();

    m_renderPassFullGBufferWithClear.OnDestroy();
    m_renderPassJustDepthAndHdr.OnDestroy();
    m_renderPassFullGBuffer.OnDestroy();
    m_GBuffer.OnDestroy();

    vkDestroyRenderPass(m_pDevice->GetDevice(), m_render_pass_shadow, nullptr);

    m_UploadHeap.OnDestroy();
    m_GPUTimer.OnDestroy();
    m_VidMemBufferPool.OnDestroy();
    m_SysMemBufferPool.OnDestroy();
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

    // Set the viewport
    //
    m_viewport.x = 0;
    m_viewport.y = (float)Height;
    m_viewport.width = (float)Width;
    m_viewport.height = -(float)(Height);
    m_viewport.minDepth = (float)0.0f;
    m_viewport.maxDepth = (float)1.0f;

    // Create scissor rectangle
    //
    m_rectScissor.extent.width = Width;
    m_rectScissor.extent.height = Height;
    m_rectScissor.offset.x = 0;
    m_rectScissor.offset.y = 0;
   
    // Create GBuffer
    //
    m_GBuffer.OnCreateWindowSizeDependentResources(pSwapChain, Width, Height);

    // Create frame buffers for the GBuffer render passes
    //
    m_renderPassFullGBufferWithClear.OnCreateWindowSizeDependentResources(Width, Height);
    m_renderPassJustDepthAndHdr.OnCreateWindowSizeDependentResources(Width, Height);
    m_renderPassFullGBuffer.OnCreateWindowSizeDependentResources(Width, Height);

    // Update PostProcessing passes
    //
    m_downSample.OnCreateWindowSizeDependentResources(Width, Height, &m_GBuffer.m_HDR, 6); //downsample the HDR texture 6 times
    m_bloom.OnCreateWindowSizeDependentResources(Width / 2, Height / 2, m_downSample.GetTexture(), 6, &m_GBuffer.m_HDR);
    m_magnifierPS.OnCreateWindowSizeDependentResources(&m_GBuffer.m_HDR);

    // Create test pasttern framebuffer
    std::vector<VkImageView> pAttachments;
    pAttachments.push_back(m_GBuffer.m_HDRSRV);
    m_pFrameBuffer_TestPattern = CreateFrameBuffer(m_pDevice->GetDevice(), m_render_pass_TestPattern, &pAttachments, Width, Height);
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnDestroyWindowSizeDependentResources()
{
    vkDestroyFramebuffer(m_pDevice->GetDevice(), m_pFrameBuffer_TestPattern, nullptr);

    m_bloom.OnDestroyWindowSizeDependentResources();
    m_downSample.OnDestroyWindowSizeDependentResources();
    m_magnifierPS.OnDestroyWindowSizeDependentResources();

    m_renderPassFullGBufferWithClear.OnDestroyWindowSizeDependentResources();
    m_renderPassJustDepthAndHdr.OnDestroyWindowSizeDependentResources();
    m_renderPassFullGBuffer.OnDestroyWindowSizeDependentResources();
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
    m_toneMappingPS.UpdatePipelines(pSwapChain->GetRenderPass());
    OnUpdateLocalDimmingChangedResources(pSwapChain, pState);
}

//--------------------------------------------------------------------------------------
//
// OnUpdateLocalDimmingChangedResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnUpdateLocalDimmingChangedResources(SwapChain *pSwapChain, const UIState* pState)
{
    m_colorConversionPS.UpdatePipelines(pSwapChain->GetRenderPass(), pSwapChain->GetDisplayMode());
    m_lpmPS.UpdatePipelines(pSwapChain->GetRenderPass(), pSwapChain->GetDisplayMode(), pState->ColorSpace,
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
            m_render_pass_shadow,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            pAsyncPool
        );

        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
        m_UploadHeap.FlushAndFinish();
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
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            nullptr,
            false, // use SSAO mask
            m_ShadowSRVPool,
            &m_renderPassFullGBufferWithClear,
            pAsyncPool
        );

        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    }
    else if (stage == 9)
    {
        Profile p("Flush");

        m_UploadHeap.FlushAndFinish();

        //once everything is uploaded we dont need the upload heaps anymore
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

    assert(m_shadowMapPool.size() == m_ShadowSRVPool.size());
    while (!m_shadowMapPool.empty())
    {
        m_shadowMapPool.back().ShadowMap.OnDestroy();
        vkDestroyFramebuffer(m_pDevice->GetDevice(), m_shadowMapPool.back().ShadowFrameBuffer, nullptr);
        vkDestroyImageView(m_pDevice->GetDevice(), m_ShadowSRVPool.back(), nullptr);
        vkDestroyImageView(m_pDevice->GetDevice(), m_shadowMapPool.back().ShadowDSV, nullptr);
        m_ShadowSRVPool.pop_back();
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
        std::vector<SceneShadowInfo>::iterator CurrentShadow = m_shadowMapPool.begin();
        for (uint32_t i = 0; CurrentShadow < m_shadowMapPool.end(); ++i, ++CurrentShadow)
        {
            CurrentShadow->ShadowMap.InitDepthStencil(m_pDevice, CurrentShadow->ShadowResolution, CurrentShadow->ShadowResolution, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, "ShadowMap");
            CurrentShadow->ShadowMap.CreateDSV(&CurrentShadow->ShadowDSV);

            // Create render pass shadow, will clear contents
            {
                VkAttachmentDescription depthAttachments;
                AttachClearBeforeUse(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &depthAttachments);

                // Create frame buffer
                VkImageView attachmentViews[1] = { CurrentShadow->ShadowDSV };
                VkFramebufferCreateInfo fb_info = {};
                fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fb_info.pNext = NULL;
                fb_info.renderPass = m_render_pass_shadow;
                fb_info.attachmentCount = 1;
                fb_info.pAttachments = attachmentViews;
                fb_info.width = CurrentShadow->ShadowResolution;
                fb_info.height = CurrentShadow->ShadowResolution;
                fb_info.layers = 1;
                VkResult res = vkCreateFramebuffer(m_pDevice->GetDevice(), &fb_info, NULL, &CurrentShadow->ShadowFrameBuffer);
                assert(res == VK_SUCCESS);
            }

            VkImageView ShadowSRV;
            CurrentShadow->ShadowMap.CreateSRV(&ShadowSRV);
            m_ShadowSRVPool.push_back(ShadowSRV);
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
    // Let our resource managers do some house keeping 
    m_ConstantBufferRing.OnBeginFrame();

    // command buffer calls
    VkCommandBuffer cmdBuf1 = m_CommandListRing.GetNewCommandList();

    {
        VkCommandBufferBeginInfo cmd_buf_info;
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmd_buf_info.pInheritanceInfo = NULL;
        VkResult res = vkBeginCommandBuffer(cmdBuf1, &cmd_buf_info);
        assert(res == VK_SUCCESS);
    }

    m_GPUTimer.OnBeginFrame(cmdBuf1, &m_TimeStamps);

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

    // Render all shadow maps
    if (m_gltfDepth && pPerFrame != NULL)
    {
        SetPerfMarkerBegin(cmdBuf1, "ShadowPass");

        VkClearValue depth_clear_values[1];
        depth_clear_values[0].depthStencil.depth = 1.0f;
        depth_clear_values[0].depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin;
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.pNext = NULL;
        rp_begin.renderPass = m_render_pass_shadow;
        rp_begin.renderArea.offset.x = 0;
        rp_begin.renderArea.offset.y = 0;
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues = depth_clear_values;

        std::vector<SceneShadowInfo>::iterator ShadowMap = m_shadowMapPool.begin();
        while (ShadowMap < m_shadowMapPool.end())
        {
            // Clear shadow map
            rp_begin.framebuffer = ShadowMap->ShadowFrameBuffer;
            rp_begin.renderArea.extent.width = ShadowMap->ShadowResolution;
            rp_begin.renderArea.extent.height = ShadowMap->ShadowResolution;
            vkCmdBeginRenderPass(cmdBuf1, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

            // Render to shadow map
            SetViewportAndScissor(cmdBuf1, 0, 0, ShadowMap->ShadowResolution, ShadowMap->ShadowResolution);

            // Set per frame constant buffer values
            GltfDepthPass::per_frame* cbPerFrame = m_gltfDepth->SetPerFrameConstants();
            cbPerFrame->mViewProj = pPerFrame->lights[ShadowMap->LightIndex].mLightViewProj;

            m_gltfDepth->Draw(cmdBuf1);

            m_GPUTimer.GetTimeStamp(cmdBuf1, "Shadow Map Render");

            vkCmdEndRenderPass(cmdBuf1);
            ++ShadowMap;
        }

        SetPerfMarkerEnd(cmdBuf1);
    }

    // Render Scene to the GBuffer ------------------------------------------------
    SetPerfMarkerBegin(cmdBuf1, "Color pass");

    VkRect2D renderArea = { 0, 0, m_Width, m_Height };

    if (pPerFrame != NULL && m_gltfPBR)
    {
        std::vector<GltfPbrPass::BatchList> opaque, transparent;
        m_gltfPBR->BuildBatchLists(&opaque, &transparent);

        // Render opaque 
        {
            m_renderPassFullGBufferWithClear.BeginPass(cmdBuf1, renderArea);

            m_gltfPBR->DrawBatchList(cmdBuf1, &opaque);
            m_GPUTimer.GetTimeStamp(cmdBuf1, "PBR Opaque");

            // Draw campfire
            m_CampfireAnimation.Draw(cmdBuf1, time, cam.GetPosition(), cam.GetProjection() * cam.GetView());

            m_renderPassFullGBufferWithClear.EndPass(cmdBuf1);
        }

        // draw transparent geometry
        {
            m_renderPassFullGBuffer.BeginPass(cmdBuf1, renderArea);

            std::sort(transparent.begin(), transparent.end());
            m_gltfPBR->DrawBatchList(cmdBuf1, &transparent);
            m_GPUTimer.GetTimeStamp(cmdBuf1, "PBR Transparent");

            //m_GBuffer.EndPass(cmdBuf1);
            m_renderPassFullGBuffer.EndPass(cmdBuf1);
        }
    }
    else
    {
        m_renderPassFullGBufferWithClear.BeginPass(cmdBuf1, renderArea);
        m_renderPassFullGBufferWithClear.EndPass(cmdBuf1);
        m_renderPassJustDepthAndHdr.BeginPass(cmdBuf1, renderArea);
        m_renderPassJustDepthAndHdr.EndPass(cmdBuf1);
    }

    VkImageMemoryBarrier barrier[1] = {};
    barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier[0].pNext = NULL;
    barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier[0].subresourceRange.baseMipLevel = 0;
    barrier[0].subresourceRange.levelCount = 1;
    barrier[0].subresourceRange.baseArrayLayer = 0;
    barrier[0].subresourceRange.layerCount = 1;
    barrier[0].image = m_GBuffer.m_HDR.Resource();
    vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, barrier);

    SetPerfMarkerEnd(cmdBuf1);

    // Post proc---------------------------------------------------------------------------

    // Bloom, takes HDR as input and applies bloom to it.
    {
        SetPerfMarkerBegin(cmdBuf1, "PostProcess");
        
        // Downsample pass
        m_downSample.Draw(cmdBuf1);
        // m_downSample.Gui();
        m_GPUTimer.GetTimeStamp(cmdBuf1, "Downsample");

        // Bloom pass (needs the downsampled data)
        m_bloom.Draw(cmdBuf1);
        // m_bloom.Gui();
        m_GPUTimer.GetTimeStamp(cmdBuf1, "Bloom");

        SetPerfMarkerEnd(cmdBuf1);
    }

    barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier[0].pNext = NULL;
    barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier[0].subresourceRange.baseMipLevel = 0;
    barrier[0].subresourceRange.levelCount = 1;
    barrier[0].subresourceRange.baseArrayLayer = 0;
    barrier[0].subresourceRange.layerCount = 1;
    barrier[0].image = m_GBuffer.m_HDR.Resource();
    vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, barrier);

    // Render TestPattern  ------------------------------------------------------------------------
    //
    {
        SetPerfMarkerBegin(cmdBuf1, "TestPattern");

        // prepare render pass
        {
            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.pNext = NULL;
            rp_begin.renderPass = m_render_pass_TestPattern;
            rp_begin.framebuffer = m_pFrameBuffer_TestPattern;
            rp_begin.renderArea.offset.x = 0;
            rp_begin.renderArea.offset.y = 0;
            rp_begin.renderArea.extent.width = m_Width;
            rp_begin.renderArea.extent.height = m_Height;
            rp_begin.clearValueCount = 0;
            rp_begin.pClearValues = NULL;
            vkCmdBeginRenderPass(cmdBuf1, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        }

        vkCmdSetScissor(cmdBuf1, 0, 1, &m_rectScissor);
        vkCmdSetViewport(cmdBuf1, 0, 1, &m_viewport);

        if (pState->TestPattern)
        {
            m_testImages.Draw(cmdBuf1, pState->TestPattern);
        }

        vkCmdEndRenderPass(cmdBuf1);

        m_GPUTimer.GetTimeStamp(cmdBuf1, "TestPattern");

        SetPerfMarkerEnd(cmdBuf1);
    }

    // Apply Exposure  ------------------------------------------------------------------------
    //
    {
        SetPerfMarkerBegin(cmdBuf1, "Exposure Multiplier");
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // we need to read from it for the post-processing
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_GBuffer.m_HDR.Resource();
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        m_exposureMultiplierCS.Draw(cmdBuf1, m_GBuffer.m_HDRSRV, pState->Exposure, m_Width, m_Height);

        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_GBuffer.m_HDR.Resource();
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        m_GPUTimer.GetTimeStamp(cmdBuf1, "Exposure Multiplier");

        SetPerfMarkerEnd(cmdBuf1);
    }

    // Render HUD  ------------------------------------------------------------------------
    //
    {
        SetPerfMarkerBegin(cmdBuf1, "ImGUI Rendering");

        // prepare render pass
        {
            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.pNext = NULL;
            rp_begin.renderPass = m_renderPassJustDepthAndHdr.GetRenderPass();
            rp_begin.framebuffer = m_renderPassJustDepthAndHdr.GetFramebuffer();
            rp_begin.renderArea.offset.x = 0;
            rp_begin.renderArea.offset.y = 0;
            rp_begin.renderArea.extent.width = m_Width;
            rp_begin.renderArea.extent.height = m_Height;
            rp_begin.clearValueCount = 0;
            rp_begin.pClearValues = NULL;
            vkCmdBeginRenderPass(cmdBuf1, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        }

        vkCmdSetScissor(cmdBuf1, 0, 1, &m_rectScissor);
        vkCmdSetViewport(cmdBuf1, 0, 1, &m_viewport);

        m_ImGUI.Draw(cmdBuf1);

        vkCmdEndRenderPass(cmdBuf1);

        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // we need to read from it for the post-processing
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_GBuffer.m_HDR.Resource();
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        m_GPUTimer.GetTimeStamp(cmdBuf1, "ImGUI Rendering");

        SetPerfMarkerEnd(cmdBuf1);
    }

    // Magnifier Pass: m_HDR as input, pass' own output
    //
    if (pState->bUseMagnifier)
    {
        vkCmdSetScissor(cmdBuf1, 0, 1, &m_rectScissor);
        vkCmdSetViewport(cmdBuf1, 0, 1, &m_viewport);

        // Note: assumes m_GBuffer.HDR is in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        m_magnifierPS.Draw(cmdBuf1, pState->MagnifierParams);
        m_GPUTimer.GetTimeStamp(cmdBuf1, "Magnifier");

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = NULL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.image = m_GBuffer.m_HDR.Resource();

        VkImageMemoryBarrier barriers[2];
        barriers[0] = barrier;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].image = m_GBuffer.m_HDR.Resource();

        barriers[1] = barrier;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[1].image = m_magnifierPS.GetPassOutputResource();

        vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, barriers);

        VkImageCopy imageCopyRegion{};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = m_GBuffer.m_HDR.GetWidth();
        imageCopyRegion.extent.height = m_GBuffer.m_HDR.GetHeight();
        imageCopyRegion.extent.depth = 1;

        vkCmdCopyImage(cmdBuf1, m_magnifierPS.GetPassOutputResource(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_GBuffer.m_HDR.Resource(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);

        barriers[0] = barrier;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].image = m_GBuffer.m_HDR.Resource();

        barriers[1] = barrier;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[1].image = m_magnifierPS.GetPassOutputResource();

        vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 2, barriers);
    }

    // If using FreeSync HDR we need to to the tonemapping in-place and then apply the GUI, later we'll apply the color conversion into the swapchain
    //
    if (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR && pState->SelectedTonemapperIndex > 0)
    {
        // In place Tonemapping ------------------------------------------------------------------------
        //
        {
            SetPerfMarkerBegin(cmdBuf1, "Tonemapping");
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = NULL;
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.image = m_GBuffer.m_HDR.Resource();
                vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            }

            m_toneMappingCS.Draw(cmdBuf1, m_GBuffer.m_HDRSRV, pState->UnusedExposure, pState->SelectedTonemapperIndex - 1, m_Width, m_Height);

            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = NULL;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // we need to read from it for the post-processing
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.image = m_GBuffer.m_HDR.Resource();
                vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            }

            m_GPUTimer.GetTimeStamp(cmdBuf1, "Tonemapping");

            SetPerfMarkerEnd(cmdBuf1);
        }
    }
    // submit command buffer
    {
        VkResult res = vkEndCommandBuffer(cmdBuf1);
        assert(res == VK_SUCCESS);

        VkSubmitInfo submit_info;
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = NULL;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = NULL;
        submit_info.pWaitDstStageMask = NULL;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmdBuf1;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = NULL;
        res = vkQueueSubmit(m_pDevice->GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);
        assert(res == VK_SUCCESS);
    }

    // Wait for swapchain (we are going to render to it) -----------------------------------
    //
    int imageIndex = pSwapChain->WaitForSwapChain();

    m_CommandListRing.OnBeginFrame();

    VkCommandBuffer cmdBuf2 = m_CommandListRing.GetNewCommandList();
    {
        VkCommandBufferBeginInfo cmd_buf_info;
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmd_buf_info.pInheritanceInfo = NULL;
        VkResult res = vkBeginCommandBuffer(cmdBuf2, &cmd_buf_info);
        assert(res == VK_SUCCESS);
    }

    SetPerfMarkerBegin(cmdBuf2, "rendering to swap chain");

    // prepare render pass
    {
        VkRenderPassBeginInfo rp_begin = {};
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.pNext = NULL;
        rp_begin.renderPass = pSwapChain->GetRenderPass();
        rp_begin.framebuffer = pSwapChain->GetFramebuffer(imageIndex);
        rp_begin.renderArea.offset.x = 0;
        rp_begin.renderArea.offset.y = 0;
        rp_begin.renderArea.extent.width = m_Width;
        rp_begin.renderArea.extent.height = m_Height;
        rp_begin.clearValueCount = 0;
        rp_begin.pClearValues = NULL;
        vkCmdBeginRenderPass(cmdBuf2, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    }

    vkCmdSetScissor(cmdBuf2, 0, 1, &m_rectScissor);
    vkCmdSetViewport(cmdBuf2, 0, 1, &m_viewport);

    // Tonemapping LPM ------------------------------------------------------------------------
    //
    if (pState->SelectedTonemapperIndex == 0)
    {
        m_lpmPS.Draw(cmdBuf2, m_GBuffer.m_HDRSRV);
        m_GPUTimer.GetTimeStamp(cmdBuf2, "Tone mapping LPM");
    }
    else
    {
        if (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR)
        {
            // FS2 mode! Apply color conversion now.
            //
            m_colorConversionPS.Draw(cmdBuf2, m_GBuffer.m_HDRSRV);
            m_GPUTimer.GetTimeStamp(cmdBuf2, "Color conversion");
        }
        else
        {
            // non FS2 mode, that is SDR, here we apply the tonemapping from the HDR into the swapchain
            //
            // Tonemapping ------------------------------------------------------------------------
            //
            {
                m_toneMappingPS.Draw(cmdBuf2, m_GBuffer.m_HDRSRV, pState->UnusedExposure, pState->SelectedTonemapperIndex - 1);
                m_GPUTimer.GetTimeStamp(cmdBuf2, "Tone mapping");
            }
        }
    }

    SetPerfMarkerEnd(cmdBuf2);

    m_GPUTimer.OnEndFrame();

    vkCmdEndRenderPass(cmdBuf2);

    // Close & Submit the command list ----------------------------------------------------
    //
    {
        VkResult res = vkEndCommandBuffer(cmdBuf2);
        assert(res == VK_SUCCESS);

        VkSemaphore ImageAvailableSemaphore;
        VkSemaphore RenderFinishedSemaphores;
        VkFence CmdBufExecutedFences;
        pSwapChain->GetSemaphores(&ImageAvailableSemaphore, &RenderFinishedSemaphores, &CmdBufExecutedFences);

        VkPipelineStageFlags submitWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info2;
        submit_info2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info2.pNext = NULL;
        submit_info2.waitSemaphoreCount = 1;
        submit_info2.pWaitSemaphores = &ImageAvailableSemaphore;
        submit_info2.pWaitDstStageMask = &submitWaitStage;
        submit_info2.commandBufferCount = 1;
        submit_info2.pCommandBuffers = &cmdBuf2;
        submit_info2.signalSemaphoreCount = 1;
        submit_info2.pSignalSemaphores = &RenderFinishedSemaphores;

        res = vkQueueSubmit(m_pDevice->GetGraphicsQueue(), 1, &submit_info2, CmdBufExecutedFences);
        assert(res == VK_SUCCESS);
    }
}
