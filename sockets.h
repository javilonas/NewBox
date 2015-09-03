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

unsigned int hostname2ip( const char *hostname );
char *ip2string( unsigned int hostaddr );
char *iptoa(char *dest, unsigned int ip );

int fdstatus_read(int s);
int fdstatus_readt(int s, int tim);
int fdstatus_write(int s);
int fdstatus_accept(int s);
// SOCKET OPTIONS
int SetSocketTimeout(int connectSocket, int milliseconds);
int SetSocketNoDelay(int sock);
int SetSocketKeepalive(int sock);
// UDP CONNECTION
SOCKET CreateServerSockUdp(int port);
SOCKET CreateClientSockUdp();
// TCP CONNECTION
SOCKET CreateServerSockTcp(int port);
SOCKET CreateClientSockTcp(unsigned int netip, int port);
// TCP NON BLOCKED CONNECTION
int CreateClientSockTcp_nonb(unsigned int netip, int port);
int recv_nonb(SOCKET sock,uint8 *buf,int len,int timeout);
int send_nonb(SOCKET sock,uint8 *buf,int len,int to);
SOCKET CreateServerSockTcp_nonb(int port);
