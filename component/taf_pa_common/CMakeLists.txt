project(TAF_PA_COMMON CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBRARY_VERSION "1.0.0")
set(LIBRARY_SOVERSION "1")
set(TARGET_NAME taf_pa_common)
set(OUTPUT_NAME Component_${TARGET_NAME})

# Default: OFF
option(PA_ENABLE_DLT "Enable DLT logging backend in PA" OFF)

# If DLT_LOGGING is defined, force PA_ENABLE_DLT to ON
if(DEFINED DLT_LOGGING)
  set(PA_ENABLE_DLT ON CACHE BOOL "Enable DLT logging backend in PA" FORCE)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
add_subdirectory(stub)

add_library(${TARGET_NAME} SHARED taf_pa_common.cpp)
set_target_properties(${TARGET_NAME} PROPERTIES
  VERSION ${LIBRARY_VERSION}
  SOVERSION ${LIBRARY_SOVERSION}
  OUTPUT_NAME ${OUTPUT_NAME}
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/stub/include)
target_link_libraries(${TARGET_NAME} PRIVATE taf_prop_common taf_ns_common)

if(PA_ENABLE_DLT)
  target_compile_definitions(${TARGET_NAME} PRIVATE PA_ENABLE_DLT=1)
  target_link_libraries(${TARGET_NAME} PRIVATE dlt)
endif()

install(TARGETS ${TARGET_NAME} LIBRARY DESTINATION lib)
