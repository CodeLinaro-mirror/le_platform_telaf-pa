
cmake_minimum_required(VERSION 3.10.2)

add_library(taf_pa_pms SHARED tafPmsPa.cpp)

add_subdirectory(stub)

target_include_directories(
    taf_pa_pms
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stub/include
)

set_target_properties(taf_pa_pms PROPERTIES OUTPUT_NAME Component_taf_pa_pms)

target_link_libraries(
    taf_pa_pms
    PRIVATE
        telux_common
        telux_power
    PRIVATE
        taf_pa_common
        taf_prop_pa_pms
        )

install(TARGETS taf_pa_pms LIBRARY DESTINATION lib)

