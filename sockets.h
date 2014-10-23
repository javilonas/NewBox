
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
