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
#include <poll.h>
#include <errno.h>

#include "common.h"
#include "tools.h"
#include "sockets.h"


// CONVERTION
uint32_t hostname2ip( char *hostname )
{
	struct hostent *phostent;
	unsigned int hostaddr;
	unsigned char *temp;

	phostent = gethostbyname(hostname);
	if (phostent==NULL) {
		//printf(" Error gethostbyname(%s)\n",hostname);
		return 0;
	}
	temp = ((unsigned char *) phostent->h_addr_list[0]);
	hostaddr = *(unsigned int*)temp;//   *(*temp<<24) + ( *(temp+1)<<16 ) + ( *(temp+2)<<8 ) + (*(temp+3));
	//printf("IP = %03d.%03d.%03d.%03d\n", *temp, *(temp+1), *(temp+2), *(temp+3));
	//if (hostaddr==0x7F000001) hostaddr=0;
	return hostaddr;
}

char *iptoa(char *dest, unsigned int ip )
{
	sprintf(dest,"%d.%d.%d.%d", 0xFF&(ip), 0xFF&(ip>>8), 0xFF&(ip>>16), 0xFF&(ip>>24));
	return dest;
}

char ip_string[3][0x40];
int32_t ip_string_counter = 0;
char *ip2string( unsigned int ip )
{
	ip_string_counter++; if (ip_string_counter>2) ip_string_counter = 0;
	return iptoa(ip_string[ip_string_counter], ip );
}

////////////////////////////////////////////////////////////////////////////////
// SOCKETS FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

int32_t fdstatus_read(SOCKET s)
{
	fd_set readfds;
	int32_t retval;
	struct timeval timeout;
	FD_ZERO(&readfds);
	FD_SET(s, &readfds);
	timeout.tv_usec = 0;
	timeout.tv_sec = 0;
	//do {
	retval = select(s+1, &readfds, NULL, NULL,&timeout); 
	//} while(retval<0 && errno==EINTR);
	return retval;
}

int32_t fdstatus_readt(SOCKET s, int32_t tim)
{
	fd_set readfds;
	int32_t retval;
	struct timeval timeout;

	FD_ZERO(&readfds);
	FD_SET(s, &readfds);
	timeout.tv_usec = (tim%1000)*1000;
	timeout.tv_sec = tim/1000;
 // do {
	retval = select(s+1, &readfds, NULL, NULL,&timeout); 
	//} while(retval<0 && errno==EINTR);
	return retval;
}

int32_t fdstatus_writet(SOCKET s, int32_t tim)
{
	fd_set writefds;
	int32_t retval;
	struct timeval timeout;

	FD_ZERO(&writefds);
	FD_SET(s, &writefds);
	timeout.tv_usec = (tim%1000)*1000;
	timeout.tv_sec = tim/1000;
	do {
	retval = select(s+1, NULL, &writefds, NULL,&timeout); 
	} while( (retval<0) && ( (errno==EINTR)||(errno==EAGAIN) ) );

	return retval;
}

int32_t fdstatus_write(SOCKET s)
{
	fd_set writefds;
	int32_t retval;
	struct timeval timeout;
	FD_ZERO(&writefds);
	FD_SET(s, &writefds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 100;
	do {
	retval = select(s+1, NULL, &writefds, NULL,&timeout); 
	} while ( (retval<0) && ( (errno==EINTR)||(errno==EAGAIN) ) );
	return retval;
}


int32_t fdstatus_accept(SOCKET s)
{
	fd_set fd;
	int32_t retval;
	struct timeval timeout;

	FD_ZERO(&fd);
	FD_SET(s, &fd);
	timeout.tv_usec = 0;
	timeout.tv_sec = 10;
	do {
	retval = select(FD_SETSIZE, &fd, NULL, NULL,&timeout); 
	} while(retval<0 && errno==EINTR);
	return retval;
}


int32_t SetSocketTimeout(SOCKET connectSocket, int32_t milliseconds)
{
	struct timeval tv;

	tv.tv_sec = milliseconds / 1000 ;
	tv.tv_usec = ( milliseconds % 1000) * 1000 ;

	return setsockopt (connectSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof tv);
}

/* Disable the Nagle (TCP No Delay) algorithm */
int32_t SetSocketNoDelay(int32_t sock)
{
	int32_t val = 1;
	if( setsockopt( sock, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0) return -1; 
	return 0;
}

int32_t SetSocketKeepalive(SOCKET sock)
{
	int32_t val = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) < 0) return -1; 
	val = 3;
	if(setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&val, sizeof(val)) < 0) return -1;
	if(setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, (void*)&val, sizeof(val)) < 0) return -1;
	val = 1;
	if(setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, (void*)&val, sizeof(val)) < 0) return -1;
	return 0;
}

