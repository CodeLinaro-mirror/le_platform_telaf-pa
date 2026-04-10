#!/bin/sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

CURDIR=$(cd `dirname $0` ; pwd)
TARGET=$1

set -e

cd ${LEGATO_ROOT} && source ${LEGATO_ROOT}/build/${TARGET}/config.sh && \
    source ${LEGATO_ROOT}/bin/configlegatoenv && cd ${CURDIR}

# Add cflags to support test coverage build.
if [ "${LE_CONFIG_TEST_COVERAGE}" == "y" ]; then
    LOCAL_MKCOMP_FLAGS="--cflags=--coverage --cxxflags=--coverage --ldflags=--coverage"
    if [ "${LE_CONFIG_TEST_COVERAGE_DIR}" != "" ]; then
        LOCAL_MKCOMP_FLAGS="${LOCAL_MKCOMP_FLAGS} --cflags=-fprofile-dir=${LE_CONFIG_TEST_COVERAGE_DIR} --cxxflags=-fprofile-dir=${LE_CONFIG_TEST_COVERAGE_DIR}"
    fi
fi

# Put all built-fruits into "${LEGATO_ROOT}/build/${TARGET}" which is the consolidated build place.
# So that we can easily found all .gcda and .gcno files under that directory.
LOCAL_BUILD_DIR="${LEGATO_ROOT}/build/${TARGET}/telaf-pa"
mkdir -p $LOCAL_BUILD_DIR; cd $LOCAL_BUILD_DIR

stub_args_sa525m=(
    "${CURDIR}/component/stub/taf_prop_keystore/"
    "${CURDIR}/component/stub/taf_prop_fscrypt/"
)
args_sa525m=(
    "${CURDIR}/component/taf_pa_keystore/"
    "${CURDIR}/component/taf_pa_fscrypt/"
    "${CURDIR}/component/taf_pa_sensor/"
    "${CURDIR}/component/taf_pa_data/"
)

case "${TARGET}" in
    "sa525m")
        for ((i=0; i<${#stub_args_sa525m[@]} + ${#args_sa525m[@]}; i++))
        do
            if [ $i -lt ${#stub_args_sa525m[@]} ]; then
                comp=${stub_args_sa525m[$i]}
                mkcomp ${LOCAL_MKCOMP_FLAGS} -t "${TARGET}" \
                       -X "-O2" -C "-O2" ${MKTOOLS_X_C_FLAGS} \
                       -i "${TELAF_ROOT}/interfaces/" ${comp}
            else
                j=$((i - ${#stub_args_sa525m[@]}))
                comp=${args_sa525m[$j]}
                mkcomp ${LOCAL_MKCOMP_FLAGS} -t "${TARGET}" \
                       -X "-O2" -C "-O2" ${MKTOOLS_X_C_FLAGS} \
                       -i "${TELAF_ROOT}/interfaces/" ${comp}
            fi
        done
        ;;
    "sa510m")
        for ((i=0; i<${#args_sa510m[@]}; i++))
        do
            if [ $i -lt ${#stub_args_sa510m[@]} ]; then
                comp=${stub_args_sa510m[$i]}
                mkcomp ${LOCAL_MKCOMP_FLAGS} -t "${TARGET}" \
                       -X "-O2" -C "-O2" ${MKTOOLS_X_C_FLAGS} \
                       -i "${TELAF_ROOT}/interfaces/" ${comp}
            else
                j=$((i - ${#stub_args_sa510m[@]}))
                comp=${args_sa510m[$j]}
                mkcomp ${LOCAL_MKCOMP_FLAGS} -t "${TARGET}" \
                       -X "-O2" -C "-O2" ${MKTOOLS_X_C_FLAGS} \
                       -i "${TELAF_ROOT}/interfaces/" ${comp}
            fi
        done
        ;;
    *)
        echo "Unknown TARGET: ${TARGET}"
        exit 1
        ;;
esac

# Extract debug symbols and strip binaries
DEBUG_DIR="${LOCAL_BUILD_DIR}/.build-id"
mkdir -p "${DEBUG_DIR}"

# Use cross-compilation tools if available
OBJCOPY_CMD="${OBJCOPY:-objcopy}"
STRIP_CMD="${STRIP:-strip}"
READELF_CMD="${READELF:-readelf}"

# Validate that required tools are available
for tool_var in OBJCOPY_CMD STRIP_CMD READELF_CMD; do
    tool_name="${!tool_var}"
    if ! command -v "${tool_name}" >/dev/null 2>&1; then
        echo "Error: Required tool '${tool_name}' not found in PATH"
        exit 1
    fi
done

echo ">>> Extracting debug symbols and stripping binaries"
echo "    Using objcopy: ${OBJCOPY_CMD}"
echo "    Using strip: ${STRIP_CMD}"
echo "    Using readelf: ${READELF_CMD}"

# Find all ELF binaries (executables and shared libraries)
# Use process substitution to avoid subshell and allow proper error propagation
while IFS= read -r binary; do
    # Skip if not an ELF file
    if ! file "${binary}" | grep -q "ELF"; then
        continue
    fi

    # Skip if already in .build-id directory
    if [[ "${binary}" == *"/.build-id/"* ]]; then
        continue
    fi

    # Get relative path from build dir
    rel_path="${binary#${LOCAL_BUILD_DIR}/}"

    echo "  Processing: ${rel_path}"

    # Extract Build ID from the binary
    export LANG=C
    BUILD_ID=$(${READELF_CMD} -n "${binary}" 2>/dev/null | sed 's/    Build ID: //;t;d')

    if [ -z "$BUILD_ID" ]; then
        echo "    Warning: No Build ID found, skipping ${rel_path}"
        continue
    fi

    # Validate Build ID has minimum length
    if [ ${#BUILD_ID} -lt 3 ]; then
        echo "    Warning: Build ID too short (${BUILD_ID}), skipping ${rel_path}"
        continue
    fi

    # Create .build-id directory structure: .build-id/<first-2-chars>/<remaining-chars>.debug
    # GDB will automatically find debug symbols using Build IDs
    BUILD_ID_DIR="${DEBUG_DIR}/${BUILD_ID:0:2}"
    mkdir -p "${BUILD_ID_DIR}"
    debug_file="${BUILD_ID_DIR}/${BUILD_ID:2}.debug"

    echo "    Build ID: ${BUILD_ID}"

    # Extract debug symbols
    if ! ${OBJCOPY_CMD} --only-keep-debug "${binary}" "${debug_file}"; then
        echo "    Error: Failed to extract debug symbols from ${rel_path}"
        exit 1
    fi

    # Strip the binary
    if ! ${STRIP_CMD} --strip-unneeded "${binary}"; then
        echo "    Error: Failed to strip ${rel_path}"
        exit 1
    fi
done < <(find "${LOCAL_BUILD_DIR}" -type f \( -executable -o -name "*.so*" \))

echo ">>> Debug symbols extracted to: ${DEBUG_DIR}"
