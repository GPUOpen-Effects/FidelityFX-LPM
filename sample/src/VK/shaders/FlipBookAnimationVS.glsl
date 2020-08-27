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

#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
 
layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 outTexCoord;

layout (std140, binding = 0) uniform perFrame
{
    uint row;
    uint cols;
    float time;
    float angle;
    vec4 camPos;
    mat4 worldMat;
    mat4 viewProjMat;
} myPerFrame;

void main()
{
    float cos_angle = cos(myPerFrame.angle);
    float sin_angle = sin(myPerFrame.angle);

    mat4 rotMatrix = mat4(cos_angle, 0, sin_angle, 0,
                          0,         1, 0,         0,
                         -sin_angle, 0, cos_angle, 0,
                          0,         0, 0,         1);

    vec4 newPos = rotMatrix * pos;

    gl_Position = myPerFrame.viewProjMat * myPerFrame.worldMat * newPos;

    vec2 size = vec2(1.0f / myPerFrame.cols, 1.0f / myPerFrame.row);
    uint totalFrames = myPerFrame.row * myPerFrame.cols;

    // use timer to increment index
    uint index = uint(myPerFrame.time * 60) % totalFrames;

    // wrap x and y indexes
    uint indexX = uint(index % myPerFrame.cols);
    uint indexY = uint(index / myPerFrame.cols);

    // get offsets to our sprite index
    vec2 offset = vec2(size.x*indexX, size.y*indexY);

    // get single sprite UV
	vec2 newUV = inTexCoord * size;

    // Slight offset in Y because of visual seam
    newUV.y += 0.001;

    outTexCoord = newUV + offset;
}