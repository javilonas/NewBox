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

#ifndef _COMMON_H_
#define _COMMON_H_

#define FD_SETSIZE2  4096

#define DELAY_MAIN   10000
#define DELAY_RECV   0
#define DELAY_ACCEPT 100000

#ifdef CCCAM_SRV
#define CCCAM
#else
#ifdef CCCAM_CLI
#define CCCAM
#endif
#endif

#ifdef MGCAMD_SRV
#define MGCAMD
#else
#ifdef MGCAMD_CLI
#define MGCAMD
#endif
#endif

#ifdef RADEGAST_SRV
#define RADEGAST
#else
#ifdef RADEGAST_CLI
#define RADEGAST
#endif
#endif

#define DATE_BUILD    "15-03-2016"
#define VERSION    "07.r28"
#define REVISION   07

#define FALSE 0
#define TRUE 1

typedef unsigned char uchar;
typedef unsigned short int usint;
typedef unsigned int uint;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;


// win32 definitions
#ifndef WIN32

typedef int32_t SOCKET;
typedef int32_t HANDLE;
#define INVALID_HANDLE_VALUE -1
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

#endif

#endif
