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
    int           u_testPattern;
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
Texture2D        sceneTexture     :register(t0);
SamplerState     samLinearWrap    :register(s0);

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
float4 mainPS(VERTEX Input) : SV_Target
{
    float3 color;

    switch (u_testPattern)
    {
        case 0:
        case 1:
        case 2:
        {
            color = sceneTexture.Sample(samLinearWrap, Input.vTexcoord).xyz;

            break;
        }
        case 3:
        {
            // Get rgb for hue value from x coord of uv
            float H = Input.vTexcoord.x * 360.0f;
            float S = 1.0f;
            float V = 1.0f;

            float C = V * S;
            float X = C * (1.0f - abs(((H / 60.0f) % 2) - 1.0f));
            float m = V - C;

            float3 absColor;
            if (H >= 0.0f && H < 60.0f)
            {
                absColor = float3(C, X, 0.0f);
            }
            else if (H >= 60.0f && H < 120.0f)
            {
                absColor = float3(X, C, 0.0f);
            }
            else if (H >= 120.0f && H < 180.0f)
            {
                absColor = float3(0.0f, C, X);
            }
            else if (H >= 180.0f && H < 240.0f)
            {
                absColor = float3(0.0f, X, C);
            }
            else if (H >= 240.0f && H < 300.0f)
            {
                absColor = float3(X, 0.0f, C);
            }
            else if (H >= 300.0f && H < 360.0f)
            {
                absColor = float3(C, 0.0f, X);
            }

            color = float3(absColor.r + m, absColor.g + m, absColor.b + m);

            if (Input.vTexcoord.y < 0.5)
            {
                color *= (Input.vTexcoord.y * 2.0);
            }
            else
            {
                float exp = lerp(0, 6, (Input.vTexcoord.y - 0.5) * 2);
                color *= pow(2, exp);
            }

            break;
        }
    }

    return float4(color, 1.0f);
}
