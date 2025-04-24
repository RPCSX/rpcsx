#!/bin/sh -ex

git config --global --add safe.directory '*'

# Pull all the submodules except llvm and opencv
# shellcheck disable=SC2046
git submodule -q update --init $(awk '/path/ && !/llvm/ && !/opencv/ { print $3 }' .gitmodules)

if [ "$COMPILER" = "gcc" ]; then
    export CC=gcc-14
    export CXX=g++-14
else
    export CC=clang
    export CXX=clang++
    export CFLAGS="$CFLAGS -fuse-ld=lld"
fi

cmake -B build                                         \
    -DCMAKE_INSTALL_PREFIX=/usr                        \
    -DCMAKE_C_FLAGS="$CFLAGS"                          \
    -DCMAKE_CXX_FLAGS="$CFLAGS"                        \
    -DUSE_NATIVE_INSTRUCTIONS=OFF                      \
    -DUSE_PRECOMPILED_HEADERS=OFF                      \
    -DUSE_SYSTEM_CURL=ON                               \
    -DUSE_SDL=OFF                                      \
    -DUSE_SYSTEM_FFMPEG=OFF                            \
    -DUSE_SYSTEM_CURL=OFF                              \
    -DUSE_SYSTEM_OPENAL=OFF                            \
    -DUSE_SYSTEM_FFMPEG=OFF                            \
    -DUSE_DISCORD_RPC=ON                               \
    -DOpenGL_GL_PREFERENCE=LEGACY                      \
    -DSTATIC_LINK_LLVM=ON                              \
    -DBUILD_LLVM=on                                    \
    -DWITH_RPCSX=off                                   \
    -DWITH_RPCS3=on                                    \
    -DWITH_RPCS3_QT_UI=on                              \
    -G Ninja

cmake --build build; build_status=$?;

shellcheck .ci/*.sh

# If it compiled succesfully let's deploy.
# Azure and Cirrus publish PRs as artifacts only.
{   [ "$CI_HAS_ARTIFACTS" = "true" ];
} && SHOULD_DEPLOY="true" || SHOULD_DEPLOY="false"

if [ "$build_status" -eq 0 ] && [ "$SHOULD_DEPLOY" = "true" ]; then
    .ci/deploy-linux.sh "aarch64"
fi
