#!/bin/bash

find . -type f -name '*.h' -exec chmod 644 {} \;
find . -type f -name '*.c' -exec chmod 644 {} \;
find . -type f -name '*.sh' -exec chmod 755 {} \;

export ARCH=sh4
export target=sh4

export TOOLCHAIN=/home/lonas/toolchains/sh4-unknown-linux-gnu

make cleanall

make target=sh4 CROSS_COMPILE=$TOOLCHAIN/bin/sh4-unknown-linux-gnu-

chmod 755 Make_SH4.sh
