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

struct trace_data {
	char host[32];
	int port;
	unsigned int ip;
	int sock;
	struct sockaddr_in addr;
};

extern int flag_debugscr;
extern int flag_debugnet;
extern int flag_debugfile;
extern char debug_file[256];

extern int flag_debugtrace;
extern struct trace_data trace;

#define MAX_DBGLINES 27 // 50 default
extern char dbgline[MAX_DBGLINES][512];
extern int idbgline;

char* debugtime(char *str);
void debug(char *str);
void debugf(char *format, ...);
void debughex(uint8 *buffer, int len);

void fdebug(char *str);
void fdebugf(char *format, ...);

