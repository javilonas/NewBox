#!/bin/bash

find . -type f -name '*.h' -exec chmod 644 {} \;
find . -type f -name '*.c' -exec chmod 644 {} \;
find . -type f -name '*.sh' -exec chmod 755 {} \;

export ARCH=mips
export target=mips

export TOOLCHAIN=/home/lonas/toolchains/mipsel-unknown-linux-gnu

make cleanall

make target=mips CROSS_COMPILE=$TOOLCHAIN/bin/mipsel-unknown-linux-gnu-

chmod 755 Make_MIPS.sh