/*
int32_t SetSocketPriority(int32_t sock)
{
	setsockopt(sock, SOL_SOCKET, SO_PRIORITY, (void *)&cfg->netprio, sizeof(ulong));
}
*/

///////////////////////////////////////////////////////////////////////////////
// UDP CONNECTION
///////////////////////////////////////////////////////////////////////////////

int32_t CreateServerSockUdp(int32_t port)
{
	int32_t reuse=1;
	SOCKET sock;
	struct sockaddr_in saddr;

	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock==INVALID_SOCKET) {
		printf("Error in INP int creation\n");
		return(INVALID_SOCKET);
	}
	saddr.sin_family = PF_INET;
	saddr.sin_addr.s_addr = htonl( INADDR_ANY );
	saddr.sin_port = htons(port);

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int32_t))< 0)
	{
		close(sock);
		printf("setsockopt() failed\n");
		return INVALID_SOCKET;
	}

	if ( bind( sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == -1 ) {
	printf("Error in bind INP int\n");
	close(sock);
	return(INVALID_SOCKET);
	}

	return( sock );
}

SOCKET CreateClientSockUdp()
{
	int32_t sock;
	sock = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if (sock==INVALID_SOCKET) {
		printf("Error: socket()\n");
		return INVALID_SOCKET;
	}
	return sock;
}


///////////////////////////////////////////////////////////////////////////////
// TCP CONNECTION
///////////////////////////////////////////////////////////////////////////////

SOCKET CreateServerSockTcp(int32_t port)
{
	SOCKET sock;
	struct sockaddr_in saddr;
 // Set up server
	saddr.sin_family = PF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(port);
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( sock==INVALID_SOCKET )
	{
		printf("socket() failed\n");
		return INVALID_SOCKET;
	}

	int32_t reuse=1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int32_t))< 0)
	{
		close(sock);
		printf("setsockopt(SO_REUSEADDR) failed\n");
		return INVALID_SOCKET;
	}

	if ( bind(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr))==SOCKET_ERROR )
	{
		close(sock);
		printf("bind() failed (Port:%d)\n",port);
		return INVALID_SOCKET;
	}
	if (listen(sock, 1) == SOCKET_ERROR)
	{
		close(sock);
		printf("listen() failed\n");
		return INVALID_SOCKET;
	}
	return sock;
}


SOCKET CreateClientSockTcp(unsigned int netip, int port)
{
	SOCKET sock;
	struct sockaddr_in saddr;

	sock = socket(PF_INET,SOCK_STREAM,0);
	if( sock<0 ) {
		//printf("Invalid Socket\n");
		return INVALID_SOCKET;
	}

	int32_t optVal = TRUE;
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&optVal, sizeof(int32_t))==-1) {
		close(sock);
		return INVALID_SOCKET;
	}

	memset(&saddr,0, sizeof(saddr));
	saddr.sin_family = PF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = netip;

	if( connect(sock,(struct sockaddr *)&saddr,sizeof(struct sockaddr_in)) != 0)
	{
		close(sock);
		return INVALID_SOCKET;
	}
	return sock;
}


///////////////////////////////////////////////////////////////////////////////
// NON BLOCKED TCP CONNECTION
///////////////////////////////////////////////////////////////////////////////

int CreateClientSockTcp_nonb(unsigned int netip, int port)
{
	int32_t ret, flags, error;
	socklen_t len;
	SOCKET sockfd;
	struct sockaddr_in saddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( sockfd<0 ) return INVALID_SOCKET;

	flags = fcntl(sockfd,F_GETFL);
	if (flags<0) {
		close(sockfd);
		printf("socket: fcntl GETFL failed\n");
		return INVALID_SOCKET;
	}
	if ( fcntl(sockfd,F_SETFL,flags|O_NONBLOCK)<0 ) {
		close(sockfd);
		printf("socket: fcntl SETFL failed\n");
		return INVALID_SOCKET;
	}

	memset(&saddr,0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = netip;
	errno = 0;
	do {
		ret = connect( sockfd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in) );
	} while ( ret && (errno==EINTR) );

	if (ret) {
		if (errno==EINPROGRESS || errno==EALREADY) {
			struct pollfd pfd;
			pfd.fd = sockfd;
			pfd.events = POLLOUT;
			errno = 0;
			do {
				ret = poll(&pfd, 1, 1000);
			} while (ret < 0 && errno == EINTR);
			if (ret < 0) {
				close(sockfd);
				return INVALID_SOCKET;
			}
			else if (ret == 0) {
				errno = ETIMEDOUT;
				close(sockfd);
				return INVALID_SOCKET;
			}
			else {
				if ( pfd.revents && (pfd.revents & POLLOUT) ) {
					len = sizeof(error);
					if ( getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) ) {
						close(sockfd);
						return INVALID_SOCKET;
					}
					if (error) {
						errno = error;
						close(sockfd);
						return INVALID_SOCKET;
					}
				}
				else {
					errno = ECONNABORTED;
					close(sockfd);
					return INVALID_SOCKET;
				}
			}
		}
		else if (errno!=EISCONN) {
			close(sockfd);
			return INVALID_SOCKET;
		}
	}

	flags &=~ O_NONBLOCK;
	fcntl(sockfd, F_SETFL, flags);	/* restore file status flags */

	return sockfd;
}

