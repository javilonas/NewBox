#if 0
# 
# Copyright (c) 2014 - 2016 Javier Sayago <admin@lonasdigital.com>
# Contact: javilonas@esp-desarrolladores.com
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#endif

UNAME := $(shell uname -s)

name	= newbox

# Compiler optimizations
WARN	= -W -Wall -Wshadow -Wredundant-decls
OPTS	= -O2 -ggdb3 -pipe -fPIC -funroll-loops -ffunction-sections -fdata-sections -fwrapv -fomit-frame-pointer \
		-D_FORTIFY_SOURCE=2 -DCHECK_NEXTDCW -DHTTP_SRV -DNEWCAMD_SRV -DMGCAMD_SRV -DCCCAM_SRV -DFREECCCAM_SRV -DCCCAM_CLI -DRADEGAST_CLI

ifeq ($(target),x32)
CC			= gcc
LD			= ld
CXX			= g++
STRIP		= strip
AOUT		= $(name).x86
CFLAGS		= -m32 -I. -s -DTARGET=1 $(OPTS)
LFLAGS		= $(CFLAGS) -lpthread -ldl -lm -Wl,--gc-sections
else
ifeq ($(target),x64)
CC			= gcc
LD			= ld
CXX			= g++
STRIP		= strip
AOUT		= $(name).x86_64
CFLAGS		= -m64 -I. -s -DTARGET=1 $(OPTS)
LFLAGS		= $(CFLAGS) -lpthread -ldl -lm -Wl,--gc-sections
else
ifeq ($(target),mips)
CC			= $(CROSS_COMPILE)gcc
LD			= $(CROSS_COMPILE)ld
CXX			= $(CROSS_COMPILE)g++
STRIP		= $(CROSS_COMPILE)strip
AOUT		= $(name).mips
CFLAGS		= -mips1 -static-libgcc -EL -I. -s -DTARGET=3 $(OPTS)
LFLAGS		= $(CFLAGS) -lpthread -ldl -lm -Wl,--gc-sections
else
ifeq ($(target),mips-uclibc)
CC			= $(CROSS_COMPILE)gcc
LD			= $(CROSS_COMPILE)ld
CXX			= $(CROSS_COMPILE)g++
STRIP		= $(CROSS_COMPILE)strip
AOUT		= $(name).mips-uclibc
CFLAGS		= -mips1 -static-libgcc -EL -I. -s -DTARGET=3 $(OPTS)
LFLAGS		= $(CFLAGS) -lpthread -ldl -lm -Wl,--gc-sections
else
ifeq ($(target),ppc-old)
CC			= $(CROSS_COMPILE)gcc
LD			= $(CROSS_COMPILE)ld
CXX			= $(CROSS_COMPILE)g++
STRIP		= $(CROSS_COMPILE)strip
AOUT		= $(name).ppc-old
CFLAGS		= -I. -s -DTARGET=3 $(OPTS) -DINOTIFY
LFLAGS		= $(CFLAGS) -lpthread -ldl -lm -Wl,--gc-sections
else
ifeq ($(target),ppc)
CC			= $(CROSS_COMPILE)gcc
LD			= $(CROSS_COMPILE)ld
CXX			= $(CROSS_COMPILE)g++
STRIP		= $(CROSS_COMPILE)strip
AOUT		= $(name).ppc
CFLAGS		= -I. -s -DTARGET=3 $(OPTS) -DINOTIFY
LFLAGS		= $(CFLAGS) -lpthread -ldl -lm -Wl,--gc-sections
else
ifeq ($(target),sh4)
CC			= $(CROSS_COMPILE)gcc
LD			= $(CROSS_COMPILE)ld
CXX			= $(CROSS_COMPILE)g++
STRIP		= $(CROSS_COMPILE)strip
AOUT		= $(name).sh4
CFLAGS		= -static-libgcc -I. -s -DTARGET=3 $(OPTS)
LFLAGS		= $(CFLAGS) -lpthread -ldl -lm -Wl,--gc-sections
else
ifeq ($(target),arm)
CC			= $(CROSS_COMPILE)gcc
LD			= $(CROSS_COMPILE)ld
CXX			= $(CROSS_COMPILE)g++
STRIP		= $(CROSS_COMPILE)strip
AOUT		= $(name).arm
CFLAGS		= -mtune=cortex-a9 -mcpu=cortex-a9 -mfpu=neon-vfpv4 -static-libgcc -I. -s -DTARGET=3 $(OPTS)
LFLAGS		= $(CFLAGS) -lpthread -ldl -lm -Wl,--gc-sections
else
endif
endif
endif
endif
endif
endif
endif
endif

SRCS =  cscrypt/aes.o cscrypt/des.o cscrypt/md5.o cscrypt/i_cbc.o \
	cscrypt/i_ecb.o cscrypt/i_skey.o cscrypt/rc6.o cscrypt/sha1.o \
	cscrypt/bn_add.o cscrypt/bn_asm.o cscrypt/bn_ctx.o cscrypt/bn_div.o \
	cscrypt/bn_exp.o cscrypt/bn_lib.o cscrypt/bn_mul.o cscrypt/bn_print.o \
	cscrypt/bn_shift.o cscrypt/bn_sqr.o cscrypt/bn_word.o cscrypt/mem.o \
	sockets.o msg-newcamd.o msg-cccam.o msg-radegast.o parser.o config.o \
	ecmdata.o httpserver.o convert.o tools.o threads.o debug.o main.o

Q = @
SAY = @echo
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)
BIN = $(AOUT)

all: $(AOUT)

-include $(OBJS:.o=.d)

%.o: %.c
	$(Q)$(CC) $(WARN) -c -o $@ $< $(CFLAGS)
	$(SAY) "CC	$<"
	$(Q)$(CC) $(WARN) -MM $(CFLAGS) $*.c > $*.d

$(AOUT): $(SRCS)
	$(CC) $(WARN) -o $@ -s $(SRCS) $(CFLAGS) $(LFLAGS) -Wl,--format=binary -Wl,--format=default
	$(STRIP) $(BIN)

clean:
	-rm -rf *.o
	-rm -rf cscrypt/*.o
	-rm -rf $(AOUT)

cleanall:
	$(MAKE) clean
	$(MAKE) target=x64 clean
	$(MAKE) target=x32 clean
	$(MAKE) target=mips clean
	$(MAKE) target=ppc clean
	$(MAKE) target=ppc-old clean
	$(MAKE) target=sh4 clean
	$(MAKE) target=arm clean

.PHONY: $(AOUT)
