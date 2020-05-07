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

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <windowsx.h>


// C RunTime Header Files
#include <malloc.h>
#include <map>
#include <mutex>
#include <vector>
#include <fstream>
#include "..\..\libs\d3d12x\d3dx12.h"

// we are using DirectXMath
#include <DirectXMath.h>
using namespace DirectX;

// TODO: reference additional headers your program requires here
#include "base\Imgui.h"
#include "Base\ImguiHelper.h"
#include "base\Fence.h"
#include "Base\Helper.h"
#include "Base\Device.h"
#include "base\Freesync2.h"
#include "base\Texture.h"
#include "base\SwapChain.h"
#include "base\UploadHeap.h"
#include "Base\UserMarkers.h"
#include "base\GPUTimestamps.h"
#include "base\CommandListRing.h"
#include "base\ShaderCompilerHelper.h"
#include "base\StaticBufferPool.h"
#include "base\DynamicBufferRing.h"
#include "base\ResourceViewHeaps.h"
#include "base\StaticConstantBufferPool.h"

#include "Misc\Misc.h"
#include "Misc\Error.h"
#include "Misc\Camera.h"
#include "Misc\FrameworkWindows.h"
#include "Misc\ColorConversion.h"

#include "GLTF\GltfPbrPass.h"
#include "GLTF\GltfDepthPass.h"
#include "GLTF\GltfBBoxPass.h"

#include "PostProc\ColorConversionPS.h"
#include "PostProc\ToneMapping.h"
#include "PostProc\ToneMappingCS.h"
#include "PostProc\DownSamplePS.h"
#include "PostProc\SkyDome.h"
#include "PostProc\SkyDomeProc.h"
#include "PostProc\BlurPS.h"
#include "PostProc\Bloom.h"

#include "TestImages.h"
#include "FlipBookAnimation.h"
#include "LPMPS.h"
#include "ExposureMultiplierCS.h"

#include "Widgets\wireframe.h"
using namespace CAULDRON_DX12;