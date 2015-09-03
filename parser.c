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
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <errno.h>

#include "common.h"
#include "parser.h"


///////////////////////////////////////////////////////////////////////////////
// PARSER FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

char *iparser; // Current Parser Index

//Skip Spaces
void parse_spaces()
{
	while ( (*iparser==' ')||(*iparser=='\t') ) iparser++;
}


int charpos( char c, char *str )
{
	int i;
	int l = strlen(str);
	for(i=0; i<l; i++) if (c==str[i]) return i+1;
	return 0;
}

int parse_value(char *str, char *delimiters)
{
	int len;
	char *end;
	parse_spaces();
	end=iparser;
	while ( !charpos(*end,delimiters) && (*end!=0) ) end++;
	if ( (len=end-iparser)>0 ) {
		if (len>=255) len=255; // check for length
		memcpy(str, iparser, len);
		iparser = end;
	}
	str[len] = 0;
	return len;
}

int parse_str(char *str)
{
	int len;
	char *end;
	parse_spaces();
	end=iparser;
	while ( (*end!=0)&&(*end!=' ')&&(*end!='\t')&&(*end!=13)&&(*end!=10) ) end++;
	if ( (len=end-iparser)>0 ) {
		if (len>=255) len=255; // check for length
		memcpy(str, iparser, len);
		iparser = end;
	}
	str[len] = 0;
	return len;
}

int parse_name(char *str)
{
	int len;
	char *end;
	parse_spaces();
	end=iparser;
	while ( (*end!=0)&&(*end!=' ')&&(*end!='\t')&&(*end!=13)&&(*end!=10)&&(*end!=']')&&(*end!=':') ) end++;
	if ( (len=end-iparser)>0 ) {
		if (len>=255) len=255; // check for length
		memcpy(str, iparser, len);
		iparser = end;
	}
	str[len] = 0;
	return len;
}

int parse_int(char *str)
{
	int len;
	char *end;
	parse_spaces();
	end=iparser;
	while ( (*end>='0')&&(*end<='9') ) end++;
	if ( (len=end-iparser)>0 ) {
		if (len>=255) len=255; // check for length
		memcpy(str, iparser, len);
		iparser = end;
	}
	str[len] = 0;
	return len;
}

int parse_hex(char *str)
{
	int len;
	char *end;
	parse_spaces();
	end=iparser;
	while ( ((*end>='0')&&(*end<='9'))||((*end>='A')&&(*end<='F'))||((*end>='a')&&(*end<='f')) ) end++;
	if ( (len=end-iparser)>0 ) {
		if (len>=255) len=255; // check for length
		memcpy(str, iparser, len);
		iparser = end;
	}
	str[len] = 0;
	return len;
}

int parse_bin(char *str)
{
	int len;
	char *end;
	parse_spaces();
	end=iparser;
	while ( (*end=='0')||(*end=='1') ) end++;
	if ( (len=end-iparser)>0 ) {
		if (len>=255) len=255; // check for length
		memcpy(str, iparser, len);
		iparser = end;
	}
	str[len] = 0;
	return len;
}

int parse_expect( char c )
{
	parse_spaces();
	if (*iparser==c) {
		iparser++;
		return 1;
	}
	else return 0;
}

