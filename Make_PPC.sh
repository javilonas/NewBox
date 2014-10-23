#!/bin/bash

find . -type f -name '*.h' -exec chmod 644 {} \;
find . -type f -name '*.c' -exec chmod 644 {} \;
find . -type f -name '*.sh' -exec chmod 755 {} \;

export ARCH=ppc
export target=ppc

export TOOLCHAIN=/home/lonas/toolchains/powerpc-tuxbox-linux-gnu

make cleanall

make target=ppc CROSS_COMPILE=$TOOLCHAIN/bin/powerpc-linux-

chmod 755 Make_PPC.sh
