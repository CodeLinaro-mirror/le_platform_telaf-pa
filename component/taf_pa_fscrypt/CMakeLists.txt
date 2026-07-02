cmake_minimum_required(VERSION 3.16)
project(taf_pa_fscrypt LANGUAGES C)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_pa_fscrypt)
set(OUTPUT_NAME Component_${TARGET_NAME})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include/keyStore)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_library(${TARGET_NAME} SHARED tafFscryptPa.c)

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION ${LIBRARY_VERSION}
    SOVERSION ${LIBRARY_SOVERSION}
    OUTPUT_NAME ${OUTPUT_NAME}
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../stub/include)

target_link_libraries(${TARGET_NAME} PRIVATE taf_pa_common)
target_link_libraries(${TARGET_NAME} PRIVATE taf_prop_fscrypt)

install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION lib
)
