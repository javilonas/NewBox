
#define CWS_FIRSTCMDNO     0xE0


#define EXT_OSD_MESSAGE    0xD1
#define EXT_ADD_CARD       0xD3
#define EXT_REMOVE_CARD    0xD4
#define EXT_GET_VERSION    0xD6
#define EXT_SID_LIST       0xD7


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
  MSG_KEEPALIVE = CWS_FIRSTCMDNO + 0x1d
} net_msg_type_t;

#define CWS_NETMSGSIZE 728 //csp 0.8.9 (default: 400). This is CWS_NETMSGSIZE. The old default was 240

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
int cs_msg_chkrecv(int sock);

