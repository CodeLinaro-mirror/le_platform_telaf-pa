cmake_minimum_required(VERSION 3.16)
project(taf_pa_mrc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_pa_mrc)
set(OUTPUT_NAME Component_${TARGET_NAME})

add_subdirectory(stub)

add_library(${TARGET_NAME} SHARED tafMrcPa.cpp)

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION ${LIBRARY_VERSION}
    SOVERSION ${LIBRARY_SOVERSION}
    OUTPUT_NAME ${OUTPUT_NAME}
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/stub/include)

target_link_libraries(${TARGET_NAME} PRIVATE telux_platform)
target_link_libraries(${TARGET_NAME} PRIVATE taf_pa_common)
target_link_libraries(${TARGET_NAME} PRIVATE taf_prop_mrc)

install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION lib
)

