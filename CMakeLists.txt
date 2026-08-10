cmake_minimum_required(VERSION 3.10)
project(telaf_pa_all)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

# Strict type-safety flags to catch unsafe enum/integer comparisons.
#
# -Wenum-compare / -Werror=enum-compare
#   Turns comparing two values of different enumerated types (e.g. taf_prop_result_t
#   vs taf_pa_result_t) into a hard error.  Prevents accidentally mixing the two
#   result-code enums in a comparison.
#
# -Wsign-compare / -Werror=sign-compare
#   Turns signed/unsigned integer comparison warnings into hard errors.
#
# Note: comparing an enum variable to a raw integer literal (e.g. result == 0 or
# result == -ENOSYS) is not caught by these flags in C++.  To enforce that at
# compile time the enum types must be changed to "enum class".
#
# Note: -Wold-style-cast / -Werror=old-style-cast would also catch C-style casts
# between enum types (e.g. (taf_pa_result_t)result), but it is intentionally
# omitted here because the DLT logging infrastructure in taf_pa_common uses
# legitimate C-style casts that would require invasive changes to fix.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -Wenum-compare -Werror=enum-compare \
    -Wsign-compare -Werror=sign-compare")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/component/include)

# Add default RPATH
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN:")

set(MODULE_LIST
    component/stub
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
    component/taf_pa_sensor
    component/taf_pa_audio
    component/taf_pa_ecall
    component/taf_pa_health
    component/taf_pa_pms
    component/taf_pa_sim
    component/taf_pa_therm
    component/taf_pa_keystore
    component/taf_pa_fscrypt
    # IMPORTANT: split_debug_symbols_strip_binaries must be the last module in this list.
    # This module extracts debug symbols from all installed binaries and strips them.
    # It must run after all other components have installed their files, otherwise
    # it would process an incomplete set of binaries. CMake processes install()
    # commands in the order modules are added, so placing this module last ensures
    # the debug symbol extraction happens after all component files are installed.
    component/split_debug_symbols_strip_binaries
)

foreach(module ${MODULE_LIST})
    message(STATUS "Including module: ${module}")
    add_subdirectory(${module})
endforeach()
