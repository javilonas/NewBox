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

#include "rc6.h"
#include "idea.h"

#define CC_MAXMSGSIZE 0x400 //by Project::Keynation: Buffer size is limited on "O" CCCam to 1024 bytes
#define CC_MAX_PROV   32
#define SWAPC(X, Y) do { char p; p = *X; *X = *Y; *Y = p; } while(0)

// CCcam Cryptage Functions

struct cc_crypt_block
{
	uint8_t keytable[256];
	uint8_t state;
	uint8_t counter;
	uint8_t sum;
} __attribute__((packed));

struct cc_srvid
{
	uint16_t sid;
	uint16_t chid;
	uint8_t ecmlen;
};

struct cc_srvid_block
{
	uint16_t sid;
	uint16_t chid;
	uint8_t  ecmlen;
	time_t   blocked_till;
};

struct cc_provider
{
	uint32_t prov;  //provider
	uint8_t sa[4]; //shared address
};

void cc_crypt_swap(unsigned char *p1, unsigned char *p2);
void cc_crypt_init( struct cc_crypt_block *block, uint8 *key, int len);
void cc_crypt_xor(uint8 *buf);
void cc_decrypt(struct cc_crypt_block *block, uint8 *data, int len);
void cc_encrypt(struct cc_crypt_block *block, uint8 *data, int len);
void cc_crypt_cw(uint8 *nodeid, uint32 card_id, uint8 *cws);


// CCcam Connection Functions

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
  CC_MSG_CMD_0A = 0x0a,
  CC_MSG_CMD_0B = 0x0b,
  CC_MSG_CMD_0C = 0x0c, // CCCam 2.2.x fake client checks
  CC_MSG_CMD_0D = 0x0d, // "
  CC_MSG_CMD_0E = 0x0e, // "
  CC_MSG_NEW_CARD_SIDINFO = 0x0f,
  CC_MSG_SLEEPSEND = 0x80, //Sleepsend support
  CC_MSG_ECM_NOK1 = 0xfe,	// server -> client ecm queue full, card not found
  CC_MSG_ECM_NOK2 = 0xff,	// server -> client
  CC_MSG_NO_HEADER = 0xffff
} cc_msg_cmd;

int cc_msg_recv(int handle,struct cc_crypt_block *recvblock, uint8 *buf, int timeout);
int cc_msg_recv_nohead(int handle, struct cc_crypt_block *recvblock, uint8 *buf, int len);
int cc_msg_send(int handle,struct cc_crypt_block *sendblock, cc_msg_cmd cmd, int len, uint8 *buf);
int cc_msg_chkrecv(int handle,struct cc_crypt_block *recvblock);

