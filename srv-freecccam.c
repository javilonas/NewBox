////
// File: srv-freecccam.c
/////


void freecc_cli_recvmsg(struct cc_client_data *cli);



///////////////////////////////////////////////////////////////////////////////
// FREECCCAM SERVER: CONNECT CLIENTS
///////////////////////////////////////////////////////////////////////////////

void frcc_sendcards_cli(struct cc_client_data *cli)
{
	int nbcard=0;
	struct cardserver_data *cs = cfg.cardserver;

	int i;
	if (cfg.freecccam.csport[0]) {
		for(i=0;i<MAX_CSPORTS;i++) {
			if(cfg.freecccam.csport[i]) {
				cs = getcsbyport(cfg.freecccam.csport[i]);
				if (cs)
					if (cc_sendcard_cli(cs, cli, 0)) nbcard++;
			} else break;
		}
	}
	else {
		while (cs) {
			if (cc_sendcard_cli(cs, cli, 0)) nbcard++;
			cs = cs->next;
		}
	}

	debugf(" FreeCCcam: %d cards --> client(%s)\n",  nbcard, ip2string(cli->ip) );
}

///////////////////////////////////////////////////////////////////////////////

void *freecc_connect_cli(struct struct_clicon *param)
{
	uint8 buf[CC_MAXMSGSIZE];
	uint8 data[16];
	int i;
	struct cc_crypt_block sendblock;	// crypto state block
	struct cc_crypt_block recvblock;	// crypto state block
	char usr[64];
	char pwd[255];

	int sock = param->sock;
	uint32 ip = param->ip;
	free(param);

	memset(usr, 0, sizeof(usr));
	memset(pwd, 0, sizeof(pwd));
	// create & send random seed
	for(i=0; i<12; i++ ) data[i]=fast_rnd();
	// Create NewBox ID
	data[3] = (data[0]^'N') + data[1] + data[2];
	data[7] = data[4] + (data[5]^'B') + data[6];
	data[11] = data[8] + data[9] + (data[10]^'x');
	//Create checksum for "O" cccam:
	for (i = 0; i < 4; i++) {
		data[12 + i] = (data[i] + data[4 + i] + data[8 + i]) & 0xff;
	}
	send_nonb(sock, data, 16, 100);
	//XOR init bytes with 'CCcam'
	cc_crypt_xor(data);
	//SHA1
	SHA_CTX ctx;
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, data, 16);
	SHA1_Final(buf, &ctx);
	//initialisate crypto states
	cc_crypt_init(&sendblock, buf, 20);
	cc_decrypt(&sendblock, data, 16);
	cc_crypt_init(&recvblock, data, 16);
	cc_decrypt(&recvblock, buf, 20);
	//debugdump(buf, 20, "SHA1 hash:");
	memcpy(usr, buf, 20);
	if ((i=recv_nonb(sock, buf, 20, 3000)) == 20) {
		cc_decrypt(&recvblock, buf, 20);
		//debugdump(buf, 20, "Recv SHA1 hash:");
		if ( memcmp(buf, usr, 20)!=0 ) {
			//debugf(" cc_connect_cli(): wrong sha1 hash from client! (%s)\n",ip2string(ip));
			close(sock);
			return NULL;
		}
	} else {
		//debugf(" cc_connect_cli(): recv sha1 timeout\n");
		close(sock);
		return NULL;
	}

  // receive username
	if ((i=recv_nonb(sock, buf, 20, 3000)) == 20) {
		cc_decrypt(&recvblock, buf, i);
		memcpy(usr, buf, 20);
		//debugf(" cc_connect_cli(): username '%s'\n", usr);
	}
	else {
		//debugf(" cc_connect_cli(): recv user timeout\n");
		close(sock);
		return NULL;
	}


  // Check for username
	pthread_mutex_lock(&prg.lockfreecccli);
	int found = 0;
	struct cc_client_data *cli = cfg.freecccam.client;
	while (cli) {
		if (!strcmp(cfg.freecccam.user,usr)) {
			if (cli->handle<=0) {
				found = 1;
				break;
			}
			else {
				if (cli->ip == ip) { // dont connect
					cc_disconnect_cli(cli);
					found = 1;
					break;
				}
			}
		}
		cli = cli->next;
	}
	if (!found)
	while (cli) {
		if (!strcmp(cfg.freecccam.user,usr)) {
			if (cli->handle>0) {
				// Check if we can disconnect idle state clients
				if  (GetTickCount()-cli->lastecmtime > 100000) cc_disconnect_cli(cli);
			}
			if (cli->handle<=0) {
				found = 1;
				break;
			}
		}
		cli = cli->next;
	}
	pthread_mutex_unlock(&prg.lockfreecccli);

	if (!found) {
		debugf(" FreeCCcam: Failed to connect new client(%s)\n",ip2string(ip));
		close(sock);
		return NULL;
	}

  // receive passwd / 'CCcam'
	strcpy( pwd, cfg.freecccam.pass);
	cc_decrypt(&recvblock, (uint8*)pwd, strlen(pwd));
	if ((i=recv_nonb(sock, buf, 6, 3000)) == 6) {
		cc_decrypt(&recvblock, buf, 6);
		if (memcmp( buf+1, "Ccam\0",5)) {
			debugf(" FreeCCcam: login failed from client(%s)\n",ip2string(ip));
			close(sock);
			return NULL;
		}
	} 
	else {
		close(sock);
		return NULL;
	}

  // send passwd ack
	memset(buf, 0, 20);
	memcpy(buf, "CCcam\0", 6);
	//debugf("Server: send ack '%s'\n",buf);
	cc_encrypt(&sendblock, buf, 20);
	send_nonb(sock, buf, 20, 100);

	sprintf(cli->user,"%s", ip2string(ip));
	//cli->ecmnb=0;
	//cli->ecmok=0;
	memcpy(&cli->sendblock,&sendblock,sizeof(sendblock));
	memcpy(&cli->recvblock,&recvblock,sizeof(recvblock));
	debugf(" FreeCCcam: client(%s) connected\n",ip2string(ip));

  // recv cli data
	memset(buf, 0, sizeof(buf));
	i = cc_msg_recv( sock, &cli->recvblock, buf, 3000);
	if (i!=97) {
		debug("error recv cli data\n");
		close(sock);
		return NULL;
	}

  // Setup Client Data
