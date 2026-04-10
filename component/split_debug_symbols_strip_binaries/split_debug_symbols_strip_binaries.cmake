# This script extracts debug symbols from installed binaries and strips them
# It is called during the install phase via install(SCRIPT)

# Get the installation directory
set(INSTALL_DIR "${CMAKE_INSTALL_PREFIX}")

# Create directory for debug symbols using Build ID structure
set(DEBUG_DIR "${INSTALL_DIR}/.build-id")
file(MAKE_DIRECTORY "${DEBUG_DIR}")

# Use cross-compilation tools if available
if(DEFINED ENV{OBJCOPY})
    set(OBJCOPY_CMD "$ENV{OBJCOPY}")
else()
    set(OBJCOPY_CMD "objcopy")
endif()

if(DEFINED ENV{STRIP})
    set(STRIP_CMD "$ENV{STRIP}")
else()
    set(STRIP_CMD "strip")
endif()

if(DEFINED ENV{READELF})
    set(READELF_CMD "$ENV{READELF}")
else()
    set(READELF_CMD "readelf")
endif()

# Validate that required tools are available
foreach(tool_var OBJCOPY_CMD STRIP_CMD READELF_CMD)
    execute_process(
        COMMAND which ${${tool_var}}
        RESULT_VARIABLE tool_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT tool_result EQUAL 0)
        message(FATAL_ERROR "Required tool '${${tool_var}}' not found in PATH")
    endif()
endforeach()

message(STATUS ">>> Extracting debug symbols and stripping binaries")
message(STATUS "    Using objcopy: ${OBJCOPY_CMD}")
message(STATUS "    Using strip: ${STRIP_CMD}")
message(STATUS "    Using readelf: ${READELF_CMD}")

# Find all ELF binaries (executables and shared libraries)
file(GLOB_RECURSE all_files "${INSTALL_DIR}/*")

foreach(binary ${all_files})
    # Skip if not a regular file
    if(IS_DIRECTORY "${binary}")
        continue()
    endif()

    # Skip if already in .build-id directory
    string(FIND "${binary}" "/.build-id/" build_id_pos)
    if(NOT build_id_pos EQUAL -1)
        continue()
    endif()

    # Check if it's an ELF file
    execute_process(
        COMMAND file "${binary}"
        OUTPUT_VARIABLE file_output
        ERROR_QUIET
    )

    string(FIND "${file_output}" "ELF" elf_pos)
    if(elf_pos EQUAL -1)
        continue()
    endif()

    # Get relative path from install dir
    file(RELATIVE_PATH rel_path "${INSTALL_DIR}" "${binary}")
    message(STATUS "  Processing: ${rel_path}")

    # Extract Build ID from the binary
    execute_process(
        COMMAND ${READELF_CMD} -n "${binary}"
        OUTPUT_VARIABLE readelf_output
        ERROR_QUIET
        RESULT_VARIABLE readelf_result
    )

    if(NOT readelf_result EQUAL 0)
        message(STATUS "    Warning: Failed to read ELF notes, skipping ${rel_path}")
        continue()
    endif()

    # Parse Build ID from readelf output
    string(REGEX MATCH "Build ID: ([0-9a-fA-F]+)" build_id_match "${readelf_output}")
    if(NOT build_id_match)
        message(STATUS "    Warning: No Build ID found, skipping ${rel_path}")
        continue()
    endif()

    set(BUILD_ID "${CMAKE_MATCH_1}")

    # Validate Build ID has minimum length
    string(LENGTH "${BUILD_ID}" build_id_length)
    if(build_id_length LESS 3)
        message(STATUS "    Warning: Build ID too short (${BUILD_ID}), skipping ${rel_path}")
        continue()
    endif()

    # Create .build-id directory structure: .build-id/<first-2-chars>/<remaining-chars>.debug
    # GDB will automatically find debug symbols using Build IDs
    string(SUBSTRING "${BUILD_ID}" 0 2 build_id_prefix)
    string(SUBSTRING "${BUILD_ID}" 2 -1 build_id_suffix)
    set(BUILD_ID_DIR "${DEBUG_DIR}/${build_id_prefix}")
    file(MAKE_DIRECTORY "${BUILD_ID_DIR}")
    set(debug_file "${BUILD_ID_DIR}/${build_id_suffix}.debug")

    message(STATUS "    Build ID: ${BUILD_ID}")

    # Extract debug symbols
    execute_process(
        COMMAND ${OBJCOPY_CMD} --only-keep-debug "${binary}" "${debug_file}"
        RESULT_VARIABLE objcopy_result
        ERROR_VARIABLE objcopy_error
    )

    if(NOT objcopy_result EQUAL 0)
        message(FATAL_ERROR "    Error: Failed to extract debug symbols from ${rel_path}: ${objcopy_error}")
    endif()

    # Strip the binary
    execute_process(
        COMMAND ${STRIP_CMD} --strip-unneeded "${binary}"
        RESULT_VARIABLE strip_result
        ERROR_VARIABLE strip_error
    )

    if(NOT strip_result EQUAL 0)
        message(FATAL_ERROR "    Error: Failed to strip ${rel_path}: ${strip_error}")
    endif()
endforeach()

message(STATUS ">>> Debug symbols extracted to: ${DEBUG_DIR}")
