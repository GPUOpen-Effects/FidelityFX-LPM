include(${CMAKE_CURRENT_SOURCE_DIR}/../../common.cmake)

add_library(LPMSample_Common INTERFACE)

set(config
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/LPMSample.json
)

copyTargetCommand("${config}" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} copied_common_config)
add_dependencies(LPMSample_Common copied_common_config)
