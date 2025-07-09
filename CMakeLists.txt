cmake_minimum_required(VERSION 3.10)
project(telaf_pa_all)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add default RPATH
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN:")

set(MODULE_LIST
    component/taf_pa_common
    component/taf_pa_voicecall
    component/taf_pa_sms
    component/taf_pa_deviceinfo
    component/taf_pa_time
    component/taf_pa_radio
    component/taf_pa_mrc
    component/taf_pa_flash
    component/taf_pa_wlan
    component/taf_pa_data
    component/taf_pa_location
    component/taf_pa_net
)

foreach(module ${MODULE_LIST})
    message(STATUS "Including module: ${module}")
    add_subdirectory(${module})
endforeach()
