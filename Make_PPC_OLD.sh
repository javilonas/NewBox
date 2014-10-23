#!/bin/bash

find . -type f -name '*.h' -exec chmod 644 {} \;
find . -type f -name '*.c' -exec chmod 644 {} \;
find . -type f -name '*.sh' -exec chmod 755 {} \;

export ARCH=ppc
export target=ppc-old

export TOOLCHAIN=/home/lonas/toolchains/powerpc-old-tuxbox-linux/dm500/cdk

make cleanall

make target=ppc-old CROSS_COMPILE=$TOOLCHAIN/bin/powerpc-tuxbox-linux-gnu-

chmod 755 Make_PPC_OLD.sh