//	pthread_mutex_lock(&prg.lockfreecccli);
	memcpy( cli->nodeid, buf+24, 8);
	memcpy( cli->version, buf+33, 32);
	memcpy( cli->build, buf+65, 32 );
	debugf(" FreeCCcam: client(%s) running version %s build %s\n",ip2string(ip), cli->version, cli->build);  // cli->nodeid,8,
	cli->cardsent = 0;
	cli->connected = GetTickCount();
	cli->lastecmtime = GetTickCount();
	cli->handle = sock;
	cli->ip = ip;
	cli->chkrecvtime = 0;
//	pthread_mutex_unlock(&prg.lockfreecccli);

  // send cli data ack
	cc_msg_send( sock, &cli->sendblock, CC_MSG_CLI_INFO, 0, NULL);
	//cc_msg_send( sock, &cli->sendblock, CC_MSG_BAD_ECM, 0, NULL);
	int sendversion = ( (cli->version[28]=='W')&&(cli->version[29]='H')&&(cli->version[30]='O') );
	cc_sendinfo_cli(cli, sendversion);
	//cc_msg_send( sock, &cli->sendblock, CC_MSG_BAD_ECM, 0, NULL);
	cli->cardsent = 1;
	usleep(10000);
	frcc_sendcards_cli(cli);
	pipe_wakeup( srvsocks[1] );
	return cli;
}


void *freecc_connect_cli_thread(void *param)
{
	SOCKET client_sock =-1;
	struct sockaddr_in client_addr;
	socklen_t socklen = sizeof(client_addr);

	pthread_t srv_tid;

	while(1) {
		pthread_mutex_lock(&prg.locksrvfreecc);
		if (cfg.freecccam.handle>0) {
			struct pollfd pfd;
			pfd.fd = cfg.freecccam.handle;
			pfd.events = POLLIN | POLLPRI;
			int retval = poll(&pfd, 1, 3000);
			if ( retval>0 ) {
				client_sock = accept( cfg.freecccam.handle, (struct sockaddr*)&client_addr, /*(socklen_t*)*/&socklen);
				if ( client_sock<0 ) {
					debugf(" FreeCCcam: Accept failed (errno=%d)\n",errno);
					usleep(30000);
				}
				else {
					//debugf(" FreeCCcam: new connection...\n");
					struct struct_clicon *clicondata = malloc( sizeof(struct struct_clicon) );
					SetSocketKeepalive(client_sock); 
					clicondata->sock = client_sock; 
					clicondata->ip = client_addr.sin_addr.s_addr;
					create_prio_thread(&srv_tid, (threadfn)freecc_connect_cli, clicondata, 50);
				}
			}
			else if (retval<0) usleep(30000);
		} else usleep(100000);
		pthread_mutex_unlock(&prg.locksrvfreecc);
		usleep(3000);
	}// While
}



