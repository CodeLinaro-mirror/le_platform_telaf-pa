cmake_minimum_required(VERSION 3.16)
project(taf_prop_net LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_prop_net)
set(OUTPUT_NAME Component_${TARGET_NAME})

add_library(${TARGET_NAME} SHARED taf_prop_net.cpp)

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION ${LIBRARY_VERSION}
    SOVERSION ${LIBRARY_SOVERSION}
    OUTPUT_NAME ${OUTPUT_NAME}
)

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET_NAME} PRIVATE taf_pa_common)

install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION lib
)

