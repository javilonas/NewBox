////
// File: pipe.c
/////


int srvsocks[2];

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
int pipe_read( int fd, uchar *buf, int len )
{
	int readlen;
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
int pipe_write( int fd, uchar *buf, int len )
{
	int writelen;
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

int pipe_purge( int fd )
{
	uchar rbuf[1024];
	while(1) {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET( fd, &readfds);
		struct timeval timeout;
		timeout.tv_usec = 0;
		timeout.tv_sec = 0;
		int retval = select(fd+1, &readfds, NULL, NULL,&timeout);
		if ( retval>0 )	{
			read( fd, rbuf, sizeof(rbuf) );
		}
		else break;
	}
	return 0;
}

int pipe_recv( int fd, uchar *buf )
{
	uchar rbuf[1024];
	int rlen;

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
	int len = (rbuf[2]<<8)|rbuf[3];
	rlen += pipe_read( fd, rbuf+4, len);
	if ( rlen!= len+4 ) {
		debugf(" pipe(%d): recv error (rlen=%d)\n",fd,rlen);
		pipe_purge(fd); // wrong data --> purge
		return 0;
	}
	//Checksum
	int i;
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

int pipe_send( int fd, uchar *buf, int len )
{
	uchar wbuf[1024];
	//ID (check for errors)
	wbuf[0] = 0xFF;
	//Checksum
	int i;
	uchar sum =0;
	for(i=0; i<len; i++) sum ^= buf[i];
	wbuf[1] = sum;
	//LENGTH
	wbuf[2] = (len>>8)&0x0f;
	wbuf[3] = len&0xff;
	// DATA
	int wlen = 4+len;
	memcpy(wbuf+4, buf, len);
	//debugf(" pipe(%d): write data\n",fd); debughex(buf, len);
	return pipe_write( fd, wbuf, wlen);
}	

void pipe_cmd( int pfd, int cmd )
{
	uchar buf[2];
	buf[0] = cmd;
	buf[1] = 0;
	pipe_send( pfd, buf, 2);
}

void pipe_lock( int pfd )
{
	uchar buf[2];
	buf[0] = PIPE_LOCK;
	buf[1] = 0;
	pipe_send( pfd, buf, 2);
}

void pipe_wakeup( int pfd )
{
	uchar buf[2];
	buf[0] = PIPE_WAKEUP;
	buf[1] = 0;
	pipe_send( pfd, buf, 2);
}

