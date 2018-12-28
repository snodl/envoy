#!/bin/bash

# Run a CI build/test target, e.g. docs, asan.

set -e

build_setup_args=""
if [[ "$1" == "fix_format" || "$1" == "check_format" || "$1" == "check_repositories" || "$1" == "check_spelling" || "$1" == "fix_spelling" ]]; then
  build_setup_args="-nofetch"
fi

. "$(dirname "$0")"/setup_gcs_cache.sh
. "$(dirname "$0")"/build_setup.sh $build_setup_args

echo "building using ${NUM_CPUS} CPUs"

function bazel_with_collection() {
  declare -r BAZEL_OUTPUT="${ENVOY_SRCDIR}"/bazel.output.txt
  bazel $* | tee "${BAZEL_OUTPUT}"
  declare BAZEL_STATUS="${PIPESTATUS[0]}"
  if [ "${BAZEL_STATUS}" != "0" ]
  then
    declare -r FAILED_TEST_LOGS="$(grep "  /build.*test.log" "${BAZEL_OUTPUT}" | sed -e 's/  \/build.*\/testlogs\/\(.*\)/\1/')"
    cd bazel-testlogs
    for f in ${FAILED_TEST_LOGS}
    do
      cp --parents -f $f "${ENVOY_FAILED_TEST_LOGS}"
    done
    exit "${BAZEL_STATUS}"
  fi
}

function bazel_release_binary_build() {
  echo "Building..."
  cd "${ENVOY_CI_DIR}"
  bazel build ${BAZEL_BUILD_OPTIONS} -c opt //source/exe:envoy-static
  # Copy the envoy-static binary somewhere that we can access outside of the
  # container.
  cp -f \
    "${ENVOY_CI_DIR}"/bazel-bin/source/exe/envoy-static \
    "${ENVOY_DELIVERY_DIR}"/envoy

  # TODO(mattklein123): Replace this with caching and a different job which creates images.
  echo "Copying release binary for image build..."
  mkdir -p "${ENVOY_SRCDIR}"/build_release
  cp -f "${ENVOY_DELIVERY_DIR}"/envoy "${ENVOY_SRCDIR}"/build_release
  mkdir -p "${ENVOY_SRCDIR}"/build_release_stripped
  strip "${ENVOY_DELIVERY_DIR}"/envoy -o "${ENVOY_SRCDIR}"/build_release_stripped/envoy
}

function bazel_debug_binary_build() {
  echo "Building..."
  cd "${ENVOY_CI_DIR}"
  bazel build ${BAZEL_BUILD_OPTIONS} -c dbg //source/exe:envoy-static
  # Copy the envoy-static binary somewhere that we can access outside of the
  # container.
  cp -f \
    "${ENVOY_CI_DIR}"/bazel-bin/source/exe/envoy-static \
    "${ENVOY_DELIVERY_DIR}"/envoy-debug
}

