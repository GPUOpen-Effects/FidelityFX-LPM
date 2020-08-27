project (LPMSample_${GFX_API})

set(sources
        LPMSample.cpp
        LPMSample.h
        SampleRenderer.cpp
        SampleRenderer.h
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
        stdafx.h)

set(LPM_Shaders_src
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/TestImagesPS.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/FlipBookAnimationVS.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/FlipBookAnimationPS.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/transferFunction.h
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ExposureMultiplierCS.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/LPMPS.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-lpm/ffx_a.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-lpm/ffx_lpm.h)

source_group("Sources"                  FILES ${sources})
source_group("LPM_Shaders_src"          FILES ${LPM_Shaders_src})

add_executable(${PROJECT_NAME} WIN32 ${sources} ${LPM_Shaders_src})
target_link_libraries (${PROJECT_NAME} LINK_PUBLIC Cauldron_VK ImGUI Vulkan::Vulkan)
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

copyCommand("${LPM_Shaders_src}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibVK)