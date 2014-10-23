////
// File: srv-cccam.c
/////


void *cc_connect_cli_thread(void *param);
void cc_cli_recvmsg(struct cc_client_data *cli);


struct cc_client_data *getcccamclientbyid(uint32 id)
{
	struct cc_client_data *cli = cfg.cccam.client;
	while (cli) {
		if (cli->id==id) return cli;
		cli = cli->next;
	}
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// CCCAM SERVER: DISCONNECT CLIENTS
///////////////////////////////////////////////////////////////////////////////

void cc_disconnect_cli(struct cc_client_data *cli)
{
	if (cli->handle>0) {
		debugf(" CCcam: client '%s' disconnected \n", cli->user);
		close(cli->handle);
		cli->handle = INVALID_SOCKET;
		cli->uptime += GetTickCount()-cli->connected;
	}
}


///////////////////////////////////////////////////////////////////////////////
// CCCAM SERVER: CONNECT CLIENTS
///////////////////////////////////////////////////////////////////////////////

unsigned int seed;
uint8 fast_rnd()
{
  unsigned int offset = 12923;
  unsigned int multiplier = 4079;

  seed = seed * multiplier + offset;
  return (uint8)(seed % 0xFF);
}

///////////////////////////////////////////////////////////////////////////////

int cc_sendinfo_cli(struct cc_client_data *cli, int sendversion)
{
	uint8 buf[CC_MAXMSGSIZE];
	memset(buf, 0, CC_MAXMSGSIZE);
	memcpy(buf, cfg.cccam.nodeid, 8 );
	memcpy(buf + 8, cfg.cccam.version, 32);		// cccam version (ascii)
	memcpy(buf + 40, cfg.cccam.build, 32);       // build number (ascii)
	if (sendversion) {
		buf[38] = REVISION >> 8;
		buf[37] = REVISION & 0xff;
		buf[36] = 0;
		buf[35] = 'x';
		buf[34] = 'B';
		buf[33] = 'N';
	}
	//debugdump(cfg.cccam.nodeid,8,"Sending server data version: %s, build: %s nodeid ", cfg.cccam.version, cfg.cccam.build);
	return cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_SRV_INFO, 0x48, buf);
}

///////////////////////////////////////////////////////////////////////////////

int cc_sendcard_cli(struct cardserver_data *cs, struct cc_client_data *cli, int uphops)
{
	int j;
	uint8 buf[CC_MAXMSGSIZE];

	memset(buf, 0, sizeof(buf));
	buf[0] = cs->id >> 24;
	buf[1] = cs->id >> 16;
	buf[2] = cs->id >> 8;
	buf[3] = cs->id & 0xff;
	buf[4] = cs->id >> 24;
	buf[5] = cs->id >> 16;
	buf[6] = cs->id >> 8;
	buf[7] = cs->id & 0xff;
	buf[8] = cs->card.caid >> 8;
	buf[9] = cs->card.caid & 0xff;
	buf[10] = uphops;
	buf[11] = cli->dnhops; // Dnhops
	buf[20] = cs->card.nbprov;
	for (j=0; j<cs->card.nbprov; j++) {
		//memcpy(buf + 21 + (j*7), card->provs[j], 7);
		buf[21+j*7] = 0xff&(cs->card.prov[j]>>16);
		buf[22+j*7] = 0xff&(cs->card.prov[j]>>8);
		buf[23+j*7] = 0xff&(cs->card.prov[j]);
/*
		buf[24+j*7] = 0xff&(cs->card.prov[j].ua>>24);
		buf[25+j*7] = 0xff&(cs->card.prov[j].ua>>16);
		buf[26+j*7] = 0xff&(cs->card.prov[j].ua>>8);
		buf[27+j*7] = 0xff&(cs->card.prov[j].ua);
*/
	}
	buf[21 + (cs->card.nbprov*7)] = 1;
	memcpy(buf + 22 + (j*7), cfg.cccam.nodeid, 8);
	cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_CARD_ADD, 30 + (j*7), buf);
	return 1;
}

///////////////////////////////////////////////////////////////////////////////

