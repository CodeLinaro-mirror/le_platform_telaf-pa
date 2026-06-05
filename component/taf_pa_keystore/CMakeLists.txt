cmake_minimum_required(VERSION 3.16)
project(taf_pa_keystore LANGUAGES C)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_pa_keystore)
set(OUTPUT_NAME Component_${TARGET_NAME})

add_subdirectory(stub)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include/keyStore)

add_library(${TARGET_NAME} SHARED tafKeystorePa.c)

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION ${LIBRARY_VERSION}
    SOVERSION ${LIBRARY_SOVERSION}
    OUTPUT_NAME ${OUTPUT_NAME}
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/stub/include)

target_link_libraries(${TARGET_NAME} PRIVATE taf_pa_common)
target_link_libraries(${TARGET_NAME} PRIVATE taf_prop_keystore)

install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION lib
)

