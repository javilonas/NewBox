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


int32_t srvsocks[2];

// PIPE COMMANDS
#define PIPE_LOCK                1
#define PIPE_WAKEUP              2

#define PIPE_CACHE_FIND          11
#define PIPE_CACHE_FIND_FAILED   12
#define PIPE_CACHE_FIND_WAIT     13
#define PIPE_CACHE_FIND_SUCCESS  14
#define PIPE_CACHE_REQUEST       15
#define PIPE_CACHE_REPLY         16



// to check for EINTR
int32_t pipe_read( int32_t fd, uchar *buf, int32_t len )
{
	int32_t readlen;
	while (1) {
		readlen = read( fd, buf, len);
		if (readlen==-1) {
			if (errno==EINTR) continue;
			else {
				debugf(" pipe(%d): read failed (errno=%d)", fd, errno);
				return -1;
			}
		}
		break;
	}
	return readlen;
}

// to check for EINTR
int32_t pipe_write( int32_t fd, uchar *buf, int32_t len )
{
	int32_t writelen;
	while (1) {
		writelen = write( fd, buf, len);
		if (writelen==-1) {
			if (errno==EINTR) continue;
			else {
				debugf(" pipe(%d): write failed (errno=%d)", fd, errno);
				return -1;
			}
		}
		break;
	}
	return writelen;
}

int32_t pipe_purge( int32_t fd )
{
	uchar rbuf[1024];
	while(1) {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET( fd, &readfds);
		struct timeval timeout;
		timeout.tv_usec = 0;
		timeout.tv_sec = 0;
		int32_t retval = select(fd+1, &readfds, NULL, NULL,&timeout);
		if ( retval>0 ) {
			read( fd, rbuf, sizeof(rbuf) );
		}
		else break;
	}
	return 0;
}

int32_t pipe_recv( int32_t fd, uchar *buf )
{
	uchar rbuf[1024];
	int32_t rlen;

	rlen = pipe_read( fd, rbuf, 4);
	if (rlen!=4) {
		debugf(" pipe(%d): header recv error (rlen=%d)\n",fd,rlen);
		pipe_purge(fd); // wrong data --> purge
		return 0;
	}
	if (rbuf[0]!=0xFF) {
		debugf(" pipe(%d): recv error, wrong id %02X\n", fd,rbuf[0]);
		pipe_purge(fd); // wrong data --> purge
		return 0;
	}
	int32_t len = (rbuf[2]<<8)|rbuf[3];
	rlen += pipe_read( fd, rbuf+4, len);
	if ( rlen!= len+4 ) {
		debugf(" pipe(%d): recv error (rlen=%d)\n",fd,rlen);
		pipe_purge(fd); // wrong data --> purge
		return 0;
	}
	//Checksum
	int32_t i;
	uchar sum =0;
	for(i=4; i<rlen; i++) sum ^= rbuf[i];
	if (sum!=rbuf[1]) {
		debugf(" pipe(%d): recv error, wrong checksum\n",fd);
		pipe_purge(fd); // wrong data --> purge
		return 0;
	}
	memcpy(buf, rbuf+4, len);
	//debugf(" pipe(%d): recv data\n"); debughex(rbuf, rlen);
	return len;
}

int32_t pipe_send( int32_t fd, uchar *buf, int32_t len )
{
	uchar wbuf[1024];
	//ID (check for errors)
	wbuf[0] = 0xFF;
	//Checksum
	int32_t i;
	uchar sum =0;
	for(i=0; i<len; i++) sum ^= buf[i];
	wbuf[1] = sum;
	//LENGTH
	wbuf[2] = (len>>8)&0x0f;
	wbuf[3] = len&0xff;
	// DATA
	int32_t wlen = 4+len;
	memcpy(wbuf+4, buf, len);
	//debugf(" pipe(%d): write data\n",fd); debughex(buf, len);
	return pipe_write( fd, wbuf, wlen);
}

void pipe_cmd( int32_t pfd, int32_t cmd )
{
	uchar buf[2];
	buf[0] = cmd;
	buf[1] = 0;
	pipe_send( pfd, buf, 2);
}

void pipe_lock( int32_t pfd )
{
	uchar buf[2];
	buf[0] = PIPE_LOCK;
	buf[1] = 0;
	pipe_send( pfd, buf, 2);
}

void pipe_wakeup( int32_t pfd )
{
	uchar buf[2];
	buf[0] = PIPE_WAKEUP;
	buf[1] = 0;
	pipe_send( pfd, buf, 2);
}