void cc_sendcards_cli(struct cc_client_data *cli)
{
	int nbcard=0;
	struct cardserver_data *cs = cfg.cardserver;

	int i;
	if (cli->csport[0]) {
		for(i=0;i<MAX_CSPORTS;i++) {
			if(cli->csport[i]) {
				cs = getcsbyport(cli->csport[i]);
				if (cs)
					if (cc_sendcard_cli(cs, cli, 0)) nbcard++;
			} else break;
		}
	}
	else if (cfg.cccam.csport[0]) {
		for(i=0;i<MAX_CSPORTS;i++) {
			if(cfg.cccam.csport[i]) {
				cs = getcsbyport(cfg.cccam.csport[i]);
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

	debugf(" CCcam: %d cards --> client(%s)\n",  nbcard, cli->user);
}

struct struct_clicon {
	int sock;
	uint32 ip;
};

///////////////////////////////////////////////////////////////////////////////

void *cc_connect_cli(struct struct_clicon *param)
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
	//init crypto states
	cc_crypt_init(&sendblock, buf, 20);
	cc_decrypt(&sendblock, data, 16);
	cc_crypt_init(&recvblock, data, 16);
	cc_decrypt(&recvblock, buf, 20);
	//debugdump(buf, 20, "SHA1 hash:");
	memcpy(usr, buf, 20);
	if ((i=recv_nonb(sock, buf, 20, 3000)) == 20) {
		if (flag_debugnet) {
			debugf(" CCcam: receive SHA1 hash %d\n",i);
			debughex(buf,i);
		}
		cc_decrypt(&recvblock, buf, 20);
		//debugf(" Decrypted SHA1 hash (20):\n"); debughex(buf,20);
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
	i = recv_nonb(sock, buf, 20, 3000);

	if (flag_debugnet) {
		debugf(" CCcam: receive username %d\n",i);
		debughex(buf,i);
	}

	if (i == 20) {
		cc_decrypt(&recvblock, buf, i);
		memcpy(usr, buf, 20);
		//strcpy(usr, (char*)buf);
		//debugf(" cc_connect_cli(): username '%s'\n", usr);
	}
	else {
		//debugf(" cc_connect_cli(): recv user timeout\n");
		close(sock);
		return NULL;
	}
	// Check for username
	pthread_mutex_lock(&prg.lockcccli);
	int found = 0;
	struct cc_client_data *cli = cfg.cccam.client;
	while (cli) {
		if (!strcmp(cli->user, usr)) {
			if (cli->disabled) { // Connect only enabled clients
				pthread_mutex_unlock(&prg.lockcccli);
				debugf(" CCcam: connection refused for client '%s' (%s), client disabled\n", usr, ip2string(ip));
				close(sock);
				return NULL;
			}
			found = 1;
			break;
		}
		cli = cli->next;
	}
	pthread_mutex_unlock(&prg.lockcccli);

	if (!found) {
		debugf(" CCcam: Unknown Client '%s' (%s)\n", usr, ip2string(ip));
		close(sock);
		return NULL;
	}

	// Check for Host
	if (cli->host) {
		struct host_data *host = cli->host;
		host->clip = ip;
		if ( host->ip && (host->ip!=ip) ) {
			uint sec = getseconds()+60;
			if ( host->checkiptime > sec ) host->checkiptime = sec;
			debugf(" CCcam: Aborted connection from Client '%s' (%s), ip refused\n", usr, ip2string(ip));
			close(sock);
			return NULL;
		}
	}

	// Receive & Check passwd / 'CCcam'
	strcpy( pwd, cli->pass);
	cc_decrypt(&recvblock, (uint8*)pwd, strlen(pwd));
	if ((i=recv_nonb(sock, buf, 6, 3000)) == 6) {
		cc_decrypt(&recvblock, buf, 6);
		if (memcmp( buf+1, "Ccam\0", 5)) {
			debugf(" CCcam: login failed from client '%s'\n", usr);
			close(sock);
			return NULL;
		}
	} 
	else {
		close(sock);
		return NULL;
	}

	// Disconnect old connection if isthere any
	if (cli->handle>0) {
		if (cli->ip==ip) {
			debugf(" CCcam: Client '%s' (%s) already connected\n", usr, ip2string(ip));
			cc_disconnect_cli(cli);
		}
		else {
			debugf(" CCcam: Client '%s' (%s) already connected with different ip \n", usr, ip2string(ip));
			cc_disconnect_cli(cli);
		}
	}

	// Send passwd ack
	memset(buf, 0, 20);
	memcpy(buf, "CCcam\0", 6);
	//debugf("Server: send ack '%s'\n",buf);
	cc_encrypt(&sendblock, buf, 20);
	send_nonb(sock, buf, 20, 100);


	//cli->ecmnb=0;
	//cli->ecmok=0;
	memcpy(&cli->sendblock,&sendblock,sizeof(sendblock));
	memcpy(&cli->recvblock,&recvblock,sizeof(recvblock));
	debugf(" CCcam: client '%s' connected\n", usr);

	// Recv cli data
	memset(buf, 0, sizeof(buf));
	i = cc_msg_recv( sock, &cli->recvblock, buf, 3000);
	if ( i<65 ) {
		debugf(" CCcam: Error recv cli data (%d)\n",i);
		debughex(buf,i);
		close(sock);
		return NULL;
	}
	// Setup Client Data
	// pthread_mutex_lock(&prg.lockcccli);
	memcpy( cli->nodeid, buf+24, 8);
	memcpy( cli->version, buf+33, 31);
	memcpy( cli->build, buf+65, 31 );
	debugf(" CCcam: client '%s' running version %s build %s\n", usr, cli->version, cli->build);  // cli->nodeid,8,
	cli->cardsent = 0;
	cli->connected = GetTickCount();
	cli->ip = ip;
	cli->chkrecvtime = 0;
	cli->handle = sock;

//	pthread_mutex_unlock(&prg.lockcccli);

	// send cli data ack
	cc_msg_send( sock, &cli->sendblock, CC_MSG_CLI_INFO, 0, NULL);
	//cc_msg_send( sock, &cli->sendblock, CC_MSG_BAD_ECM, 0, NULL);
	int sendversion = ( (cli->version[28]=='W')&&(cli->version[29]='H')&&(cli->version[30]='O') );
	cc_sendinfo_cli(cli, sendversion);
	//cc_msg_send( sock, &cli->sendblock, CC_MSG_BAD_ECM, 0, NULL);
	cli->cardsent = 1;
	//TODO: read from client packet CC_MSG_BAD_ECM
	//len = cc_msg_recv(cli->handle, &cli->recvblock, buf, 3);
	usleep(10000);
	cc_sendcards_cli(cli);
	cli->handle = sock;
	pipe_wakeup( srvsocks[1] );
	return cli;
}

////////////////////////////////////////////////////////////////////////////////

void *cc_connect_cli_thread(void *param)
{
	SOCKET client_sock =-1;
	struct sockaddr_in client_addr;
	socklen_t socklen = sizeof(client_addr);

	pthread_t srv_tid;

	while(1) {
		pthread_mutex_lock(&prg.locksrvcc);
		if (cfg.cccam.handle>0) {
			struct pollfd pfd;
			pfd.fd = cfg.cccam.handle;
			pfd.events = POLLIN | POLLPRI;
			int retval = poll(&pfd, 1, 3000);
			if ( retval>0 ) {
				client_sock = accept( cfg.cccam.handle, (struct sockaddr*)&client_addr, /*(socklen_t*)*/&socklen);
				if ( client_sock<0 ) {
					debugf(" CCcam: Accept failed (errno=%d)\n",errno);
					usleep(30000);
				}
				else {
					SetSocketKeepalive(client_sock); 
					//debugf(" CCcam: new connection...\n");
					struct struct_clicon *clicondata = malloc( sizeof(struct struct_clicon) );
					clicondata->sock = client_sock; 
					clicondata->ip = client_addr.sin_addr.s_addr;
					create_prio_thread(&srv_tid, (threadfn)cc_connect_cli, clicondata, 50);
				}
			}
			else if (retval<0) usleep(30000);
		} else usleep(100000);
		pthread_mutex_unlock(&prg.locksrvcc);
		usleep(3000);
	}// While
}



///////////////////////////////////////////////////////////////////////////////
// CCCAM SERVER: RECEIVE MESSAGES FROM CLIENTS
///////////////////////////////////////////////////////////////////////////////

void cc_store_ecmclient(int ecmid, struct cc_client_data *cli)
{
	uint32 ticks = GetTickCount();

	cli->ecm.dcwtime = cli->dcwtime;
	cli->ecm.recvtime = ticks;
	cli->ecm.id = ecmid;
    cli->ecm.status = STAT_ECM_SENT;
}

///////////////////////////////////////////////////////////////////////////////

// Receive messages from client
void cc_cli_recvmsg(struct cc_client_data *cli)
{     
	unsigned char buf[CC_MAXMSGSIZE];
	unsigned char data[CC_MAXMSGSIZE]; // for other use
	unsigned int cardid;
	int len;

	if (cli->handle>0) {
		len = cc_msg_chkrecv(cli->handle,&cli->recvblock);
		if (len==0) {
			debugf(" CCcam: client '%s' read failed %d\n", cli->user, len);
			cc_disconnect_cli(cli);
		}
		else if (len==-1) {
			if (!cli->chkrecvtime) cli->chkrecvtime = GetTickCount();
			else if ( (cli->chkrecvtime+500)<GetTickCount() ) {
				debugf(" CCcam: client '%s' read failed %d\n", cli->user, len);
				cc_disconnect_cli(cli);
			}
		}
		else if (len>0) {
			cli->chkrecvtime = 0;
			len = cc_msg_recv( cli->handle, &cli->recvblock, buf, 3);
			if (len==0) {
				debugf(" CCcam: client '%s' read failed %d\n", cli->user, len);
				cc_disconnect_cli(cli);
			}
			else if (len<0) {
				debugf(" CCcam: client '%s' read failed %d(%d)\n", cli->user, len, errno);
				cc_disconnect_cli(cli);
			}
			else if (len>0) {
				switch (buf[1]) {
					 case CC_MSG_ECM_REQUEST:
						cli->ecmnb++;
						cli->lastecmtime = GetTickCount();
	
						if (cli->ecm.busy) {
							// send decode failed
							cli->ecmdenied++;
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client '%s', too many ecm requests\n", cli->user);
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
							// check for cs by caid:prov
							cs = getcsbycaidprov(caid, provid);
							if (!cs) {
								cli->ecmdenied++;
								cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
								debugf(" <|> decode failed to CCcam client '%s', card-id %x (%04x:%06x) not found\n",cli->user, cardid, caid, provid);
								break;
							}
						}
						// Check for sid
						if (!sid) {
							cs->ecmdenied++;
							cli->ecmdenied++;
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client '%s', Undefined SID\n", cli->user);
							break;
						}
						// Check for caid, accept caid=0
						if ( !accept_caid(cs,caid) ) {
							cs->ecmdenied++;
							cli->ecmdenied++;
							// send decode failed
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client '%s' ch %04x:%06x:%04x Wrong CAID\n", cli->user, caid, provid, sid);
							break;
						}

						// Check for provid, accept provid==0
						if ( !accept_prov(cs,provid) ) {
							cli->ecmdenied++;
							cs->ecmdenied++;
							// send decode failed
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client '%s' ch %04x:%06x:%04x Wrong PROVIDER\n", cli->user, caid, provid, sid);
							break;
						}
	
						// Check for Accepted sids
						if ( !accept_sid(cs,sid) ) {
							cli->ecmdenied++;
							cs->ecmdenied++;
							// send decode failed
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client '%s' ch %04x:%06x:%04x SID not accepted\n", cli->user, caid, provid, sid);
							break;
						}

						// Check ECM length
						if ( !accept_ecmlen(len) ) {
							cli->ecmdenied++;
							cs->ecmdenied++;
							buf[1] = 0; buf[2] = 0;
							cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK2, 0, NULL);
							debugf(" <|> decode failed to CCcam client '%s' ch %04x:%06x:%04x, ecm length error(%d)\n", cli->user, caid, provid, sid, len);
							break;
						}
	
						// ACCEPTED
						//cs->ecmaccepted++;
						//cli->ecmaccepted++;

						// XXX: check ecm tag = 0x80,0x81
						memcpy( &data[0], &buf[17], len-17);

						pthread_mutex_lock(&prg.lockecm);
	
						// Search for ECM
						int ecmid = search_ecmdata_dcw( data, len-17, sid); // dont get failed ecm request from cache
						if ( ecmid!=-1 ) {
							ECM_DATA *ecm=getecmbyid(ecmid);
							ecm->lastrecvtime = GetTickCount();
							//TODO: Add another card for sending ecm
							cc_store_ecmclient(ecmid, cli);
							debugf(" <- ecm from CCcam client '%s' ch %04x:%06x:%04x*\n", cli->user, caid, provid, sid);
							cli->ecm.busy=1;
							cli->ecm.hash = ecm->hash;
							cli->ecm.cardid = cardid;
						}
						else {
							cs->ecmaccepted++;
							// Setup ECM Request for Server(s)
							ecmid = store_ecmdata(cs->id, &data[0], len-17, sid, caid, provid);
							ECM_DATA *ecm=getecmbyid(ecmid);
							cc_store_ecmclient(ecmid, cli);
							debugf(" <- ecm from CCcam client '%s' ch %04x:%06x:%04x (>%dms)\n", cli->user, caid, provid, sid, cli->ecm.dcwtime);
							cli->ecm.busy=1;
							cli->ecm.hash = ecm->hash;
							cli->ecm.cardid = cardid;
							if (cs->usecache && cfg.cachepeer) {
								pipe_send_cache_find(ecm, cs);
								ecm->waitcache = 1;
								ecm->dcwstatus = STAT_DCW_WAITCACHE;
							} else ecm->dcwstatus = STAT_DCW_WAIT;
						}
						ecm_check_time = cc_dcw_check_time = 0;

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
					 //default: debugf(" CCcam: Unknown Packet ID : %02x from client '%s'\n", buf[1],cli->user);
				}
			}
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
// CCCAM SERVER: SEND DCW TO CLIENTS
////////////////////////////////////////////////////////////////////////////////

void cc_senddcw_cli(struct cc_client_data *cli)
{
	unsigned char buf[CC_MAXMSGSIZE];

	if (cli->ecm.status==STAT_DCW_SENT) {
		debugf(" +> cw send failed to CCcam client '%s', cw already sent\n", cli->user); 
		return;
	}
	if (cli->handle<=0) {
		debugf(" +> cw send failed to CCcam client '%s', client disconnected\n", cli->user); 
		return;
	}
	if (!cli->ecm.busy) {
		debugf(" +> cw send failed to CCcam client '%s', no ecm request\n", cli->user); 
		return;
	}

	ECM_DATA *ecm = getecmbyid(cli->ecm.id);
	//FREEZE
	int enablefreeze;
	if ( (cli->ecm.laststatus=1)&&(cli->ecm.lastcaid==ecm->caid)&&(cli->ecm.lastprov==ecm->provid)&&(cli->ecm.lastsid==ecm->sid)&&(cli->lastdcwtime+200<GetTickCount()) )
		enablefreeze = 1; else enablefreeze = 0;
	//
	cli->ecm.lastcaid = ecm->caid;
	cli->ecm.lastprov = ecm->provid;
	cli->ecm.lastsid = ecm->sid;
	cli->ecm.lastdecodetime = GetTickCount()-cli->ecm.recvtime;
	cli->ecm.lastid = cli->ecm.id;

	if ( (ecm->dcwstatus==STAT_DCW_SUCCESS)&&(ecm->hash==cli->ecm.hash) ) {
		cli->ecm.lastdcwsrctype = ecm->dcwsrctype;
		cli->ecm.lastdcwsrcid = ecm->dcwsrcid;
		cli->ecm.laststatus=1;
		cli->ecmok++;
		cli->lastdcwtime = GetTickCount();
		cli->ecmoktime += GetTickCount()-cli->ecm.recvtime;
		//cli->lastecmoktime = GetTickCount()-cli->ecm.recvtime;
		memcpy( buf, ecm->cw, 16 );
		cc_crypt_cw( cli->nodeid, cli->ecm.cardid , buf);
		cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_REQUEST, 16, buf);
		cc_encrypt(&cli->sendblock, buf, 16); // additional crypto step

		debugf(" => cw to CCcam client '%s' ch %04x:%06x:%04x (%dms)\n", cli->user, ecm->caid, ecm->provid, ecm->sid, GetTickCount()-cli->ecm.recvtime);
	}
	else { //if (ecm->data->dcwstatus==STAT_DCW_FAILED)
		if (enablefreeze) cli->freeze++;
		cli->ecm.lastdcwsrctype = DCW_SOURCE_NONE;
		cli->ecm.lastdcwsrcid = 0;
		cli->ecm.laststatus=0;
		cc_msg_send( cli->handle, &cli->sendblock, CC_MSG_ECM_NOK1, 0, NULL);
		debugf(" |> decode failed to CCcam client '%s' ch %04x:%06x:%04x (%dms)\n", cli->user, ecm->caid,ecm->provid, ecm->sid, GetTickCount()-cli->ecm.recvtime);
	}
	cli->ecm.busy=0;
	cli->ecm.status = STAT_DCW_SENT;
}

///////////////////////////////////////////////////////////////////////////////

// Check sending cw to clients
uint32 cc_check_sendcw()
{
	struct cc_client_data *cli = cfg.cccam.client;
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


///////////////////////////////////////////////////////////////////////////////
// CCCAM SERVER: START/STOP
///////////////////////////////////////////////////////////////////////////////

pthread_t cc_cli_tid;
int start_thread_cccam()
{
	create_prio_thread(&cc_cli_tid, cc_connect_cli_thread, NULL, 50); // Lock server
	return 0;
}

void done_cccamsrv()
{
  if (close(cfg.cccam.handle)) debugf("Close failed(%d)", cfg.cccam.handle);
}