// Receive messages from clients
void freecc_cli_recvmsg(struct cc_client_data *cli)
{
	unsigned char buf[CC_MAXMSGSIZE];
	unsigned char data[CC_MAXMSGSIZE]; // for other use
	unsigned int cardid;
	int len;

	if (cli->handle>0) {
		len = cc_msg_chkrecv(cli->handle,&cli->recvblock);
		if (len==0) {
			debugf(" FreeCCcam: client(%s) read failed %d\n", cli->user,len);
			cc_disconnect_cli(cli);
		}
		else if (len==-1) {
			if (!cli->chkrecvtime) cli->chkrecvtime = GetTickCount();
			else if ( (cli->chkrecvtime+500)<GetTickCount() ) {
				debugf(" FreeCCcam: client(%s) read failed %d\n", cli->user,len);
				cc_disconnect_cli(cli);
			}
		}
		else if (len>0) {
			cli->chkrecvtime = 0;
			len = cc_msg_recv( cli->handle, &cli->recvblock, buf, 3);
			if (len==0) {
				debugf(" FreeCCcam: client(%s) read failed %d\n", cli->user,len);
				cc_disconnect_cli(cli);
			}
			else if (len<0) {
				debugf(" FreeCCcam: client(%s) read failed %d(%d)\n", cli->user,len,errno);
				cc_disconnect_cli(cli);
			}
			else if (len>0) {
				switch (buf[1]) {
					 case CC_MSG_ECM_REQUEST:
						// Check for length
						cli->lastecmtime = GetTickCount();
	
						if (cli->ecm.busy) {
							// send decode failed
							debugf(" <|> decode failed to CCcam client(%s), too many ecm requests\n", cli->user);
							break;
						}
						//Check for card availability
						cardid = buf[10]<<24 | buf[11]<<16 | buf[12]<<8 | buf[13];
						uint16 caid = buf[4]<<8 | buf[5];
						uint32 provid = buf[6]<<24 | buf[7]<<16 | buf[8]<<8 | buf[9];
						uint16 sid = buf[14]<<8 | buf[15];
						// Check for Profile
						struct cardserver_data *cs=getcsbyid( cardid );
						if (!cs) {
							cli->ecmdenied++;
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client(%s), card-id %x not found\n", cli->user, cardid);
							break;
						}
						// Check for sid
						if (!sid) {
							cs->ecmdenied++;
							cli->ecmdenied++;
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client(%s), Undefined SID\n", cli->user);
							break;
						}
						// Check for caid, accept caid=0
						if ( !accept_caid(cs,caid) ) {
							cli->ecmdenied++;
							cs->ecmdenied++;
							// send decode failed
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client(%s) ch %04x:%06x:%04x Wrong CAID\n", cli->user, caid, provid, sid);
							break;
						}
						// Check for provid, accept provid==0
						if ( !accept_prov(cs,provid) ) {
							cli->ecmdenied++;
							cs->ecmdenied++;
							// send decode failed
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client(%s) ch %04x:%06x:%04x Wrong PROVIDER\n", cli->user, caid, provid, sid);
							break;
						}
	
						// Check for Accepted sids
						if ( !accept_sid(cs,sid) ) {
							cli->ecmdenied++;
							cs->ecmdenied++;
							// send decode failed
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client(%s) ch %04x:%06x:%04x SID not accepted\n", cli->user, caid, provid, sid);
							break;
						}
	
						// Check ECM length
						if ( !accept_ecmlen(len) ) {
							cli->ecmdenied++;
							cs->ecmdenied++;
							buf[1] = 0; buf[2] = 0;
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client(%s) ch %04x:%06x:%04x, ecm length error(%d)\n", cli->user, caid, provid, sid, len);
							break;
						}
	
						// ACCEPTED
						memcpy( &data[0], &buf[17], len-17);

						pthread_mutex_lock(&prg.lockecm);
	
						// Search for ECM
						int ecmid = search_ecmdata_dcw( data,  len-17, sid); // dont get failed ecm request from cache
						if ( ecmid!=-1 ) {
							ECM_DATA *ecm=getecmbyid(ecmid);
							ecm->lastrecvtime = GetTickCount();
							//TODO: Add another card for sending ecm
							cc_store_ecmclient(ecmid, cli);
							debugf(" <- ecm from CCcam client(%s) ch %04x:%06x:%04x*\n", cli->user, caid, provid, sid);
							cli->ecm.busy=1;
							cli->ecm.hash = ecm->hash;
							cli->ecm.cardid = cs->id;
						}
						else {
							cs->ecmaccepted++;
							// Setup ECM Request for Server(s)
							ecmid = store_ecmdata(cs->id, &data[0], len-17, sid, caid, provid);
							ECM_DATA *ecm=getecmbyid(ecmid);
							cc_store_ecmclient(ecmid, cli);
							debugf(" <- ecm from CCcam client(%s) ch %04x:%06x:%04x (>%dms)\n", cli->user, caid, provid, sid, cli->ecm.dcwtime);
							cli->ecm.busy=1;
							cli->ecm.hash = ecm->hash;
							cli->ecm.cardid = cs->id;
							if (cs->usecache && cfg.cachepeer) {
								pipe_send_cache_find(ecm, cs);
								ecm->waitcache = 1;
								ecm->dcwstatus = STAT_DCW_WAITCACHE;
							} else ecm->dcwstatus = STAT_DCW_WAIT;
						}
						ecm_check_time = frcc_dcw_check_time = 0;

						pthread_mutex_unlock(&prg.lockecm);
						break;
		
					 case CC_MSG_KEEPALIVE:
						//printf(" Keepalive from client '%s'\n",cli->user);
						cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_KEEPALIVE, 0, NULL);
						break;
					 case CC_MSG_BAD_ECM:
						//debugf(" CCcam: cmd 0x05 ACK from client '%s'\n",cli->user);
						//if (cli->cardsent==0) {
						//	cc_sendcards_cli(cli);
						//	cli->cardsent=1;
						//}
	
						break;
					 //default: debugf(" FreeCCcam: Unknown Packet ID : %02x from client(%s)\n", buf[1], cli->user);
				}
			}
		}
	}
}


