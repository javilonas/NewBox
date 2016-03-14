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

// TCP DATA
#define MAX_BUFFERSIZE   1024
char tcp_buffer[MAX_BUFFERSIZE];
int32_t tcp_bufindex; // current pos.
int32_t tcp_bufsize; // allocated size

struct tcp_buffer_data
{
	char data[MAX_BUFFERSIZE];
	int32_t index; // current pos.
	int32_t size; // allocated size
};

void tcp_init(struct tcp_buffer_data *buf)
{
	buf->size = sizeof(buf->data);
	buf->index = 0;
}

void tcp_flush(struct tcp_buffer_data *buf, int32_t sock)
{
	if (sock==INVALID_SOCKET) return;
	if (buf->index>0) {
		send(sock,buf->data,buf->index,MSG_NOSIGNAL);
		buf->index=0;
	}
}

void tcp_write(struct tcp_buffer_data *buf, int32_t sock, const char *data, int32_t size )
{
	int32_t datapos=0;
	int32_t bytes=0;
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

void tcp_writestr(struct tcp_buffer_data *buf, int32_t sock, const char *data )
{
	tcp_write( buf, sock, data, strlen(data) );
}
