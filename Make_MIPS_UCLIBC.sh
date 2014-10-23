#!/bin/bash

find . -type f -name '*.h' -exec chmod 644 {} \;
find . -type f -name '*.c' -exec chmod 644 {} \;
find . -type f -name '*.sh' -exec chmod 755 {} \;

export ARCH=mips
export target=mips-uclibc

export TOOLCHAIN=/home/lonas/toolchains/stbgcc-4.5.3-2.4

make cleanall

make target=mips-uclibc CROSS_COMPILE=$TOOLCHAIN/bin/mipsel-linux-uclibc-

chmod 755 Make_MIPS_UCLIBC.sh
