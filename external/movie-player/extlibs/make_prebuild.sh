#!/bin/sh

# Assumes vcvars is reflected in msys or cygwin environment
# (VS build environment is set up on msys/cygwin)
# For building with clang, it is necessary to specify Ninja instead of VS for the Generator,
# but in the case of Ninja, the target switches depending on whether the execution environment's vcvars is for 32-bit or 64-bit,
# so please be sure to execute in a vcvars64 environment.

RELATIVE_DIR=`dirname "$0"`
cd ${RELATIVE_DIR}/libyuv || exit
mkdir build
cd build/

# build Debug and copy
echo "Building libyuv Win64 lib [Debug]: start"
cmake \
  -G"Ninja" \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_C_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=Debug \
  ..
cmake --build .
cp yuv.lib ../../prebuild/win64/yuv_debug.lib
echo "done."

# build Release and copy
echo "Building libyuv Win64 lib [Relase]: start"
cmake \
  -G"Ninja" \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_C_COMPILER=clang-cl \
  -DCMAKE_BUILD_TYPE=Release \
  ..
cmake --build .
cp yuv.lib ../../prebuild/win64/yuv_release.lib
echo "done."
