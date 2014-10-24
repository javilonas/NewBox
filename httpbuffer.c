////
// File: httpbuffer.c
/////

// TCP DATA
#define MAX_BUFFERSIZE   2048 // 1024 default
char tcp_buffer[MAX_BUFFERSIZE];
int tcp_bufindex; // current pos.
int tcp_bufsize; // allocated size

struct tcp_buffer_data
{
	char data[MAX_BUFFERSIZE];
	int index; // current pos.
	int size; // allocated size
};

void tcp_init(struct tcp_buffer_data *buf)
{
	buf->size = sizeof(buf->data);
 	buf->index = 0;
}

void tcp_flush(struct tcp_buffer_data *buf, int sock)
{
	if (sock==INVALID_SOCKET) return;
	if (buf->index>0) {
		send(sock,buf->data,buf->index,MSG_NOSIGNAL);
		buf->index=0;
	}
}

void tcp_write(struct tcp_buffer_data *buf, int sock, const char *data, int size )
{
	int datapos=0;
	int bytes=0;
	if (sock==INVALID_SOCKET) return;
	while ( buf->index+size>=buf->size ) {
		bytes = buf->size - buf->index;
		memcpy( &buf->data[buf->index], &data[datapos], bytes);
		datapos += bytes;
		size -= bytes;
		buf->index += bytes;
		// send buffer
		tcp_flush(buf, sock);
	}
	if (size>0) {
		memcpy( &buf->data[buf->index], &data[datapos], size);
		buf->index += size;
	}
}

void tcp_writestr(struct tcp_buffer_data *buf, int sock, const char *data )
{
	tcp_write( buf, sock, data, strlen(data) );
}
