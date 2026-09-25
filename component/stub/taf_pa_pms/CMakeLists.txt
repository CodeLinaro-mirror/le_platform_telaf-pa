cmake_minimum_required(VERSION 3.15)

add_library(taf_prop_pms SHARED taf_prop_pms.cpp)

target_include_directories(
    taf_prop_pms
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(taf_prop_pms PRIVATE taf_pa_common)

set_target_properties(
    taf_prop_pms
    PROPERTIES OUTPUT_NAME Component_taf_prop_pms)

install(
    TARGETS
        taf_prop_pms
        LIBRARY DESTINATION lib
)