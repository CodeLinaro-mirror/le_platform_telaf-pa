cmake_minimum_required(VERSION 3.16)
project(taf_pa_voicecall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_pa_voicecall)
set(OUTPUT_NAME Component_${TARGET_NAME})

add_library(${TARGET_NAME} SHARED tafVoicecallPa.cpp)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../stub/include)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# link glib-2.0
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
target_link_libraries(${TARGET_NAME} PRIVATE PkgConfig::GLIB)

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION ${LIBRARY_VERSION}
    SOVERSION ${LIBRARY_SOVERSION}
    OUTPUT_NAME ${OUTPUT_NAME}
)

target_link_libraries(${TARGET_NAME} PRIVATE telux_common telux_tel)

target_link_libraries(${TARGET_NAME} PRIVATE taf_pa_common)

install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION lib
)