SOCKET CreateServerSockTcp_nonb(int32_t port)
{
	SOCKET sock;
	struct sockaddr_in saddr;
 // Set up server
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(port);

	struct protoent *ptrp;
	int32_t p_proto;
	if ((ptrp = getprotobyname("tcp"))) p_proto = ptrp->p_proto; else p_proto = 6;

	sock = socket(AF_INET, SOCK_STREAM, p_proto);
	if ( sock==INVALID_SOCKET )
	{
		printf("socket() failed\n");
		return INVALID_SOCKET;
	}

	int32_t flgs=fcntl(sock,F_GETFL);
	if(flgs<0) {
		close(sock);
		printf("socket: fcntl GETFL failed\n");
		return INVALID_SOCKET;
	}
	if ( fcntl(sock,F_SETFL,flgs|O_NONBLOCK)<0 ) {
		close(sock);
		printf("socket: fcntl SETFL failed\n");
		return INVALID_SOCKET;
	}

	int32_t reuse=1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int32_t))< 0)
	{
		close(sock);
		printf("setsockopt(SO_REUSEADDR) failed\n");
		return INVALID_SOCKET;
	}

	if ( bind(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr))==SOCKET_ERROR )
	{
		close(sock);
		printf("bind() failed (Port:%d)\n",port);
		return INVALID_SOCKET;
	}
	if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
	{
		close(sock);
		printf("listen() failed\n");
		return INVALID_SOCKET;
	}
	return sock;
}


int32_t recv_nonb1(SOCKET sock,uint8_t *buf,int32_t len,int32_t timeout)
{
	int32_t retval;
	int32_t index = 0;

	uint32_t ticks = GetTickCount()+timeout; // timeout ~ 2sec
	uint32_t now;
	do {
		now = GetTickCount();
		if ( ticks<now ) {
			//printf("recv_nonb(): receive timeout (got %d from %d)\n",index,len);
			return -2; // timeout
		}
/*
		errno = 0;
		retval = fdstatus_readt(sock,ticks-now);
		switch (retval) {
			case -1: // error
				if ( (errno==EINTR)||(errno==EAGAIN) ) continue;
				return -2;
			case 0: // timeout
				continue;
				return -2;
			default: // nb descriptors
*/
				do {
					errno = 0;
						retval = recv(sock, buf+index, len-index, MSG_NOSIGNAL|MSG_DONTWAIT);
				} while(retval<0 && errno==EINTR);
				switch (retval) {
					case -1:
						if ( (errno==0)||(errno==110)||(errno==EAGAIN)||(errno==EWOULDBLOCK)||(errno==EINTR) ) continue;
						//printf("recv_nonb(): recv() error(%d)\n", errno);
						return -1;
					case 0:
						if (!recv(sock, buf+index, len-index, MSG_NOSIGNAL|MSG_DONTWAIT)) { // retry
							//if (index) printf("Gotten data %d\n", index);
							//printf("recv_nonb err = 0 errno(%d)\n", errno);
							return -1;
						}
						else {
							//printf("recv_nonb(): err = 0 errno(%d) -> Saved\n", errno);
							return -2;
						}
						if (errno==EINTR) continue; //return index;
						else if (index==len) return index;
						else {
							//printf("recv_nonb(): err = 0 errno(%d)\n", errno);
							return -1;
						}
					default: index+=retval;
				}
//		}
	} while (index<len);
	return index; 
}

int32_t recv_nonb2(SOCKET sock,uint8_t *buf,int32_t len,int32_t timeout)
{
	int32_t retval;
	int32_t index = 0;

	uint32_t ticks = GetTickCount()+timeout; // timeout ~ 2sec
	uint32_t now;
	do {
		now = GetTickCount();
		if ( ticks<now ) {
			//printf("socket: receive timeout\n");
			return -2; // timeout
		}
		errno = 0;
		retval = fdstatus_readt(sock,ticks-now);
		switch (retval) {
			case -1: // error
				if (errno==EINTR) continue;
				if ( (errno==EAGAIN)||(errno==EWOULDBLOCK) ) continue;
				if ( (errno==0)||(errno==110) ) return -2;
				return -1;
			case 0: // timeout
				return -2;
			default: // nb descriptors
					retval = recv(sock, buf+index, len-index, MSG_NOSIGNAL);
				switch (retval) {
					case -1:
						if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) return -1;
					case 0:
						errno = ECONNRESET;
						return -1;
					default: index+=retval;
				}
		}
	} while (index<len);
	return index; 
}