// Check sending cw to clients
uint32 freecc_check_sendcw()
{
	struct cc_client_data *cli = cfg.freecccam.client;
	uint ticks = GetTickCount();
	uint restime = ticks + 10000;
	uint clitime = restime;
	while (cli) {
			if ( (cli->handle!=INVALID_SOCKET)&&(cli->ecm.busy) ) { //&&(cli->ecm.status==STAT_ECM_SENT) ) {
				pthread_mutex_lock(&prg.lockecm); //###
				// Check for DCW ANSWER
				ECM_DATA *ecm = getecmbyid(cli->ecm.id);
				if (ecm->hash!=cli->ecm.hash) cc_senddcw_cli( cli );
				// Check for FAILED
				if (ecm->dcwstatus==STAT_DCW_FAILED) {
					static char msg[] = "decode failed";
					cli->ecm.statmsg=msg;
					cc_senddcw_cli( cli );
				}
				// Check for SUCCESS
				else if (ecm->dcwstatus==STAT_DCW_SUCCESS) {
					// check for client allowed cw time
					if ( (cli->ecm.recvtime+cli->ecm.dcwtime)<=ticks ) {
						static char msg[] = "good dcw";
						cli->ecm.statmsg = msg;
						cc_senddcw_cli( cli );
					} else clitime = cli->ecm.recvtime+cli->ecm.dcwtime;
				}
				// check for timeout / no server again
				else if (ecm->dcwstatus==STAT_DCW_WAIT){
					uint32 timeout;
					struct cardserver_data *cs = getcsbyid( ecm->csid);
					if (cs) timeout = cs->dcwtimeout; else timeout = 5700;
					if ( (cli->ecm.recvtime+timeout) < ticks ) {
						static char msg[] = "dcw timeout";
						cli->ecm.statmsg=msg;
						cc_senddcw_cli( cli );
					} else clitime = cli->ecm.recvtime+timeout;
				}
				if (restime>clitime) restime = clitime;
				pthread_mutex_unlock(&prg.lockecm); //###
			}
			cli = cli->next;
		}
	return (restime+1);
}

