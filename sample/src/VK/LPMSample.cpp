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

#include "LPMSample.h"


LPMSample::LPMSample(LPCSTR name) : FrameworkWindows(name)
{
    m_lastFrameTime = MillisecondsNow();
    m_time = 0;
    m_bPlay = true;

    m_pGltfLoader = NULL;
    m_previousDisplayMode = m_currentDisplayMode = m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex = DISPLAYMODE_SDR;
    m_enableLocalDimming = true;
}

//--------------------------------------------------------------------------------------
//
// OnParseCommandLine
//
//--------------------------------------------------------------------------------------
void LPMSample::OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight, bool* pbFullScreen)
{
    // set some default values
    *pWidth = 1920;
    *pHeight = 1080;
    *pbFullScreen = false;
}

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void LPMSample::OnCreate(HWND hWnd)
{
    // Create Device
    //
    m_device.OnCreate("myapp", "myEngine", m_isCpuValidationLayerEnabled, m_isGpuValidationLayerEnabled, hWnd);
    m_device.CreatePipelineCache();

    //init the shader compiler
    CreateShaderCache();

    // Create Swapchain
    //
    uint32_t dwNumberOfBackBuffers = 2;
    m_swapChain.OnCreate(&m_device, dwNumberOfBackBuffers, hWnd);

    // Create a instance of the renderer and initialize it, we need to do that for each GPU
    //
    m_Node = new SampleRenderer();
    m_Node->OnCreate(&m_device, &m_swapChain);

    // init GUI (non gfx stuff)
    //
    ImGUI_Init((void *)hWnd);

    // init GUI state
    m_state.testPattern = 0;
    m_state.colorSpace = ColorSpace::ColorSpace_REC709;
    m_state.toneMapper = 6;
    m_state.exposure = 0.5f;
    m_state.unusedExposure = 1.0f;
    m_state.emmisiveFactor = 1.0f;
    m_state.camera.LookAt(10.0f, 0.0f, 3.5f, XMVectorSet(-52.0f, -26.0f, -47.0f, 0));
    m_state.lightIntensity = 10.0f;

    m_swapChain.EnumerateDisplayModes(&m_displayModesAvailable, &m_displayModesNamesAvailable);
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void LPMSample::OnDestroy()
{
    ImGUI_Shutdown();

    m_device.GPUFlush();

    // Set display mode to SDR before quitting.
    m_previousDisplayMode = m_currentDisplayMode = m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex = DISPLAYMODE_SDR;

    // Fullscreen state should always be false before exiting the app.
    SetFullScreen(false);

    m_Node->UnloadScene();
    m_Node->OnDestroyWindowSizeDependentResources();
    m_Node->OnDestroy();

    delete m_Node;

    m_swapChain.OnDestroyWindowSizeDependentResources();
    m_swapChain.OnDestroy();
    
    //shut down the shader compiler 
    DestroyShaderCache(&m_device);

    if (m_pGltfLoader)
    {
        delete m_pGltfLoader;
        m_pGltfLoader = NULL;
    }

    m_device.DestroyPipelineCache();
    m_device.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnEvent
//
//--------------------------------------------------------------------------------------
bool LPMSample::OnEvent(MSG msg)
{
    if (ImGUI_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
        return true;

    return true;
}

//--------------------------------------------------------------------------------------
//
// SetFullScreen
//
//--------------------------------------------------------------------------------------
void LPMSample::SetFullScreen(bool fullscreen)
{
    m_device.GPUFlush();

    // when going to windowed make sure we are always using SDR
    if ((fullscreen == false) && (m_currentDisplayMode != DISPLAYMODE_SDR))
        m_previousDisplayMode = m_currentDisplayMode = m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex = DISPLAYMODE_SDR;

    m_swapChain.SetFullScreen(fullscreen);
}

//--------------------------------------------------------------------------------------
//
// OnResize
//
//--------------------------------------------------------------------------------------
void LPMSample::OnResize(uint32_t width, uint32_t height)
{
    // Flush GPU
    //
    m_device.GPUFlush();

    // If resizing but no minimizing
    //
    if (m_Width > 0 && m_Height > 0)
    {
        if (m_Node != NULL)
        {
            m_Node->OnDestroyWindowSizeDependentResources();
        }
        m_swapChain.OnDestroyWindowSizeDependentResources();
    }

    m_swapChain.EnumerateDisplayModes(&m_displayModesAvailable, &m_displayModesNamesAvailable);

    m_Width = width;
    m_Height = height;

    // if resizing but not minimizing the recreate it with the new size
    //
    if (m_Width > 0 && m_Height > 0)
    {
        m_swapChain.OnCreateWindowSizeDependentResources(m_Width, m_Height, false, m_currentDisplayMode);
        if (m_Node != NULL)
        {
            m_Node->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height, &m_state);
        }
    }

    m_state.camera.SetFov(XM_PI / 4, m_Width, m_Height, 0.1f, 1000.0f);

}

//--------------------------------------------------------------------------------------
//
// OnActivate
//
//--------------------------------------------------------------------------------------
void LPMSample::OnActivate(bool windowActive)
{
    if (m_previousDisplayMode == DisplayModes::DISPLAYMODE_SDR && m_currentDisplayMode == DisplayModes::DISPLAYMODE_SDR)
        return;

    m_currentDisplayMode = windowActive && m_swapChain.IsFullScreen() ? m_previousDisplayMode : DisplayModes::DISPLAYMODE_SDR;
    m_currentDisplayModeNamesIndex = windowActive && m_swapChain.IsFullScreen() ? m_previousDisplayModeNamesIndex : DisplayModes::DISPLAYMODE_SDR;
    OnResize(m_Width, m_Height);
}

//--------------------------------------------------------------------------------------
//
// OnLocalDimmingChanged
//
//--------------------------------------------------------------------------------------
void LPMSample::OnLocalDimmingChanged()
{
    // Flush GPU
    //
    m_device.GPUFlush();

    fsHdrSetLocalDimmingMode(m_swapChain.GetSwapChain(), m_enableLocalDimming);

    m_Node->OnUpdateLocalDimmingChangedResources(&m_swapChain, &m_state);
}

//--------------------------------------------------------------------------------------
//
// OnRender
//
//--------------------------------------------------------------------------------------
void LPMSample::OnRender()
{
    // Get timings
    //
    double timeNow = MillisecondsNow();
    m_deltaTime = timeNow - m_lastFrameTime;
    m_lastFrameTime = timeNow;

    // Build UI and set the scene state. Note that the rendering of the UI happens later.
    //
    ImGUI_UpdateIO();
    ImGui::NewFrame();

    static int cameraControlSelected = 1;
    static int loadingStage = 0;
    if (loadingStage >= 0)
    {
        // LoadScene needs to be called a number of times, the scene is not fully loaded until it returns -1
        // This is done so we can display a progress bar when the scene is loading
        if (m_pGltfLoader == NULL)
        {
            m_pGltfLoader = new GLTFCommon();
            m_pGltfLoader->Load("..\\media\\campfire\\", "Campfire_scene_groundscaled.gltf");
        }

        loadingStage = m_Node->LoadScene(m_pGltfLoader, loadingStage);
    }
    else
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameBorderSize = 1.0f;

        bool opened = true;
        ImGui::Begin("Stats", &opened);

        if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Resolution       : %ix%i", m_Width, m_Height);
            const std::vector<TimeStamp> timeStamps = m_Node->GetTimingValues();
            if (timeStamps.size() > 0)
            {
                static float values[128];
                values[127] = (float)(timeStamps.back().m_microseconds - timeStamps.front().m_microseconds);
                float average = values[0];
                for (int i = 0; i < 128 - 1; i++) { values[i] = values[i + 1]; average += values[i]; }
                average /= 128;

                ImGui::Text("%-17s:%7.1f FPS", "Framerate", (1.0f / average) * 1000000.0f);
            }
        }

        if (ImGui::CollapsingHeader("Scene Setup", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static float exposureStep = 0.0f;
            ImGui::SliderFloat("Exposure", &exposureStep, -4.0f, +1.0f, NULL, 1.0f);
            m_state.exposure = (float)pow(2, exposureStep);
            ImGui::SliderFloat("Emmisive", &m_state.emmisiveFactor, 0.0f, 2.0f, NULL, 1.0f);
            ImGui::SliderFloat("PointLightIntensity", &m_state.lightIntensity, 0.0f, 20.0f);

            const char * tonemappers[] = { "Timothy", "DX11DSK", "Reinhard", "Uncharted2Tonemap", "ACES", "No tonemapper", "FidelityFX LPM" };
            ImGui::Combo("Tone mapper", &m_state.toneMapper, tonemappers, _countof(tonemappers));

            static bool openWarning = false;

            const char **displayModeNames = &m_displayModesNamesAvailable[0];
            if (ImGui::Combo("Display Mode", (int *)&m_currentDisplayModeNamesIndex, displayModeNames, (int)m_displayModesNamesAvailable.size()))
            {
                if (m_swapChain.IsFullScreen())
                {
                    m_previousDisplayMode = m_currentDisplayMode = m_displayModesAvailable[m_currentDisplayModeNamesIndex];
                    m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex;
                    OnResize(m_Width, m_Height);
                }
                else
                {
                    openWarning = true;
                    m_previousDisplayMode = m_currentDisplayMode = DisplayModes::DISPLAYMODE_SDR;
                    m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex = DisplayModes::DISPLAYMODE_SDR;
                }
            }

            if (openWarning)
            {
                ImGui::OpenPopup("Display Modes Warning");
                ImGui::BeginPopupModal("Display Modes Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("\nChanging display modes is only available in fullscreen, please press ALT + ENTER for fun!\n\n");
                if (ImGui::Button("Cancel", ImVec2(120, 0))) { openWarning = false; ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

            if (m_currentDisplayMode == DisplayModes::DISPLAYMODE_FSHDR_Gamma22 || m_currentDisplayMode == DisplayModes::DISPLAYMODE_FSHDR_SCRGB)
            {
                if (ImGui::Checkbox("Enable Local Dimming", &m_enableLocalDimming))
                {
                    OnLocalDimmingChanged();
                }
            }

            const char * testPatterns[] = { "None", "Luxo Double Checker", "DCI P3 1000 Nits", "REC 2020 1000 Nits", "Rec 709 5000 Nits" };
            if (ImGui::Combo("Test Patterns", &m_state.testPattern, testPatterns, _countof(testPatterns)))
            {
                if (m_state.testPattern == 2) // P3
                    m_state.colorSpace = ColorSpace::ColorSpace_P3;
                else if (m_state.testPattern == 3) // rec2020
                    m_state.colorSpace = ColorSpace::ColorSpace_REC2020;
                else // Rec 709
                    m_state.colorSpace = ColorSpace::ColorSpace_REC709;

                // Flush GPU
                //
                m_device.GPUFlush();

                if (m_Node != NULL)
                {
                    m_Node->OnDestroyWindowSizeDependentResources();
                    m_Node->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height, &m_state);
                }
            }

            const char * cameraControl[] = { "WASD", "Orbit" };
            ImGui::Combo("Camera", &cameraControlSelected, cameraControl, _countof(cameraControl));
        }

        ImGui::End();
    }

    // If the mouse was not used by the GUI then it's for the camera
    //
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse == false)
    {
        float yaw = m_state.camera.GetYaw();
        float pitch = m_state.camera.GetPitch();
        float distance = m_state.camera.GetDistance();

        if ((io.KeyCtrl == false) && (io.MouseDown[0] == true))
        {
            yaw -= io.MouseDelta.x / 100.f;
            pitch += io.MouseDelta.y / 100.f;
        }

        // Choose camera movement depending on setting
        //

        if (cameraControlSelected == 0)
        {
            //  WASD
            //
            m_state.camera.UpdateCameraWASD(yaw, pitch, io.KeysDown, io.DeltaTime);
        }
        else
        {
            //  Orbiting
            //
            distance -= (float)io.MouseWheel / 3.0f;
            distance = std::max<float>(distance, 0.1f);

            bool panning = (io.KeyCtrl == true) && (io.MouseDown[0] == true);

            m_state.camera.UpdateCameraPolar(yaw, pitch, panning ? -io.MouseDelta.x / 100.0f : 0.0f, panning ? io.MouseDelta.y / 100.0f : 0.0f, distance);
        }
    }

    // Set animation time
    //
    if (m_bPlay)
    {
        m_time += (float)m_deltaTime / 1000.0f;
    }

    // transform scene
    //
    if (m_pGltfLoader)
    {
        m_pGltfLoader->SetAnimationTime(0, m_time);
        m_pGltfLoader->TransformScene(0, XMMatrixIdentity());
    }

    m_state.time = m_time;

    // Do Render frame using AFR 
    //
    m_Node->OnRender(&m_state, &m_swapChain);

    m_swapChain.Present();
}


//--------------------------------------------------------------------------------------
//
// WinMain
//
//--------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    LPCSTR Name = "LPMSample VK v1.0";

    // create new Vulkan sample
    return RunFramework(hInstance, lpCmdLine, nCmdShow, new LPMSample(Name));
}

