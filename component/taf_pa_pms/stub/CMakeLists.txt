cmake_minimum_required(VERSION 3.15)

add_library(taf_ns_pa_pms SHARED taf_ns_pa_pms.cpp)

target_include_directories(
    taf_ns_pa_pms
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../taf_pa_common/stub/include
)

target_link_libraries(taf_ns_pa_pms PRIVATE taf_pa_common)

set_target_properties(
    taf_ns_pa_pms
    PROPERTIES OUTPUT_NAME Component_taf_ns_pa_pms)

install(
    TARGETS
        taf_ns_pa_pms
        LIBRARY DESTINATION lib
)
