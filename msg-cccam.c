////
// File: msg-cccam.c
/////


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include "common.h"
#include "debug.h"
#include "msg-cccam.h"
#include "sockets.h"


///////////////////////////////////////////////////////////////////////////////
void cc_crypt_swap(unsigned char *p1, unsigned char *p2)
{
  unsigned char tmp=*p1; *p1=*p2; *p2=tmp;
}

///////////////////////////////////////////////////////////////////////////////
void cc_crypt_init( struct cc_crypt_block *block, uint8 *key, int len)
{
  int i = 0 ;
  uint8 j = 0;

  for (i=0; i<256; i++) {
    block->keytable[i] = i;
  }

  for (i=0; i<256; i++) {
    j += key[i % len] + block->keytable[i];
    cc_crypt_swap(&block->keytable[i], &block->keytable[j]);
  }

  block->state = *key;
  block->counter=0;
  block->sum=0;
}

///////////////////////////////////////////////////////////////////////////////
// XOR init bytes with 'CCcam'
void cc_crypt_xor(uint8 *buf)
{
  const char cccam[] = "CCcam";
  uint8 i;

  for ( i = 0; i < 8; i++ ) {
    buf[8 + i] = i * buf[i];
    if ( i <= 5 ) {
      buf[i] ^= cccam[i];
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void cc_decrypt(struct cc_crypt_block *block, uint8 *data, int len)
{
  int i;
  uint8 z;

  for (i = 0; i < len; i++) {
    block->counter++;
    block->sum += block->keytable[block->counter];
    cc_crypt_swap(&block->keytable[block->counter], &block->keytable[block->sum]);
    z = data[i];
    data[i] = z ^ block->keytable[(block->keytable[block->counter] + block->keytable[block->sum]) & 0xff] ^ block->state;
    z = data[i];
    block->state = block->state ^ z;
  }
}

///////////////////////////////////////////////////////////////////////////////
void cc_encrypt(struct cc_crypt_block *block, uint8 *data, int len)
{
  int i;
  uint8 z;
  // There is a side-effect in this function:
  // If in & out pointer are the same, then state is xor'ed with modified input
  // (because output(=in ptr) is written before state xor)
  // This side-effect is used when initialising the encrypt state!
  for (i = 0; i < len; i++) {
    block->counter++;
    block->sum += block->keytable[block->counter];
    cc_crypt_swap(&block->keytable[block->counter], &block->keytable[block->sum]);
    z = data[i];
    data[i] = z ^ block->keytable[(block->keytable[block->counter] + block->keytable[block->sum]) & 0xff] ^ block->state;
    block->state = block->state ^ z;
  }
}

///////////////////////////////////////////////////////////////////////////////
// node_id : client nodeid, the sender of the ECM Request(big endian)
// card_id : local card_id for the server
void cc_crypt_cw(uint8 *nodeid/*client node id*/, uint32 card_id, uint8 *cws)
{
  uint8 tmp;
  int i;
  int n;
  uint8 nod[8];

  for(i=0; i<8; i++) nod[i] = nodeid[7-i];
  for (i = 0; i < 16; i++) {
    if (i&1)
	if (i!=15) n = (nod[i>>1]>>4) | (nod[(i>>1)+1]<<4); else n = nod[i>>1]>>4;
    else n = nod[i>>1];
    n = n & 0xff;
    tmp = cws[i] ^ n;
    if (i & 1) tmp = ~tmp;
    cws[i] = (card_id >> (2 * i)) ^ tmp;
    //printf("(%d) n=%02x, tmp=%02x, cw=%02x\n",i,n,tmp,cws[i]); 
  }
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Return:
// =0: Disconnected
// -1: Packet Error
// >0: Success


int cc_msg_recv(int handle,struct cc_crypt_block *recvblock, uint8 *buf, int timeout)
{
	int len;
	uint8 netbuf[CC_MAXMSGSIZE];

	if (handle < 0) return -1;

	len = recv_nonb(handle, netbuf, 4,timeout);

	if (len<=0) {
		debugf(" CCcam: recv error %d(%d)\n",len, errno);
		return len;
	}

	if (len != 4) { // invalid header length read
		debugf(" CCcam: invalid header length\n");
		//debugdump(netbuf, len, "Header:");
		return -1;
	}

	cc_decrypt(recvblock, netbuf, 4);
	//debugdump(netbuf, 4, "CCcam: decrypted header:");

	if (((netbuf[2] << 8) | netbuf[3]) != 0) {  // check if any data is expected in msg
		if (((netbuf[2] << 8) | netbuf[3]) > CC_MAXMSGSIZE - 2) {
			debugf(" CCcam: message too big\n");
			return -1;
		}

		len = recv_nonb(handle, netbuf+4, (netbuf[2] << 8) | netbuf[3],timeout);

		if (len != ((netbuf[2] << 8) | netbuf[3])) {
			debugf(" CCcam: invalid message length read %d(%d)\n",len,errno);
			return -1;
		}

		cc_decrypt(recvblock, netbuf+4, len);
		len += 4;
	}

	//debugdump(netbuf, len, "CCcam: Reveive Data");
	if (flag_debugnet) {
		debugf(" CCcam: receive data %d\n",len);
		debughex(netbuf,len);
	}
	memcpy(buf, netbuf, len);
	return len;
}



// -1: not yet
// 0: disconnect
// >0: ok
int cc_msg_chkrecv(int handle,struct cc_crypt_block *recvblock)
{
	int len;
	uint8 netbuf[CC_MAXMSGSIZE];
	struct cc_crypt_block block;

	if (handle<=0) return -1;

	//len = recv(handle, netbuf, 4, 0);
	len = recv(handle, netbuf, 4, MSG_PEEK|MSG_NOSIGNAL|MSG_DONTWAIT);
	if (len==0) return 0;
	if (len!=4) return -1;

	memcpy( &block, recvblock, sizeof(struct cc_crypt_block));
	cc_decrypt(&block, netbuf, 4);

	int datasize = (netbuf[2] << 8) | netbuf[3];
	if ( datasize!=0 ) {  // check if any data is expected in msg
		if ( datasize > CC_MAXMSGSIZE - 2) return 0; // Disconnect
		len = recv(handle, netbuf, 4+datasize, MSG_PEEK|MSG_NOSIGNAL|MSG_DONTWAIT);
		if (len==0) return 0;
		if (len != 4+datasize) return -1;
		cc_decrypt(&block, netbuf+4, len-4);
	}
	//debugf("CCcam: Check Reveive Data %d\n",datasize+4);
	return len;
}

///////////////////////////////////////////////////////////////////////////////
// Return:
// =0: Disconnected
// -1: Packet Error
// >0: Success
int cc_msg_recv_nohead(int handle, struct cc_crypt_block *recvblock, uint8 *buf, int len)
{
	if (handle < 0) return -1;
	len = recv_nonb(handle, buf, len, 2000);  // read rest of msg
	cc_decrypt(recvblock, buf, len);
	return len;
}

///////////////////////////////////////////////////////////////////////////////
int cc_msg_send(int handle,struct cc_crypt_block *sendblock, cc_msg_cmd cmd, int len, uint8 *buf)
{
  uint8 netbuf[CC_MAXMSGSIZE];

  memset(netbuf, 0, len+4);
  if (cmd == CC_MSG_NO_HEADER) memcpy(netbuf, buf, len);
  else {
    // build command message
    netbuf[0] = 0;   // flags??
    netbuf[1] = cmd & 0xff;
    netbuf[2] = len >> 8;
    netbuf[3] = len & 0xff;
    if (buf) memcpy(netbuf+4, buf, len);
    len += 4;
  }
  //debugdump(netbuf, len, "CCcam: Send data");
	if (flag_debugnet) {
		debugf(" CCcam: send data %d\n",len);
		debughex(netbuf,len);
	}
  cc_encrypt(sendblock, netbuf, len);
  return send_nonb(handle, netbuf, len, 10);
}

