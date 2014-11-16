#!/bin/bash

find . -type f -name '*.h' -exec chmod 644 {} \;
find . -type f -name '*.c' -exec chmod 644 {} \;
find . -type f -name '*.sh' -exec chmod 755 {} \;

export ARCH=mips
export target=mips-uclibc-openwrt

export TOOLCHAIN=/home/lonas/toolchains/OpenWrt-Toolchain-ar71xx-for-mips_34kc-gcc-4.8-linaro_uClibc-0.9.33.2/toolchain-mips_34kc_gcc-4.8-linaro_uClibc-0.9.33.2

make cleanall

make target=mips-uclibc-openwrt CROSS_COMPILE=$TOOLCHAIN/bin/mips-openwrt-linux-uclibc-

chmod 755 Make_MIPS_UCLIBC_OPENWRT.sh
