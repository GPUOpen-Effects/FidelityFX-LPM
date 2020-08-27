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
#include <fstream>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR 1
#include "vulkan/vulkan.h"


// we are using DirectXMath
#include <DirectXMath.h>
using namespace DirectX;

// TODO: reference additional headers your program requires here
#include "base\Imgui.h"
#include "Base\ImguiHelper.h"
#include "base\Texture.h"
#include "base\Device.h"
#include "base\SwapChain.h"
#include "base\ExtFreeSyncHDR.h"
#include "Base\FreeSyncHDR.h"
#include "Base\Helper.h"
#include "base\UploadHeap.h"
#include "base\GPUTimeStamps.h"
#include "Base\ExtDebugMarkers.h"
#include "base\CommandListRing.h"
#include "base\StaticBufferPool.h"
#include "base\DynamicBufferRing.h"
#include "base\ResourceViewHeaps.h"
#include "Base\ShaderCompilerHelper.h"

#include "Misc\Error.h"
#include "misc\Misc.h"
#include "misc\Camera.h"

#include "GLTF\GltfPbrPass.h"
#include "GLTF\GltfBBoxPass.h"
#include "GLTF\GltfDepthPass.h"

#include "PostProc\Bloom.h"
#include "PostProc\BlurPS.h"
#include "PostProc\SkyDome.h"
#include "PostProc\SkyDomeProc.h"
#include "PostProc\DownSamplePS.h"
#include "PostProc\ColorConversionPS.h"
#include "PostProc\ToneMapping.h"
#include "PostProc\ToneMappingCS.h"

#include "TestImages.h"
#include "FlipBookAnimation.h"
#include "LPMPS.h"
#include "ExposureMultiplierCS.h"

#include "Widgets\Axis.h"
#include "Widgets\CheckerBoardFloor.h"

#include "..\common\Misc\Camera.h"
#include "..\common\Misc\FrameworkWindows.h"
#include "..\common\Misc\ColorConversion.h"

using namespace CAULDRON_VK;