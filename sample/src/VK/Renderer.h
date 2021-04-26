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
#pragma once

#include "stdafx.h"

#include "base/GBuffer.h"
#include "PostProc/MagnifierPS.h"

// We are queuing (backBufferCount + 0.5) frames, so we need to triple buffer the resources that get modified each frame
static const int backBufferCount = 3;

using namespace CAULDRON_VK;

struct UIState;

//
// Renderer class is responsible for rendering resources management and recording command buffers.
class Renderer
{
public:

    void OnCreate(Device *pDevice, SwapChain *pSwapChain, float fontSize);
    void OnDestroy();

    void OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height);
    void OnDestroyWindowSizeDependentResources();

    void OnUpdateDisplayDependentResources(SwapChain *pSwapChain, const UIState *pState);

    void OnUpdateLocalDimmingChangedResources(SwapChain *pSwapChain, const UIState *pState);

    int LoadScene(GLTFCommon *pGLTFCommon, int stage = 0);
    void UnloadScene();

    void AllocateShadowMaps(GLTFCommon* pGLTFCommon);

    const std::vector<TimeStamp> &GetTimingValues() { return m_TimeStamps; }
    const std::string*& GetScreenshotFileNamePointer() { return m_pScreenShotName; }

    void OnRender(const UIState* pState, const Camera& cam, SwapChain* pSwapChain, float time);

private:
    Device *m_pDevice;

    uint32_t                        m_Width;
    uint32_t                        m_Height;

    VkRect2D                        m_rectScissor;
    VkViewport                      m_viewport;

    // Initialize helper classes
    ResourceViewHeaps               m_resourceViewHeaps;
    UploadHeap                      m_UploadHeap;
    DynamicBufferRing               m_ConstantBufferRing;
    StaticBufferPool                m_VidMemBufferPool;
    StaticBufferPool                m_SysMemBufferPool;
    CommandListRing                 m_CommandListRing;
    GPUTimestamps                   m_GPUTimer;

    //gltf passes
    GltfPbrPass                    *m_gltfPBR;
    GltfDepthPass                  *m_gltfDepth;
    GLTFTexturesAndBuffers         *m_pGLTFTexturesAndBuffers;

    // effects
    Bloom                           m_bloom;
    DownSamplePS                    m_downSample;
    ToneMapping                     m_toneMappingPS;
    ToneMappingCS                   m_toneMappingCS;
    ColorConversionPS               m_colorConversionPS;
    MagnifierPS                     m_magnifierPS;

    // GUI
    ImGUI                           m_ImGUI;

    // GBuffer and render passes
    GBuffer                         m_GBuffer;
    GBufferRenderPass               m_renderPassFullGBufferWithClear;
    GBufferRenderPass               m_renderPassJustDepthAndHdr;
    GBufferRenderPass               m_renderPassFullGBuffer;

    // shadowmaps
    typedef struct {
        Texture         ShadowMap;
        uint32_t        ShadowIndex;
        uint32_t        ShadowResolution;
        uint32_t        LightIndex;
        VkImageView     ShadowDSV;
        VkFramebuffer   ShadowFrameBuffer;
    } SceneShadowInfo;

    std::vector<SceneShadowInfo>    m_shadowMapPool;
    std::vector< VkImageView>       m_ShadowSRVPool;

    VkRenderPass                    m_render_pass_shadow;

    std::vector<TimeStamp>          m_TimeStamps;

    // screen shot
    const std::string              *m_pScreenShotName = NULL;
    AsyncPool                       m_asyncPool;

    FlipBookAnimation               m_CampfireAnimation;
    TestImages                      m_testImages;
    ExposureMultiplierCS            m_exposureMultiplierCS;
    LPMPS                           m_lpmPS;

    VkRenderPass                    m_render_pass_TestPattern;
    VkFramebuffer                   m_pFrameBuffer_TestPattern;
};

