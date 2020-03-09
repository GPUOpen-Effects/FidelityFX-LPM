// AMD Cauldron code
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

//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerFrame : register(b0)
{
    int u_colourSpace;
    int u_displayMode;
    bool u_shoulder;
    float u_pad;
    matrix u_inputToOutputMatrix;
    uint4 u_ctl[24];
}

//--------------------------------------------------------------------------------------
// I/O Structures
//--------------------------------------------------------------------------------------
struct VERTEX
{
    float2 vTexcoord : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
Texture2D        sceneTexture     : register(t0);
SamplerState     samLinearWrap    : register(s0);

#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

uint4 LpmFilterCtl(uint i) { return u_ctl[i]; }

#define LPM_NO_SETUP 1
#include "ffx_lpm.h"

#include "transferFunction.h"

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
float4 mainPS(VERTEX Input) : SV_Target
{
    float4 color = sceneTexture.Sample(samLinearWrap, Input.vTexcoord);

    color = mul(u_inputToOutputMatrix, color);

    // This code is there to make sure no negative values make it down to LPM. Make sure to never hit this case and convert content to correct colourspace
    color.r = max(0, color.r);
    color.g = max(0, color.g);
    color.b = max(0, color.b);
    //

    switch (u_colourSpace)
    {
        // REC 709
        case 0:
        {
            switch (u_displayMode)
            {
            case 0:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_709_709);
                break;

            case 1:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_FS2RAW_709);
                break;

            case 2:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_FS2SCRGB_709);
                break;

            case 3:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_HDR10RAW_709);
                break;

            case 4:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_HDR10SCRGB_709);
                break;
            }
            break;
        }

        // P3
        case 1:
        {
            switch (u_displayMode)
            {
            case 0:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_709_P3);
                break;

            case 1:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_FS2RAW_P3);
                break;

            case 2:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_FS2SCRGB_P3);
                break;

            case 3:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_HDR10RAW_P3);
                break;

            case 4:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_HDR10SCRGB_P3);
                break;
            }
            break;
        }

        // REC2020
        case 2:
        {
            switch (u_displayMode)
            {
            case 0:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_709_2020);
                break;

            case 1:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_FS2RAW_2020);
                break;

            case 2:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_FS2SCRGB_2020);
                break;

            case 3:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_HDR10RAW_2020);
                break;

            case 4:
                LpmFilter(color.r, color.g, color.b, u_shoulder, LPM_CONFIG_HDR10SCRGB_2020);
                break;
            }
            break;
        }
    }

    switch (u_displayMode)
    {
        case 1:
            // FS2_DisplayNative
            // Apply gamma
            color.xyz = ApplyGamma(color.xyz);
            break;

        case 3:
            // HDR10_ST2084
            // Apply ST2084 curve
            color.xyz = ApplyPQ(color.xyz);
            break;
    }

    return color;
}