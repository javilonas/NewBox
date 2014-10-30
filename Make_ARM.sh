#!/bin/bash

find . -type f -name '*.h' -exec chmod 644 {} \;
find . -type f -name '*.c' -exec chmod 644 {} \;
find . -type f -name '*.sh' -exec chmod 755 {} \;

export ARCH=arm
export target=arm

export TOOLCHAIN=/home/lonas/toolchains/arm-unknown-linux-gnueabi

make cleanall

make target=arm CROSS_COMPILE=$TOOLCHAIN/bin/arm-unknown-linux-gnueabi-

chmod 755 Make_ARM.sh
