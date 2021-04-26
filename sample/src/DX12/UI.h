// AMD SampleDX12 sample code
// 
// Copyright(c) 2020 Advanced Micro Devices, Inc.All rights reserved.
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

#include "Misc/ColorConversion.h"
#include "PostProc/MagnifierPS.h"
#include <string>

struct UIState
{
    //
    // WINDOW MANAGEMENT
    //
    bool bShowControlsWindow;
    bool bShowProfilerWindow;

    //
    // POST PROCESS CONTROLS
    //
    int   SelectedTonemapperIndex;
    float Exposure;
    float UnusedExposure;

    bool  bUseMagnifier;
    bool  bLockMagnifierPosition;
    bool  bLockMagnifierPositionHistory;
    int   LockedMagnifiedScreenPositionX;
    int   LockedMagnifiedScreenPositionY;
    CAULDRON_DX12::MagnifierPS::PassParameters MagnifierParams;

    //
    // APP/SCENE CONTROLS
    //
    float EmissiveFactor;

    //
    // PROFILER CONTROLS
    //
    bool  bShowMilliseconds;

    //
    // TEST PATTERN
    //
    int TestPattern;
    ColorSpace ColorSpace;

    //
    // LPM CONFIG
    //
    bool  bShoulder; // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
    float SoftGap; // Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.
    float HdrMax; // Maximum input value.
    float LpmExposure; // Number of stops between 'hdrMax' and 18% mid-level on input.
    float Contrast; // Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
    float ShoulderContrast; // Shoulder shaping, 1.0 = no change (fast path).
    float Saturation[3]; // A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
    float Crosstalk[3]; // One channel must be 1.0, the rest can be <= 1.0 but not zero.

    // -----------------------------------------------

    void Initialize();

    void ToggleMagnifierLock();
    void AdjustMagnifierSize(float increment = 0.05f);
    void AdjustMagnifierMagnification(float increment = 1.00f);

    void ResetLPMSceneDefaults();
};