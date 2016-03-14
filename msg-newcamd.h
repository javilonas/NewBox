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

#define EXT_OSD_MESSAGE    0xD1
#define EXT_ADD_CARD       0xD3
#define EXT_REMOVE_CARD    0xD4
#define EXT_GET_VERSION    0xD6
#define EXT_SID_LIST       0xD7

#define CWS_NETMSGSIZE 600 //csp 0.8.9 (default: 400). This is CWS_NETMSGSIZE. The old default was 240

#define NCD_CLIENT_ID 0x8888

#define CWS_FIRSTCMDNO 0xe0
typedef enum
{
  MSG_CLIENT_2_SERVER_LOGIN = CWS_FIRSTCMDNO,
  MSG_CLIENT_2_SERVER_LOGIN_ACK,
  MSG_CLIENT_2_SERVER_LOGIN_NAK,
  MSG_CARD_DATA_REQ,
  MSG_CARD_DATA,
  MSG_SERVER_2_CLIENT_NAME,
  MSG_SERVER_2_CLIENT_NAME_ACK,
  MSG_SERVER_2_CLIENT_NAME_NAK,
  MSG_SERVER_2_CLIENT_LOGIN,
  MSG_SERVER_2_CLIENT_LOGIN_ACK,
  MSG_SERVER_2_CLIENT_LOGIN_NAK,
  MSG_ADMIN,
  MSG_ADMIN_ACK,
  MSG_ADMIN_LOGIN,
  MSG_ADMIN_LOGIN_ACK,
  MSG_ADMIN_LOGIN_NAK,
  MSG_ADMIN_COMMAND,
  MSG_ADMIN_COMMAND_ACK,
  MSG_ADMIN_COMMAND_NAK,
  MSG_KEEPALIVE = CWS_FIRSTCMDNO + 0x1d,
  MSG_SERVER_2_CLIENT_OSD = 0xd1,
  MSG_SERVER_2_CLIENT_ALLCARDS = 0xd2,
  MSG_SERVER_2_CLIENT_ADDCARD = 0xd3,
  MSG_SERVER_2_CLIENT_REMOVECARD = 0xd4,
  MSG_SERVER_2_CLIENT_CHANGE_KEY = 0xd5,
  MSG_SERVER_2_CLIENT_GET_VERSION = 0xd6,
  MSG_SERVER_2_CLIENT_ADDSID = 0xd7,
  MSG_CLIENT_2_SERVER_CARDDISCOVER = 0xd8
} net_msg_type_t;

struct cs_custom_data
{
  unsigned short msgid;
  unsigned short sid;
  unsigned short caid;
  unsigned int provid;
};
/*
      netbuf[4] = cd->sid >> 8;
      netbuf[5] = cd->sid & 0xff;
      netbuf[6] = cd->caid >> 8;
      netbuf[7] = cd->caid & 0xff;
      netbuf[8] = (cd->provid >> 16) & 0xFF;
      netbuf[9] = (cd->provid >> 8) & 0xff;
      netbuf[10] = cd->provid & 0xff;
*/

void cs_init();
int cs_message_send(int sock,struct cs_custom_data *cd, unsigned char *buffer, int len, unsigned char *deskey);
int cs_message_receive(int sock,struct cs_custom_data *cd, unsigned char *buffer, unsigned char *deskey, int timeout);
int32_t cs_msg_chkrecv(int32_t sock);

