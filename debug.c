/////
// File: debug.c
/////


#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#include "common.h"
#include "debug.h"

int flag_debugtrace=0;
struct trace_data trace;

char dbgline[MAX_DBGLINES][512];
int idbgline = 0;

void add_dbgline(char *line)
{
	strcpy( dbgline[idbgline], line );
	idbgline++;
	if (idbgline>=MAX_DBGLINES) idbgline = 0;
}

void encryptstr( unsigned char *src, unsigned char *dest)
{
	unsigned char last = 0;
	*dest= 0;
	dest++;
	while(*src) {
		*dest = *src^0xA5^last;
		last = *src;
		src++;
		dest++;
	}
	*dest = 0;
}

void decryptstr( char *src, char *dest)
{
	unsigned char last = 0;
	if (*src!=0) return;
	src++;
	while (*src) {
		*dest = *src^0xA5^last;
		last = *dest;
		src++;
		dest++;
	}
	*dest = 0;
}

void debug(char *str)
{
	add_dbgline(str);
	if (flag_debugscr) printf( "%s", str );

	if (flag_debugfile) {
		FILE *fhandle;
		fhandle=fopen(debug_file, "at");
		if (fhandle!=0) {
			fprintf(fhandle,"%s", str );
			fclose(fhandle);
		}
	}

	if ( flag_debugtrace ) sendto(trace.sock, str, strlen(str), 0, (struct sockaddr *)&trace.addr, sizeof(trace.addr) );
}

char* debugtime(char *str)
{
    struct timeval tv;
    gettimeofday( &tv, NULL );
    int ms = tv.tv_usec / 1000;
    int hr = (1+(tv.tv_sec/3600)) % 24;
    int mn = (tv.tv_sec % 3600) /60;
    int sd = (tv.tv_sec % 60);
    sprintf( str, "[%02d:%02d:%02d.%03d]", hr,mn,sd,ms);
    return str;
}



// Formatted Debug
void debugf(char *format, ...)
{
	int index;
	char debugline[512];
	char fstr[512];
	if (format[0]==0) { // DECRYPT
		decryptstr(format, fstr);
	} else strcpy(fstr, format);

	if (fstr[0]==' ') { // ADD TIME
		struct timeval tv;
		gettimeofday( &tv, NULL );
		int ms = tv.tv_usec / 1000;
		int hr = (1+(tv.tv_sec/3600)) % 24;
		int mn = (tv.tv_sec % 3600) /60;
		int sd = (tv.tv_sec % 60);
		sprintf( debugline, "[%02d:%02d:%02d.%03d]", hr,mn,sd,ms);
		index = strlen(debugline);
	} else index=0;
	
	va_list args;
	va_start (args, fstr);
	vsprintf( debugline+index, fstr, args);
	va_end( args );

	debug(debugline);
}

#define DUMP_LENGTH 0x10
void debughex(uint8 *buffer, int len)
{
	int i;
	for ( i = 0; i < len; ++i ) {
		if (!(i%DUMP_LENGTH)) debugf(" \t  %04x: ",i);
		debugf("%02X ", buffer[i]);
		if (!((i+1)%DUMP_LENGTH)) debug("\n");
	}
	if (i%DUMP_LENGTH) debug("\n");
}


void fdebug(char *str)
{
	FILE *fhandle;
//	sprintf( fdebug_file,"%s.log", config_file);
	fhandle=fopen(debug_file, "at");
	if (fhandle!=0) {
		fprintf(fhandle,"%s", str );
		fclose(fhandle);
	}
}

// Formatted fdebug
void fdebugf(char *format, ...)
{
	int index;
	char fdebugline[512];
	if (format[0]==' ') { // ADD TIME
		struct timeval tv;
		gettimeofday( &tv, NULL );
		int ms = tv.tv_usec / 1000;
		int hr = (1+(tv.tv_sec/3600)) % 24;
		int mn = (tv.tv_sec % 3600) /60;
		int sd = (tv.tv_sec % 60);
		sprintf( fdebugline, "[%02d:%02d:%02d.%03d]", hr,mn,sd,ms);
		index = strlen(fdebugline);
	} else index=0;
	
	va_list args;
	va_start (args, format);
	vsprintf( fdebugline+index, format, args);
	va_end( args );

	fdebug(fdebugline);
}

