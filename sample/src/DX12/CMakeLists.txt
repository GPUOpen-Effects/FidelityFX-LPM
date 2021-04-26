project (LPMSample_${GFX_API})

set(sources
    LPMSample.cpp
    LPMSample.h
    Renderer.cpp
    Renderer.h
    UI.cpp
    UI.h
    TestImages.cpp
    TestImages.h
    FlipBookAnimation.cpp
    FlipBookAnimation.h
    ExposureMultiplierCS.h
    ExposureMultiplierCS.cpp
    LPMPS.cpp
    LPMPS.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-lpm/ffx_a.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-lpm/ffx_lpm.h
    stdafx.cpp
    stdafx.h
    dpiawarescaling.manifest)

set(LPM_Shaders_src
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/TestImagesPS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/FlipBookAnimationVS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/FlipBookAnimationPS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/transferFunction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ExposureMultiplierCS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/LPMPS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-lpm/ffx_a.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-lpm/ffx_lpm.h)

set(JSON_Data_src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/LPMSample.json)

source_group("Sources"          FILES ${sources})
source_group("LPM_Shaders_src"  FILES ${LPM_Shaders_src})
source_group("Icon"             FILES ${icon_src}) # defined in top-level CMakeLists.txt
source_group("JSON_Data"        FILES ${JSON_Data_src})

add_executable(${PROJECT_NAME} WIN32 ${sources} ${LPM_Shaders_src} ${icon_src} ${JSON_Data_src})
target_link_libraries (${PROJECT_NAME} LINK_PUBLIC LPMSample_Common Cauldron_DX12 ImGUI amd_ags d3dcompiler D3D12)
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")

set_source_files_properties(${LPM_Shaders_src} PROPERTIES VS_TOOL_OVERRIDE "Text")

function(copyCommand list dest)
    foreach(fullFileName ${list})
        get_filename_component(file ${fullFileName} NAME)
        message("Generating custom command for ${fullFileName}")
        add_custom_command(
            OUTPUT   ${dest}/${file}
            PRE_BUILD
            COMMAND cmake -E make_directory ${dest}
            COMMAND cmake -E copy ${fullFileName}  ${dest}
            MAIN_DEPENDENCY  ${fullFileName}
            COMMENT "Updating ${file} into ${dest}"
        )
    endforeach()
endfunction()

copyCommand("${LPM_Shaders_src}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX)