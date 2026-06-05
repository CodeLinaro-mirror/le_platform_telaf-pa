project(TAF_PA_COMMON CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_pa_common)
set(OUTPUT_NAME Component_${TARGET_NAME})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
add_subdirectory(stub)

add_library(${TARGET_NAME} SHARED tafCommonPa.cpp)
set_target_properties(${TARGET_NAME} PROPERTIES
  VERSION ${LIBRARY_VERSION}
  SOVERSION ${LIBRARY_SOVERSION}
  OUTPUT_NAME ${OUTPUT_NAME}
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/stub/include)
target_link_libraries(${TARGET_NAME} PRIVATE taf_prop_common taf_ns_common)
target_link_libraries(${TARGET_NAME} PRIVATE dlt)

install(TARGETS ${TARGET_NAME} LIBRARY DESTINATION lib)