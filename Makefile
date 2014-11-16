SHELL = /bin/sh

UNAME := $(shell uname -s)

# Compiler warnings
#WARN = -W -Wall -Wredundant-decls -Wold-style-definition -Wundef
WARN = -W -Wall -Wundef -Wshadow -Wredundant-decls -Wstrict-prototypes -Wold-style-definition -Wno-unused \
	-Wno-format-security -fno-delete-null-pointer-checks

OPTS = -pipe -fdata-sections -ffunction-sections -DCHECK_NEXTDCW -DHTTP_SRV -DMGCAMD_SRV -DCCCAM_SRV -DFREECCCAM_SRV -DCCCAM_CLI -DRADEGAST_CLI

ifeq ($(target),x32)
  CC        = gcc
  LD        = ld
  CXX       = g++
  STRIP     = strip
  OUTPUT	= x32/
  CFLAGS	= -s -ggdb3 -m32 -O2 -Wall -I. -DTARGET=1 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),x64)
  CC        = gcc
  LD        = ld
  CXX       = g++
  STRIP     = strip
  OUTPUT	= x64/
  CFLAGS	= -s -ggdb3 -m64 -O2 -Wall -I. -DTARGET=1 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),ppc-old)
  CC        = $(CROSS_COMPILE)gcc
  LD        = $(CROSS_COMPILE)ld
  CXX       = $(CROSS_COMPILE)g++
  STRIP     = $(CROSS_COMPILE)strip
  OUTPUT	= ppc-old/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=3 $(OPTS) -DINOTIFY
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),mips)
  CC        = $(CROSS_COMPILE)gcc
  LD        = $(CROSS_COMPILE)ld
  CXX       = $(CROSS_COMPILE)g++
  STRIP     = $(CROSS_COMPILE)strip
  OUTPUT	= mips/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=3 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),mips-uclibc)
  CC        = $(CROSS_COMPILE)gcc
  LD        = $(CROSS_COMPILE)ld
  CXX       = $(CROSS_COMPILE)g++
  STRIP     = $(CROSS_COMPILE)strip
  OUTPUT	= mips-uclibc/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=3 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),mips-uclibc-openwrt)
  CC        = $(CROSS_COMPILE)gcc
  LD        = $(CROSS_COMPILE)ld
  CXX       = $(CROSS_COMPILE)g++
  STRIP     = $(CROSS_COMPILE)strip
  OUTPUT	= mips-uclibc-openwrt/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=3 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),sh4)
  CC        = $(CROSS_COMPILE)gcc
  LD        = $(CROSS_COMPILE)ld
  CXX       = $(CROSS_COMPILE)g++
  STRIP     = $(CROSS_COMPILE)strip
  OUTPUT	= sh4/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=3 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),openwrt)
  CC        = armeb-linux-gcc
  LD        = armeb-linux-ld
  CXX       = armeb-linux-g++
  STRIP     = armeb-linux-strip
  OUTPUT	= openwrt/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=3 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),arm)
  CC        = $(CROSS_COMPILE)gcc
  LD        = $(CROSS_COMPILE)ld
  CXX       = $(CROSS_COMPILE)g++
  STRIP     = $(CROSS_COMPILE)strip
  OUTPUT	= arm/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=3 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),ppc)
  CC        = $(CROSS_COMPILE)gcc
  LD        = $(CROSS_COMPILE)ld
  CXX       = $(CROSS_COMPILE)g++
  STRIP     = $(CROSS_COMPILE)strip
  OUTPUT	= ppc/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=3 $(OPTS) -DINOTIFY
  LFLAGS	= $(CFLAGS)
else
ifeq ($(target),win32)
  CC        = i586-mingw32msvc-gcc
  LD        = i586-mingw32msvc-ld
  CXX       = i586-mingw32msvc-g++
  STRIP     = i586-mingw32msvc-strip
  OUTPUT	= win32/
  CFLAGS	= -s -ggdb3 -O2 -Wall -I. -DTARGET=2 -DWIN32 $(OPTS)
  LFLAGS	= $(CFLAGS)
else
  CC        = gcc
  LD        = ld
  CXX       = g++
  STRIP     = strip
  OUTPUT	= x/
  CFLAGS	= -ggdb3 -O2 -Wall -I. -DTARGET=2 $(OPTS)
  LFLAGS	= $(CFLAGS)
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif

ifndef name
	AOUT	= $(OUTPUT)Newbox
else
	AOUT	= $(name)
endif

OBJECTS = $(OUTPUT)sha1.o $(OUTPUT)des.o $(OUTPUT)md5.o $(OUTPUT)convert.o $(OUTPUT)tools.o $(OUTPUT)threads.o $(OUTPUT)debug.o \
	$(OUTPUT)sockets.o $(OUTPUT)msg-newcamd.o $(OUTPUT)msg-cccam.o $(OUTPUT)msg-radegast.o $(OUTPUT)parser.o $(OUTPUT)config.o \
        $(OUTPUT)bn_ctx.o $(OUTPUT)bn_lib.o $(OUTPUT)bn_exp.o $(OUTPUT)bn_sqr.o $(OUTPUT)bn_div.o $(OUTPUT)bn_asm.o $(OUTPUT)bn_shift.o \
        $(OUTPUT)bn_word.o $(OUTPUT)bn_add.o $(OUTPUT)bn_mul.o $(OUTPUT)aes.o $(OUTPUT)i_cbc.o $(OUTPUT)i_ecb.o $(OUTPUT)i_skey.o \
        $(OUTPUT)mem.o $(OUTPUT)via3surenc.o $(OUTPUT)emulator.o\
	$(OUTPUT)ecmdata.o $(OUTPUT)httpserver.o $(OUTPUT)main.o


link: main.c $(OBJECTS)
	$(CC) $(WARN) $(OPTS) $(LFLAGS) -Wall -o $(AOUT) $(OBJECTS) -lpthread

%.o: ../%.c Makefile common.h globals.h helpfunctions.h httpserver.h config.h Newbox.cfg Newbox.channelinfo badcw.cfg
	$(CC) $(WARN) -Wall -c $(CFLAGS) $< -o $@

clean:
	-rm -rf $(OUTPUT)*

cleanall:
	$(MAKE) clean
	$(MAKE) target=x clean
	$(MAKE) target=x64 clean
	$(MAKE) target=x32 clean
	$(MAKE) target=ppc clean
	$(MAKE) target=ppc-old clean
	$(MAKE) target=mips clean
	$(MAKE) target=mips-uclibc clean
	$(MAKE) target=mips-uclibc-openwrt clean
	$(MAKE) target=win32 clean
	$(MAKE) target=openwrt clean
	$(MAKE) target=sh4 clean
	$(MAKE) target=arm clean