// >0 : received ok
// =0 : disconnected
// =-1 : error
// =-2 : timeout
int32_t recv_nonb(SOCKET sock,uint8_t *buf,int32_t len,int32_t timeout)
{
	int32_t ret;
	int32_t index = 0;

	uint32_t last = GetTickCount()+timeout;
	while (1) {
		uint32_t now = GetTickCount();
		if (now>last) return -2; // timeout
				struct pollfd pfd;
				pfd.fd = sock;
				pfd.events = POLLIN | POLLPRI;
				ret = poll(&pfd, 1, last-now);
		if (ret>0) {
			if ( pfd.revents & (POLLIN|POLLPRI) ) {
				ret = recv( sock, buf+index, len-index, MSG_NOSIGNAL|MSG_DONTWAIT );
				if (ret>0) {
					index+=ret;
					if (index==len) return index;
				}
				else if (ret==0) return 0; // disconected
				else if ( (ret==-1)&&(errno!=EAGAIN)&&(errno!=EWOULDBLOCK)&&(errno!=EINTR) ) return -1; // error
			}
			if ( pfd.revents & (POLLHUP|POLLNVAL) ) return 0; // disconnected
		}
		else if (ret==0) return -2; // timeout
		else return -1; // error
	}
}


int32_t send_nonb2(SOCKET sock,uint8_t *buf,int32_t len,int32_t to)
{
	int32_t retval;
	int32_t index = 0;

	uint32_t ticks = GetTickCount()+to;
	uint32_t now;
	do {

		now = GetTickCount();
		if ( ticks<now ) {
			printf("send error timeout\n");
			return -2; // timeout
		}

		fd_set writefds;
		struct timeval timeout;
		timeout.tv_usec = (to%1000)*1000;
		timeout.tv_sec = to/1000;
		FD_ZERO(&writefds);
		FD_SET(sock, &writefds);
		retval = select( sock+1, NULL, &writefds, NULL, &timeout );

		switch (retval) {
			case -1: // error
				if ( (errno == EINTR)||(errno==EWOULDBLOCK)||(errno==EAGAIN)||(errno==0) ) continue;
					printf("send error %d(%d)\n",retval,errno);
					return retval; // disconnection
			case 0: // timeout
				printf("send error timeout\n");
				return retval;
			default: // nb. desriptors
				do {
						retval = send(sock, buf+index, len-index, MSG_NOSIGNAL|MSG_DONTWAIT);
				} while( (retval<0)&&((errno==EINTR)||(errno==EWOULDBLOCK)||(errno==EAGAIN)||(errno==0)) );
				if(retval>0) index+=retval;
		}
	} while (retval>0 && index<len);
	if (index!=len) printf("send_nonb err %d!=%d\n",index,len);
	if (retval>0) return index; else return -1;
}


int32_t send_nonb(SOCKET sock,uint8_t *buf,int32_t len,int32_t to)
{
	int32_t remain, got;
	uint8_t *ptr;
	int32_t error;
	struct timeval timeout;

	error = 0;
	timeout.tv_usec = (to%1000)*1000;
	timeout.tv_sec = to/1000;
	remain = len;
	ptr = buf;

	uint32_t ticks = GetTickCount()+to;
	uint32_t now;
	while (remain) {
		now = GetTickCount();
		if ( ticks<now ) {
			//printf("send_nonb(): timeout\n");
			return FALSE; // timeout
		}
/*		FD_ZERO(&writefds);
		FD_SET(sock, &writefds);
		error = select( sock+1, NULL, &writefds, NULL, &timeout );
//		if (error == 0) {
//			errno = ETIMEDOUT;
//			return FALSE;

//		} else 

		if (error < 0) {
			if ( (!errno)||(errno==EINTR)||(errno==EWOULDBLOCK)||(errno==EAGAIN) ) continue;
			return -1;
		}
		else {
*/
			got = send(sock, (void *) ptr, (size_t) remain, MSG_NOSIGNAL|MSG_DONTWAIT);
			if (got >= 0) {
				remain -= got;
				ptr += got;
			} else if (
				errno != EWOULDBLOCK &&
				errno != EAGAIN &&
				errno != EINTR &&
				errno != 0
			) {
				//printf(" send_nonb: error(%d) (sent %d from %d)\n", errno, len-remain,len );
				return FALSE;
			}
//		}
	}
	return TRUE;
}

