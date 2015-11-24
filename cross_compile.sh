#!/bin/bash

# build_output 디렉토리를 만들고, 그곳에 빌드 결과물을 담도록 한다.
# 사용예)
# MK=-j4 ./cross_compile.sh 를 수행하면, make에 -j4 옵션을 붙여서 더 빠르게 컴파일을 수행할 수 있다.

# 여기를 각자의 환경에 맞게 수정하여 사용한다.

TOOLCHAIN_DIR=`pwd`/../cross-root
OUTPUT_DIR=build_output

mkdir -p build_output
export PATH=$TOOLCHAIN_DIR/bin:$PATH
export STAGING_DIR=$TOOLCHAIN_DIR
make ${MK} O=build_output ARCH=mips CROSS_COMPILE=mips-openwrt-linux- menuconfig
make ${MK} O=build_output ARCH=mips CROSS_COMPILE=mips-openwrt-linux- "$@" \
&& ./cp_ath9k_drivers.sh
