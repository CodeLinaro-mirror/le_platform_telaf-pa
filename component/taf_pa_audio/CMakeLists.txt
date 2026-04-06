cmake_minimum_required(VERSION 3.16)
project(taf_pa_audio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_pa_audio)
set(OUTPUT_NAME Component_${TARGET_NAME})

set(PKG_CONFIG_SYSROOT_DIR $ENV{PKG_CONFIG_SYSROOT_DIR})
set(TELUX_AUDIO_PATH "${PKG_CONFIG_SYSROOT_DIR}/usr/lib/libtelux_audio.so")

if(EXISTS ${TELUX_AUDIO_PATH})
    message(STATUS "libtelux_audio.so exists")
    set(TELUX_AUDIO_SUPPORTED ON)
else()
    message(STATUS "libtelux_audio.so NOT found")
    set(TELUX_AUDIO_SUPPORTED OFF)
endif()

if(TELUX_AUDIO_SUPPORTED)
    # common headers
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_library(${TARGET_NAME} SHARED taf_pa_audio.cpp)

    # link glib-2.0
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
    target_link_libraries(${TARGET_NAME} PRIVATE PkgConfig::GLIB)

    set_target_properties(${TARGET_NAME} PROPERTIES
        VERSION ${LIBRARY_VERSION}
        SOVERSION ${LIBRARY_SOVERSION}
        OUTPUT_NAME ${OUTPUT_NAME}
    )

    target_link_libraries(${TARGET_NAME} PRIVATE telux_common telux_audio telux_tel)

    target_link_libraries(${TARGET_NAME} PRIVATE taf_pa_common)

    install(TARGETS ${TARGET_NAME}
        LIBRARY DESTINATION lib
    )
else()
    message(STATUS "taf_pa_audio.so not available")
endif()
