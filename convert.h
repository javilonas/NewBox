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

#include <stdio.h>

void bin8(unsigned char n, char *s);
void bin16(unsigned short n, char *s);
void bin32(unsigned short n, char *s);
void array2bin( char *src, char *dest, int count );
void hex8(int n, char *s);
void hex16(int n, char *s);
void hex32(int n, char *s);
char *array2hex( unsigned char *src, char *dest, int count );
int hexvalue(char src);
int hex2int(char *src);
int hex2array( char *src, unsigned char *dest);
