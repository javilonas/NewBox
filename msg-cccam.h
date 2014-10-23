// CCcam Cryptage Functions

struct cc_crypt_block
{
	uint8 keytable[256];
	uint8 state;
	uint8 counter;
	uint8 sum;
} __attribute__((packed));



void cc_crypt_swap(unsigned char *p1, unsigned char *p2);
void cc_crypt_init( struct cc_crypt_block *block, uint8 *key, int len);
void cc_crypt_xor(uint8 *buf);
void cc_decrypt(struct cc_crypt_block *block, uint8 *data, int len);
void cc_encrypt(struct cc_crypt_block *block, uint8 *data, int len);
void cc_crypt_cw(uint8 *nodeid, uint32 card_id, uint8 *cws);


// CCcam Connection Functions

#define CC_MAXMSGSIZE	2512

typedef enum
{
  CC_MSG_CLI_INFO,		// client -> server
  CC_MSG_ECM_REQUEST,		// client -> server
  CC_MSG_EMM_REQUEST,		// client -> server
  CC_MSG_CARD_DEL = 4,		// server -> client
  CC_MSG_BAD_ECM,
  CC_MSG_KEEPALIVE,		// client -> server
  CC_MSG_CARD_ADD,		// server -> client
  CC_MSG_SRV_INFO,		// server -> client
  CC_MSG_CMD_0B = 0x0b,	        // server -> client ???????
  CC_MSG_ECM_NOK1 = 0xfe,	// server -> client ecm queue full, card not found
  CC_MSG_ECM_NOK2 = 0xff,	// server -> client
  CC_MSG_NO_HEADER = 0xffff
} cc_msg_cmd;

int cc_msg_recv(int handle,struct cc_crypt_block *recvblock, uint8 *buf, int timeout);
int cc_msg_recv_nohead(int handle, struct cc_crypt_block *recvblock, uint8 *buf, int len);
int cc_msg_send(int handle,struct cc_crypt_block *sendblock, cc_msg_cmd cmd, int len, uint8 *buf);
int cc_msg_chkrecv(int handle,struct cc_crypt_block *recvblock);

