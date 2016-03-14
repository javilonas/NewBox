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

uint32_t hostname2ip( char *hostname );
char *ip2string( unsigned int hostaddr );
char *iptoa(char *dest, unsigned int ip );

int32_t fdstatus_read(int32_t s);
int32_t fdstatus_readt(int32_t s, int32_t tim);
int32_t fdstatus_write(int32_t s);
int32_t fdstatus_accept(int32_t s);
// SOCKET OPTIONS
int32_t SetSocketTimeout(int32_t connectSocket, int32_t milliseconds);
int32_t SetSocketNoDelay(int32_t sock);
int32_t SetSocketKeepalive(int32_t sock);
// UDP CONNECTION
SOCKET CreateServerSockUdp(int32_t port);
SOCKET CreateClientSockUdp();
// TCP CONNECTION
SOCKET CreateServerSockTcp(int32_t port);
SOCKET CreateClientSockTcp(unsigned int netip, int port);

// TCP NON BLOCKED CONNECTION
int CreateClientSockTcp_nonb(unsigned int netip, int port);
int32_t recv_nonb(SOCKET sock,uint8_t *buf,int32_t len,int32_t timeout);
int32_t send_nonb(SOCKET sock,uint8_t *buf,int32_t len,int32_t to);
SOCKET CreateServerSockTcp_nonb(int32_t port);
