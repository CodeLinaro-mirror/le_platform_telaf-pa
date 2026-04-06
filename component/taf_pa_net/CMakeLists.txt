cmake_minimum_required(VERSION 3.16)
project(taf_pa_net LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_pa_net)
set(OUTPUT_NAME Component_${TARGET_NAME})

add_subdirectory(stub)

# -------------------------------------------------------------------------
# Sources
# -------------------------------------------------------------------------
set(TAF_PA_NET_SOURCES
    taf_pa_net.cpp
    taf_pa_vlan.cpp
    taf_pa_socks.cpp
    taf_pa_l2tp.cpp
    taf_pa_nat.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include/taf_pa_net)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../stub/include)


add_library(${TARGET_NAME} SHARED ${TAF_PA_NET_SOURCES})

# link glib-2.0
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
target_link_libraries(${TARGET_NAME} PRIVATE PkgConfig::GLIB)

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION ${LIBRARY_VERSION}
    SOVERSION ${LIBRARY_SOVERSION}
    OUTPUT_NAME ${OUTPUT_NAME}
)


target_link_libraries(${TARGET_NAME}
    PRIVATE telux_common telux_tel telux_data
    PRIVATE taf_pa_common
)
target_link_libraries(${TARGET_NAME} PRIVATE taf_ns_net)

install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION lib
)