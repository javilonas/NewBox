////
// File: msg-radegast.c
/////


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netdb.h> 
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "debug.h"
#include "sockets.h"
#include "msg-radegast.h"

///////////////////////////////////////////////////////////////////////////////

int rdgd_message_receive(int sock, unsigned char *buffer, int timeout)
{
	int len;
	unsigned char netbuf[300];

	if (sock==INVALID_SOCKET) {
		return -1;
	}

	len = recv_nonb(sock, netbuf, 2,timeout);
	if (len<=0) {
		return len; // disconnected
	}
	if (len != 2) {
		return -1;
	}
	len = recv_nonb(sock, netbuf+2, netbuf[1],timeout);
	if (len<=0) {
		return len; // disconnected
	}
	if (len != netbuf[1]) {
		return -1;
	}
	len += 2;
	if (flag_debugnet) {
		debugf(" radegast: receive data %d\n",len);
		debughex(netbuf,len);
	}
	memcpy(buffer, netbuf, len);
	return len;
}

///////////////////////////////////////////////////////////////////////////////

int rdgd_message_send(int sock, unsigned char *buf, int len)
{
	if (flag_debugnet) {
		debugf(" radegast: send data %d\n",len);
		debughex(buf,len);
	}
	return send_nonb( sock, buf, len, 10);
}

///////////////////////////////////////////////////////////////////////////////

// -1: not yet
// 0: disconnect
// >0: ok
int rdgd_check_message(int sock)
{
	int len;
	unsigned char netbuf[300];

	len = recv(sock, netbuf, 2, MSG_PEEK|MSG_NOSIGNAL|MSG_DONTWAIT);
	if (len==0) return 0;
	if (len!=2) return -1;

	int datasize = netbuf[1];
	len = recv(sock, netbuf, 2+datasize, MSG_PEEK|MSG_NOSIGNAL|MSG_DONTWAIT);

	if (len!=2+datasize) return -1;

	return len;
}

