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

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outColor;

layout (set=0, binding = 0, std140) uniform perFrame
{
	int           u_testPattern;
} myPerFrame;

layout(set=0, binding=1) uniform sampler2D sSampler;

void main() 
{
    vec4 texColor = vec4(1.0,1.0,1.0,1.0);
	switch (myPerFrame.u_testPattern)
    {
        case 0:
        case 1:
        case 2:
        {
            texColor = texture(sSampler, inTexCoord.st);
            break;
        }
        case 3:
        {
            // Get rgb for hue value from x coord of uv
            float H = inTexCoord.x * 360.0f;
            float S = 1.0f;
            float V = 1.0f;

            float C = V * S;
            float X = C * (1.0f - abs(mod((H / 60.0f) , 2.0f) - 1.0f));
            float m = V - C;

            vec3 absColor;
            if (H >= 0.0f && H < 60.0f)
            {
                absColor = vec3(C, X, 0.0f);
            }
            else if (H >= 60.0f && H < 120.0f)
            {
                absColor = vec3(X, C, 0.0f);
            }
            else if (H >= 120.0f && H < 180.0f)
            {
                absColor = vec3(0.0f, C, X);
            }
            else if (H >= 180.0f && H < 240.0f)
            {
                absColor = vec3(0.0f, X, C);
            }
            else if (H >= 240.0f && H < 300.0f)
            {
                absColor = vec3(X, 0.0f, C);
            }
            else if (H >= 300.0f && H < 360.0f)
            {
                absColor = vec3(C, 0.0f, X);
            }

            texColor.xyz = vec3(absColor.r + m, absColor.g + m, absColor.b + m);

            if (inTexCoord.y < 0.5)
            {
                texColor.xyz *= (inTexCoord.y * 2.0);
            }
            else
            {
                float exp = mix(0, 6, (inTexCoord.y - 0.5) * 2);
                texColor.xyz *= pow(2, exp);
            }
            break;
        }
    }

    outColor = texColor;
}
