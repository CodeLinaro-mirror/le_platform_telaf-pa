#!/bin/sh

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
    "${CURDIR}/component/taf_pa_voicecall/"
    "${CURDIR}/component/taf_pa_keystore/"
    "${CURDIR}/component/taf_pa_fscrypt/"
    "${CURDIR}/component/taf_pa_sensor/"
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
    *)
        echo "Unknown TARGET: ${TARGET}"
        exit 1
        ;;
esac
