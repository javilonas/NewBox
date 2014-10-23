int rdgd_message_receive(int sock, unsigned char *buffer, int timeout);
int rdgd_message_send(int sock, unsigned char *buf, int len);
int rdgd_check_message(int sock);