if [[ "$1" == "bazel.release" ]]; then
  # The release build step still runs during tag events. Avoid rebuilding for no reason.
  # TODO(mattklein123): Consider moving this into its own "build".
  if [[ -n "$CIRCLE_TAG" ]]
  then
    echo 'Ignoring build for git tag event'
    exit 0
  fi

  setup_gcc_toolchain
  echo "bazel release build with tests..."
  bazel_release_binary_build

  if [[ $# -gt 1 ]]; then
    shift
    echo "Testing $* ..."
    # Run only specified tests. Argument can be a single test
    # (e.g. '//test/common/common:assert_test') or a test group (e.g. '//test/common/...')
    bazel_with_collection test ${BAZEL_TEST_OPTIONS} -c opt $*
  else
    echo "Testing..."
    # We have various test binaries in the test directory such as tools, benchmarks, etc. We
    # run a build pass to make sure they compile.
    bazel build ${BAZEL_BUILD_OPTIONS} -c opt //include/... //source/... //test/...
    # Now run all of the tests which should already be compiled.
    bazel_with_collection test ${BAZEL_TEST_OPTIONS} -c opt //test/...
  fi
  exit 0
elif [[ "$1" == "bazel.release.server_only" ]]; then
  setup_gcc_toolchain
  echo "bazel release build..."
  bazel_release_binary_build
  exit 0
elif [[ "$1" == "bazel.debug" ]]; then
  setup_gcc_toolchain
  echo "bazel debug build with tests..."
  bazel_debug_binary_build
  echo "Testing..."
  bazel test ${BAZEL_TEST_OPTIONS} -c dbg //test/...
  exit 0
elif [[ "$1" == "bazel.debug.server_only" ]]; then
  setup_gcc_toolchain
  echo "bazel debug build..."
  bazel_debug_binary_build
  exit 0
elif [[ "$1" == "bazel.asan" ]]; then
  setup_clang_toolchain
  echo "bazel ASAN/UBSAN debug build with tests..."
  cd "${ENVOY_FILTER_EXAMPLE_SRCDIR}"
  echo "Building and testing..."
  bazel_with_collection test ${BAZEL_TEST_OPTIONS} -c dbg --config=clang-asan @envoy//test/... \
    //:echo2_integration_test //:envoy_binary_test
  # Also validate that integration test traffic capture (useful when debugging etc.)
  # works. This requires that we set CAPTURE_ENV. We do this under bazel.asan to
  # ensure a debug build in CI.
  CAPTURE_TMP=/tmp/capture/
  rm -rf "${CAPTURE_TMP}"
  mkdir -p "${CAPTURE_TMP}"
  bazel_with_collection test ${BAZEL_TEST_OPTIONS} -c dbg --config=clang-asan \
    @envoy//test/integration:ssl_integration_test \
    --test_env=CAPTURE_PATH="${CAPTURE_TMP}/capture"
  # Verify that some pb_text files have been created. We can't check for pcap,
  # since tcpdump is not available in general due to CircleCI lack of support
  # for privileged Docker executors.
  ls -l "${CAPTURE_TMP}"/*.pb_text > /dev/null
  exit 0
elif [[ "$1" == "bazel.tsan" ]]; then
  setup_clang_toolchain
  echo "bazel TSAN debug build with tests..."
  cd "${ENVOY_FILTER_EXAMPLE_SRCDIR}"
  echo "Building and testing..."
  bazel_with_collection test ${BAZEL_TEST_OPTIONS} -c dbg --config=clang-tsan @envoy//test/... \
    //:echo2_integration_test //:envoy_binary_test
  exit 0
elif [[ "$1" == "bazel.dev" ]]; then
  setup_clang_toolchain
  # This doesn't go into CI but is available for developer convenience.
  echo "bazel fastbuild build with tests..."
  cd "${ENVOY_CI_DIR}"
  echo "Building..."
  bazel build ${BAZEL_BUILD_OPTIONS} -c fastbuild //source/exe:envoy-static
  # Copy the envoy-static binary somewhere that we can access outside of the
  # container for developers.
  cp -f \
    "${ENVOY_CI_DIR}"/bazel-bin/source/exe/envoy-static \
    "${ENVOY_DELIVERY_DIR}"/envoy-fastbuild
  echo "Building and testing..."
  bazel test ${BAZEL_TEST_OPTIONS} -c fastbuild //test/...
  exit 0
elif [[ "$1" == "bazel.compile_time_options" ]]; then
  # Right now, none of the available compile-time options conflict with each other. If this
  # changes, this build type may need to be broken up.
  COMPILE_TIME_OPTIONS="--define=signal_trace=disabled --define hot_restart=disabled --define google_grpc=disabled --define boringssl=fips"
  setup_clang_toolchain
  # This doesn't go into CI but is available for developer convenience.
  echo "bazel with different compiletime options build with tests..."
  cd "${ENVOY_CI_DIR}"
  echo "Building..."
  bazel build ${BAZEL_BUILD_OPTIONS} ${COMPILE_TIME_OPTIONS} -c dbg //source/exe:envoy-static
  echo "Building and testing..."
  bazel test ${BAZEL_TEST_OPTIONS} ${COMPILE_TIME_OPTIONS} -c dbg //test/...
  exit 0
elif [[ "$1" == "bazel.ipv6_tests" ]]; then
  # This is around until Circle supports IPv6. We try to run a limited set of IPv6 tests as fast
  # as possible for basic sanity testing.

  # Hack to avoid returning IPv6 DNS
  sed -i 's_#precedence ::ffff:0:0/96  100_precedence ::ffff:0:0/96  100_' /etc/gai.conf
  # Debug IPv6 network issues
  apt-get update && apt-get install -y dnsutils net-tools curl && \
    ifconfig && \
    route -A inet -A inet6 && \
    curl -v https://go.googlesource.com && \
    curl -6 -v https://go.googlesource.com && \
    dig go.googlesource.com A go.googlesource.com AAAA

  setup_clang_toolchain
  echo "Testing..."
  cd "${ENVOY_CI_DIR}"
  bazel_with_collection test ${BAZEL_TEST_OPTIONS} --test_env=ENVOY_IP_TEST_VERSIONS=v6only -c fastbuild \
    //test/integration/... //test/common/network/...
  exit 0
elif [[ "$1" == "bazel.api" ]]; then
  setup_clang_toolchain
  cd "${ENVOY_CI_DIR}"
  echo "Building API..."
  bazel build ${BAZEL_BUILD_OPTIONS} -c fastbuild @envoy_api//envoy/...
  echo "Testing API..."
  bazel_with_collection test ${BAZEL_TEST_OPTIONS} -c fastbuild @envoy_api//test/... @envoy_api//tools/... \
    @envoy_api//tools:capture2pcap_test
  exit 0
elif [[ "$1" == "bazel.coverage" ]]; then
  setup_gcc_toolchain
  echo "bazel coverage build with tests..."

  # gcovr is a pain to run with `bazel run`, so package it up into a
  # relocatable and hermetic-ish .par file.
  cd "${ENVOY_SRCDIR}"
  bazel build @com_github_gcovr_gcovr//:gcovr.par
  export GCOVR="${ENVOY_SRCDIR}/bazel-bin/external/com_github_gcovr_gcovr/gcovr.par"

  export GCOVR_DIR="${ENVOY_BUILD_DIR}/bazel-envoy"
  export TESTLOGS_DIR="${ENVOY_BUILD_DIR}/bazel-testlogs"
  export WORKSPACE=ci
  # There is a bug in gcovr 3.3, where it takes the -r path,
  # in our case /source, and does a regex replacement of various
  # source file paths during HTML generation. It attempts to strip
  # out the prefix (e.g. /source), but because it doesn't do a match
  # and only strip at the start of the string, it removes /source from
  # the middle of the string, corrupting the path. The workaround is
  # to point -r in the gcovr invocation in run_envoy_bazel_coverage.sh at
  # some Bazel created symlinks to the source directory in its output
  # directory. Wow.
  cd "${ENVOY_BUILD_DIR}"
  SRCDIR="${GCOVR_DIR}" "${ENVOY_SRCDIR}"/test/run_envoy_bazel_coverage.sh
  exit 0
elif [[ "$1" == "bazel.clang_tidy" ]]; then
  setup_clang_toolchain
  cd "${ENVOY_CI_DIR}"
  ./run_clang_tidy.sh
  exit 0
elif [[ "$1" == "bazel.coverity" ]]; then
  # Coverity Scan version 2017.07 fails to analyze the entirely of the Envoy
  # build when compiled with Clang 5. Revisit when Coverity Scan explicitly
  # supports Clang 5. Until this issue is resolved, run Coverity Scan with
  # the GCC toolchain.
  setup_gcc_toolchain
  echo "bazel Coverity Scan build"
  echo "Building..."
  cd "${ENVOY_CI_DIR}"
  /build/cov-analysis/bin/cov-build --dir "${ENVOY_BUILD_DIR}"/cov-int bazel build --action_env=LD_PRELOAD ${BAZEL_BUILD_OPTIONS} \
    -c opt //source/exe:envoy-static
  # tar up the coverity results
  tar czvf "${ENVOY_BUILD_DIR}"/envoy-coverity-output.tgz -C "${ENVOY_BUILD_DIR}" cov-int
  # Copy the Coverity results somewhere that we can access outside of the container.
  cp -f \
     "${ENVOY_BUILD_DIR}"/envoy-coverity-output.tgz \
     "${ENVOY_DELIVERY_DIR}"/envoy-coverity-output.tgz
  exit 0
elif [[ "$1" == "fix_format" ]]; then
  echo "fix_format..."
  cd "${ENVOY_SRCDIR}"
  ./tools/check_format.py fix
  exit 0
elif [[ "$1" == "check_format" ]]; then
  cd "${ENVOY_SRCDIR}"
  echo "check_format_test..."
  ./tools/check_format_test_helper.py --log=WARN
  echo "check_format..."
  ./tools/check_format.py check
  ./tools/format_python_tools.sh check
  exit 0
elif [[ "$1" == "check_repositories" ]]; then
  cd "${ENVOY_SRCDIR}"
  echo "check_repositories..."
  ./tools/check_repositories.sh
  exit 0
elif [[ "$1" == "check_spelling" ]]; then
  cd "${ENVOY_SRCDIR}"
  echo "check_spelling..."
  ./tools/check_spelling.sh check
  exit 0
elif [[ "$1" == "fix_spelling" ]];then
  cd "${ENVOY_SRCDIR}"
  echo "fix_spell..."
  ./tools/check_spelling.sh fix
  exit 0
elif [[ "$1" == "docs" ]]; then
  echo "generating docs..."
  docs/build.sh
  exit 0
else
  echo "Invalid do_ci.sh target, see ci/README.md for valid targets."
  exit 1
fi
