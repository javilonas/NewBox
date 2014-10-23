////
// File: msg-newcamd.c
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
#include <pthread.h>

#include "common.h"
#include "des.h"
#include "debug.h"
#include "sockets.h"
#include "msg-newcamd.h"

int cs_message_send(int sock,struct cs_custom_data *cd, unsigned char *buffer, int len, unsigned char *deskey)
{
	unsigned char netbuf[CWS_NETMSGSIZE];

	if (sock==INVALID_SOCKET) return 0;

	if (!len) len = 0x0fff & ( (buffer[1]<<8)|buffer[2] );
	else {
		buffer[1] = (buffer[1] & 0xf0) | (((len - 3) >> 8) & 0x0f);
		buffer[2] = (len - 3) & 0xff;
	}

	if (len < 3 || len + 12 > CWS_NETMSGSIZE) return -1;

	//NetBuf Header

	memset(netbuf, 0, 12);
	if (cd) {
		netbuf[2] = (cd->msgid >> 8) & 0xff; 
		netbuf[3] = cd->msgid & 0xff;
		netbuf[4] = (cd->sid >> 8) & 0xff;
		netbuf[5] = cd->sid & 0xff;
		netbuf[6] = (cd->caid >> 8) & 0xff;
		netbuf[7] = cd->caid & 0xff;
		netbuf[8] = (cd->provid >> 16) & 0xff;
		netbuf[9] = (cd->provid >> 8) & 0xff;
		netbuf[10] = cd->provid & 0xff;
		netbuf[11] = (cd->provid >> 24) & 0xff; // used for mgcamd
	}
	memcpy(netbuf+12, buffer, len);
	// adding packet header size
	len += 12;

	//debugdump(netbuf,len,"SEND BUF ");
	if (flag_debugnet) {
		debugf(" newcamd: send data %d\n",len);
		debughex(netbuf,len);
	}

	len=des_encrypt(netbuf, len, deskey);

	if (len < 0) return -1;
	netbuf[0] = (len - 2) >> 8;
	netbuf[1] = (len - 2) & 0xff;

	return send_nonb(sock, netbuf, len,100);
}


// -2: timeout
// -1: Error
// =0: disconnect
// >0: ok
int cs_message_receive(int sock,struct cs_custom_data *cd, unsigned char *buffer, unsigned char *deskey, int timeout)
{
  int len;
  unsigned char netbuf[CWS_NETMSGSIZE];
  int returnLen;

  if (sock==INVALID_SOCKET) {
	//printf("newcamd: Invalid Socket\n");
	return -1;
  }

  len = recv_nonb(sock, netbuf, 2,timeout);

  if (len<=0) {
	//printf("newcamd: first recive error (%d)\n",errno);
	return len; // disconnected
  }
  if (len != 2) {
	//printf("newcamd: header length error (%d)\n",len);
	return -1;
  }
  if (((netbuf[0] << 8) | netbuf[1]) > CWS_NETMSGSIZE - 2) {
	//printf("newcamd: big message size (%d)\n", (netbuf[0] << 8) | netbuf[1] );
	return -1;
  }
  int netlen = (netbuf[0] << 8) | netbuf[1];
  len = recv_nonb(sock, netbuf+2, netlen,timeout);
  if (len<=0) {
	//printf("newcamd: recive error\n");
	return len; // disconnected
  }
  if (len!=netlen) {
	//printf("newcamd: data length error\n");
	return -1;
  }
  len += 2;

  len= des_decrypt(netbuf, len, deskey);

  if (len < 15) {
	//printf("newcamd: Error des_decrypt length %d\n",len);
    //debug_dump(netbuf, len, "netbuf:");
	return -2;
  }

  if ( (returnLen = (((netbuf[13] & 0x0f) << 8) | netbuf[14]) + 3)  > len-12) {
	//printf("newcamd: wrong data length\n");
	return -1;
  }

  //debugdump(netbuf,returnLen+12,"RECV BUF ");
	if (flag_debugnet) {
		debugf(" newcamd: receive data %d\n",returnLen+12);
		debughex(netbuf,returnLen+12);
	}

	// Setup Custom Data
	if (cd) {
		cd->msgid = (netbuf[2] << 8) | netbuf[3];
		cd->sid = (netbuf[4] << 8) | netbuf[5];
		cd->caid = (netbuf[6] << 8) | netbuf[7];
		cd->provid = (netbuf[8] << 16) | (netbuf[9] << 8) | netbuf[10];
	}
  memcpy(buffer, netbuf+12, returnLen);
  return returnLen;
}


// -1: not yet
// 0: disconnect
// >0: ok
int cs_msg_chkrecv(int sock)
{
	int len;
	unsigned char netbuf[CWS_NETMSGSIZE];

	len = recv(sock, netbuf, 2, MSG_PEEK|MSG_NOSIGNAL|MSG_DONTWAIT);

	if (len==0) return 0;
	if (len!=2) return -1;

	int datasize = (netbuf[0] << 8) | netbuf[1];
	if ( datasize > CWS_NETMSGSIZE-2) return 0;

	len = recv(sock, netbuf, 2+datasize, MSG_PEEK|MSG_NOSIGNAL|MSG_DONTWAIT);

	if (len!=2+datasize) return -1;

	return len;
}

