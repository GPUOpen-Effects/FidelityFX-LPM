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

//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerFrame : register(b0)
{
	uint u_row;
	uint u_cols;
	float u_time;
	float u_angle;
	float4 u_camPos;
	matrix u_mWorld;
	matrix u_mCameraViewProj;
};

struct VERTEX_IN
{
	float4 vPosition : POSITION;
	float2 vTexcoord : TEXCOORD;
};

struct VERTEX_OUT
{
	float4 vPosition : SV_POSITION; 
	float2 vTexcoord : TEXCOORD;
}; 

VERTEX_OUT mainVS(VERTEX_IN Input)
{
    VERTEX_OUT Output;

    float cos_angle = cos(u_angle);
    float sin_angle = sin(u_angle);

    matrix rotMatrix = matrix(cos_angle, 0, sin_angle, 0,
                              0,         1, 0,         0,
                             -sin_angle, 0, cos_angle, 0,
                              0,         0, 0,         1);

    float4 newPos = mul(rotMatrix, Input.vPosition);

    Output.vPosition = mul(u_mCameraViewProj, mul(u_mWorld, newPos));

    float2 size = float2(1.0f / u_cols, 1.0f / u_row);
    uint totalFrames = u_row * u_cols;
	
    // use timer to increment index
    uint index = (u_time * 60) % totalFrames;
	
    // wrap x and y indexes
    uint indexX = index % u_cols;
    uint indexY = index / u_cols;
	
    // get offsets to our sprite index
    float2 offset = float2(size.x*indexX, size.y*indexY);
	
    // get single sprite UV
    float2 newUV = Input.vTexcoord * size;
	
    // Slight offset in Y because of visual seam
    newUV.y += 0.001;

    Output.vTexcoord = newUV + offset;

    return Output;
}