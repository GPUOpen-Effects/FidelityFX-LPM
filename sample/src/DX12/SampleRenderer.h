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

static const int backBufferCount = 3;

#define USE_VID_MEM true
#define USE_SHADOWMASK false

using namespace CAULDRON_DX12;

//
// This class deals with the GPU side of the sample.
//
class SampleRenderer
{
public:

    struct State
    {
        float time;
        Camera camera;

        float exposure;
        float unusedExposure;
        float emmisiveFactor;

        int testPattern;
        ColorSpace colorSpace;
        int toneMapper;
        float lightIntensity;
    };

    void OnCreate(Device* pDevice, SwapChain *pSwapChain);
    void OnDestroy();

    void OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, uint32_t Width, uint32_t Height, State *pState);
    void OnDestroyWindowSizeDependentResources();

    int LoadScene(GLTFCommon *pGLTFCommon, int stage = 0);
    void UnloadScene();

    const std::vector<TimeStamp> &GetTimingValues() { return m_TimeStamps; }

    void OnRender(State *pState, SwapChain *pSwapChain);

private:
    Device *m_pDevice;

    uint32_t m_Width;
    uint32_t m_Height;

    D3D12_VIEWPORT m_ViewPort;
    D3D12_RECT m_RectScissor;
   
    // Initialize helper classes
    ResourceViewHeaps m_resourceViewHeaps;
    UploadHeap m_UploadHeap;
    DynamicBufferRing m_ConstantBufferRing;
    StaticBufferPool m_VidMemBufferPool;
    CommandListRing m_CommandListRing;
    GPUTimestamps m_GPUTimer;
    
    //gltf passes
    GLTFTexturesAndBuffers *m_pGLTFTexturesAndBuffers;
    GltfPbrPass *m_gltfPBR;
    GltfDepthPass *m_gltfDepth;
    
    FlipBookAnimation m_CampfireAnimation;
    
    // effects
    Bloom m_bloom;
    DownSamplePS m_downSample;
    TestImages m_testImages;
    ExposureMultiplierCS m_exposureMultiplierCS;
    ToneMapping m_toneMappingPS;
    ToneMappingCS m_toneMappingCS;
    ColorConversionPS m_colorConversionPS;
    LPMPS m_lpmPS;
    
    // GUI
    ImGUI m_ImGUI;
    
    // Temporary render targets
    
    // depth buffer
    Texture m_depthBuffer;
    DSV m_DepthBufferDSV;

    // shadowmap
    Texture m_shadowMap;
    CBV_SRV_UAV m_ShadowMapSRV;
    DSV m_ShadowMapDSV;

    // MSAA RT
    Texture m_HDRMSAA;
    RTV m_HDRRTVMSAA;

    // Resolved RT
    Texture m_HDR;
    CBV_SRV_UAV m_HDRSRV;
    CBV_SRV_UAV m_HDRUAV;
    RTV m_HDRRTV;

    std::vector<TimeStamp> m_TimeStamps;
};

