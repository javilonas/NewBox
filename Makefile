#if 0
# 
# Copyright (c) 2014 - 2015 Javier Sayago <admin@lonasdigital.com>
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

name	= newbox

WARN    = -W -Wall #-Wno-array-bounds
OPTS    = -O3 -ggdb -pipe -ffunction-sections -fdata-sections -D_FORTIFY_SOURCE=2 -fwrapv \
        -DCHECK_NEXTDCW -DHTTP_SRV -DMGCAMD_SRV -DCCCAM_SRV -DFREECCCAM_SRV -DCCCAM_CLI -DRADEGAST_CLI

ifeq ($(target),x32)
CC      = gcc
CXX     = g++
STRIP   = strip
AOUT	= $(name).x86
CFLAGS	= -m32 -I. -s -DTARGET=1 $(OPTS)
LFLAGS  = -lpthread -ldl -lm -lcrypt
else
ifeq ($(target),x64)
CC      = gcc
CXX     = g++
STRIP   = strip
AOUT	= $(name).x86_64
CFLAGS	= -m64 -I. -s -DTARGET=1 $(OPTS)
LFLAGS  = -lpthread -ldl -lm -lcrypt
else
ifeq ($(target),mips)
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
STRIP   = $(CROSS_COMPILE)strip
AOUT	= $(name).mips
CFLAGS	= -mips1 -static-libgcc -EL -I. -s -DTARGET=3 $(OPTS)
LFLAGS  = -lpthread -ldl -lm -lcrypt
else
ifeq ($(target),mips-uclibc)
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
STRIP   = $(CROSS_COMPILE)strip
AOUT	= $(name).mips-uclibc
CFLAGS	= -mips1 -static-libgcc -EL -I. -s -DTARGET=3 $(OPTS)
LFLAGS  = -lpthread -ldl -lm -lcrypt
else
ifeq ($(target),ppc-old)
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
STRIP   = $(CROSS_COMPILE)strip
AOUT	= $(name).ppc-old
CFLAGS	= -I. -s -DTARGET=3 $(OPTS) -DINOTIFY
LFLAGS	= -lpthread -ldl -lm -lcrypt
else
ifeq ($(target),ppc)
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
STRIP   = $(CROSS_COMPILE)strip
AOUT	= $(name).ppc
CFLAGS	= -I. -s -DTARGET=3 $(OPTS) -DINOTIFY
LFLAGS	= -lpthread -ldl -lm -lcrypt
else
ifeq ($(target),sh4)
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
STRIP   = $(CROSS_COMPILE)strip
AOUT	= $(name).sh4
CFLAGS	= -I. -s -DTARGET=3 $(OPTS)
LFLAGS	= -lpthread -ldl -lm -lcrypt
else
ifeq ($(target),arm)
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
STRIP   = $(CROSS_COMPILE)strip
AOUT	= $(name).arm
CFLAGS	= -I. -s -DTARGET=3 $(OPTS)
LFLAGS	= -lpthread -ldl -lm -lcrypt
else
endif
endif
endif
endif
endif
endif
endif
endif

OBJECTS = seca.o irdeto.o sha1.o des.o md5.o convert.o tools.o threads.o debug.o \
	    sockets.o msg-newcamd.o msg-cccam.o msg-radegast.o parser.o config.o \
        aes_core.o i_cbc.o i_skey.o rc6.o ecmdata.o httpserver.o main.o
	
$(AOUT): $(OBJECTS)
	$(CC) $(WARN) $(OPTS) -o $@ -s $(OBJECTS) $(LFLAGS)
			
clean:
	-rm -rf *.o
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
	
.c.o:
	$(CC) $(WARN) -c $(CFLAGS) $< -o $@

