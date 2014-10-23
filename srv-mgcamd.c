////
// File: srv-mgcamd.c
/////


void *mg_connect_cli_thread(void *param);
uint mg_check_sendcw();


struct mg_client_data *getmgcamdclientbyid(uint32 id)
{
	struct mg_client_data *cli = cfg.mgcamd.client;
	while (cli) {
		if (cli->id==id) return cli;
		cli = cli->next;
	}
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// DISCONNECT
///////////////////////////////////////////////////////////////////////////////

void mg_disconnect_cli(struct mg_client_data *cli)
{
	if (cli->handle!=INVALID_SOCKET) {
		debugf(" mgcamd: client '%s' disconnected\n", cli->user);
		close(cli->handle);
		cli->handle = INVALID_SOCKET;
		cli->uptime += GetTickCount()-cli->connected;
		cli->connected = 0;
	}
}


///////////////////////////////////////////////////////////////////////////////
// CONECCT
///////////////////////////////////////////////////////////////////////////////


struct mg_clicon {
	int sock;
	uint32 ip;
};


void *th_mg_connect_cli(struct mg_clicon *param)
{
    char passwdcrypt[120];
	unsigned char keymod[14];
	int i,index;
	unsigned char sessionkey[16];
	struct cs_custom_data clicd;
	unsigned char buf[CWS_NETMSGSIZE];

	int sock = param->sock;
	uint32 ip = param->ip;
	free(param);

	// Create random deskey
	for (i=0; i<14; i++) keymod[i] = 0xff & rand();
	// Create NewBox ID
	keymod[3] = (keymod[0]^'N') + keymod[1] + keymod[2];
	keymod[7] = keymod[4] + (keymod[5]^'B') + keymod[6];
	keymod[11] = keymod[8] + keymod[9] + (keymod[10]^'x');
	// send random des key
	if (send_nonb(sock, keymod, 14, 500)<=0) {
		debugf(" ERR SEND\n");
		close(sock);
		pthread_exit(NULL);
	}

	uint32 ticks = GetTickCount();
	// Calc SessionKey
	des_login_key_get(keymod, cfg.mgcamd.key, 14, sessionkey);

//STAT: LOGIN INFO
	// 3. login info
	i = cs_message_receive(sock, &clicd, buf, sessionkey, 3000);
	if (i<=0) {
		if (i==-2) debugf(" mgcamd: (%s) new connection closed, wrong des key\n", ip2string(ip));
		else debugf(" mgcamd: (%s) new connection closed, receive timeout\n", ip2string(ip));
		close(sock);
		pthread_exit(NULL);
	}

	ticks = GetTickCount()-ticks;
	if (buf[0]!=MSG_CLIENT_2_SERVER_LOGIN) {
		close(sock);
		pthread_exit(NULL);
	}

	// Check username length
	if ( strlen( (char*)buf+3 )>63 ) {
		/*
		buf[0] = MSG_CLIENT_2_SERVER_LOGIN_NAK;
		buf[1] = 0;
		buf[2] = 0;
		cs_message_send(sock, NULL, buf, 3, sessionkey);
		*/
		debugf(" newcamd: wrong username length (%s)\n", ip2string(ip));
		close(sock);
		pthread_exit(NULL);
	}

	pthread_mutex_lock(&prg.lockclimg);
	index = 3;
	struct mg_client_data *usr = cfg.mgcamd.client;
	int found = 0;
	while (usr) {
		if (!strcmp(usr->user,(char*)&buf[index])) {
			if (usr->disabled) { // Connect only enabled clients
				pthread_mutex_unlock(&prg.lockclimg);
				debugf(" mgcamd: connection refused for client '%s' (%s), client disabled\n", usr->user, ip2string(ip));
				close(sock);
				pthread_exit(NULL);
			}
			found=1;
			break;
		}
		usr = usr->next;
	}
	if (!found) {
		pthread_mutex_unlock(&prg.lockclimg);
		buf[0] = MSG_CLIENT_2_SERVER_LOGIN_NAK;
		buf[1] = 0;
		buf[2] = 0;
		//cs_message_send(sock, NULL, buf, 3, sessionkey);
		debugf(" mgcamd: unknown user '%s' (%s)\n", &buf[3], ip2string(ip));
		close(sock);
		pthread_exit(NULL);
	}

	// Check password
	index += strlen(usr->user) +1;
	__md5_crypt(usr->pass, "$1$abcdefgh$",passwdcrypt);
	if (!strcmp(passwdcrypt,(char*)&buf[index])) {
		//Check Reconnection
		if (usr->handle!=INVALID_SOCKET) {
			debugf(" mgcamd: user already connected '%s' (%s)\n",usr->user, ip2string(ip));
			mg_disconnect_cli(usr);
		}
		// Store program id
		usr->progid = clicd.sid;

		buf[0] = MSG_CLIENT_2_SERVER_LOGIN_ACK;
		buf[1] = 0;
		buf[2] = 0;

		//clicd.msgid = 0;
		clicd.sid = 0x6E73;
		clicd.caid = 0;
		clicd.provid = 0x14000000; // mgcamd protocol version?

		cs_message_send(sock, &clicd, buf, 3, sessionkey);
		des_login_key_get( cfg.mgcamd.key, (unsigned char*)passwdcrypt, strlen(passwdcrypt),sessionkey);
		memcpy( &usr->sessionkey, &sessionkey, 16);
		// Setup User data
		usr->handle = sock;
		usr->ip = ip;
		usr->ping = ticks;
		usr->lastecmtime = GetTickCount();
		usr->ecm.busy=0;
		usr->ecm.recvtime=0;
		usr->connected = GetTickCount();
		usr->chkrecvtime = 0;
		debugf(" mgcamd: client '%s' connected (%s)\n", usr->user, ip2string(ip));
		pipe_wakeup( srvsocks[1] );
	}
	else {
		// send NAK
		buf[0] = MSG_CLIENT_2_SERVER_LOGIN_NAK;
		buf[1] = 0;
		buf[2] = 0;
		//cs_message_send(sock, NULL, buf, 3, sessionkey);
		debugf(" mgcamd: client '%s' wrong password (%s)\n", usr->user, ip2string(ip));
		close(sock);
	}
	pthread_mutex_unlock(&prg.lockclimg);
	pthread_exit(NULL);
}


void *mg_connect_cli_thread(void *param)
{
	int clientsock;
	struct sockaddr_in clientaddr;
	socklen_t socklen = sizeof(struct sockaddr);
	// Connect Clients 
	pthread_t srv_tid;

	while (1) {
		pthread_mutex_lock(&prg.locksrvmg);
		if (cfg.mgcamd.handle>0) {
			struct pollfd pfd;
			pfd.fd = cfg.mgcamd.handle;
			pfd.events = POLLIN | POLLPRI;
			int retval = poll(&pfd, 1, 3000);
			if ( retval>0 ) {
				clientsock = accept(cfg.mgcamd.handle, (struct sockaddr*)&clientaddr, &socklen );
				if (clientsock<0) {
					if (errno == EAGAIN || errno == EINTR) continue;
					else {
						debugf(" mgcamd: Accept failed (%d)\n",errno);
					}
				}
				else {			
					debugf(" mgcamd: new client Connection(%d)...%s\n", clientsock, ip2string(clientaddr.sin_addr.s_addr) );
					SetSocketKeepalive(clientsock); 
					struct mg_clicon *clicondata = malloc( sizeof(struct mg_clicon) );
					clicondata->sock = clientsock; 
					clicondata->ip = clientaddr.sin_addr.s_addr;
					//while(EAGAIN==pthread_create(&srv_tid, NULL, th_cs_connect_cli,clicondata)) usleep(1000); pthread_detach(&srv_tid);
					create_prio_thread(&srv_tid, (threadfn)th_mg_connect_cli,clicondata, 50);
				}
			}
			else if (retval<0) usleep(30000);
		} else usleep(100000);
		pthread_mutex_unlock(&prg.locksrvmg);
		usleep(3000);
	}
}



///////////////////////////////////////////////////////////////////////////////
// RECEIVE MESSAGES
///////////////////////////////////////////////////////////////////////////////


void mg_store_ecmclient( int ecmid, struct mg_client_data *cli, int climsgid)
{
	//check for last ecm recv time
	uint32 ticks = GetTickCount();
	cli->ecm.dcwtime = cli->dcwtime;
	cli->ecm.recvtime = ticks;
	cli->ecm.id = ecmid;
	cli->ecm.climsgid = climsgid;
	cli->ecm.status = STAT_ECM_SENT;
}


void mg_cli_recvmsg(struct mg_client_data *cli)
{
	struct cs_custom_data clicd; // Custom data
	struct cardserver_data *cs=NULL; 
	int len;
	unsigned char buf[CWS_NETMSGSIZE];
	unsigned char data[CWS_NETMSGSIZE]; // for other use

	if ( (cli->handle>0)&&(!cli->ecm.busy) ) {
		len = cs_msg_chkrecv(cli->handle);
		if (len==0) {
			debugf(" mgcamd: client '%s' read failed %d\n", cli->user,len);
			mg_disconnect_cli(cli);
		}
		else if (len==-1) {
			if (!cli->chkrecvtime) cli->chkrecvtime = GetTickCount();
			else if ( (cli->chkrecvtime+300)<GetTickCount() ) {
				debugf(" mgcamd: client '%s' read failed %d\n", cli->user,len);
				mg_disconnect_cli(cli);
			}
		}
		else if (len>0) {
			cli->chkrecvtime = 0;
			len = cs_message_receive(cli->handle, &clicd, buf, cli->sessionkey,3);
			if (len==0) {
				debugf(" mgcamd: client '%s' read failed %d\n", cli->user,len);
				mg_disconnect_cli(cli);
			}
			else if (len<0) {
				debugf(" mgcamd: client '%s' read failed %d(%d)\n", cli->user,len,errno);
				mg_disconnect_cli(cli);
			}
			else if (len>0) switch ( buf[0] ) {
				case EXT_GET_VERSION:
					memset( buf, 0, sizeof(buf) );
					buf[0] = EXT_GET_VERSION; buf[1]=0; buf[2]=0;
					buf[3] = 0x31;
					buf[4] = 0x2e;
					buf[5] = 0x36;
					buf[6] = 0x37;// 1.67
					cs_message_send(cli->handle, 0, buf, 7, cli->sessionkey);
					break;
				case MSG_CARD_DATA_REQ:
					memset( buf, 0, sizeof(buf) );
					buf[0] = MSG_CARD_DATA; buf[1]=0; buf[2]=0;
					buf[3] = 2;
					buf[4] = 0;
					buf[5] = 0;
					buf[14] = 1;
					cs_message_send(cli->handle, &clicd, buf, 15+11, cli->sessionkey);

					if (cli->csport[0]) {
						int i;
						for(i=0;i<MAX_CSPORTS;i++) {
							if (!cli->csport[i]) break;
							cs = getcsbyport(cli->csport[i]);
							if (cs)
							for(len=0; len<cs->card.nbprov; len++) {
								clicd.sid = cfg.mgcamd.port;
								clicd.caid = cs->card.caid; 
								clicd.provid = cs->card.prov[len];
								buf[0]=EXT_ADD_CARD; buf[1]=0; buf[2]=0;
								cs_message_send(cli->handle, &clicd, buf, 3, cli->sessionkey);
							}
						}
					}
					else if (cfg.mgcamd.csport[0]) {
						int i;
						for(i=0;i<MAX_CSPORTS;i++) {
							if (!cfg.mgcamd.csport[i]) break;
							cs = getcsbyport(cfg.mgcamd.csport[i]);
							if (cs)
							for(len=0; len<cs->card.nbprov; len++) {
								clicd.sid = cfg.mgcamd.port;
								clicd.caid = cs->card.caid; 
								clicd.provid = cs->card.prov[len];
								buf[0]=EXT_ADD_CARD; buf[1]=0; buf[2]=0;
								cs_message_send(cli->handle, &clicd, buf, 3, cli->sessionkey);
							}
						}
					}
					else {
						cs = cfg.cardserver;
						while(cs) {
							for(len=0; len<cs->card.nbprov; len++) {
								clicd.sid = cfg.mgcamd.port;
								clicd.caid = cs->card.caid; 
								clicd.provid = cs->card.prov[len];
								buf[0]=EXT_ADD_CARD; buf[1]=0; buf[2]=0;
								cs_message_send(cli->handle, &clicd, buf, 3, cli->sessionkey);
							}
							cs = cs->next;
						}
					}

					debugf(" mgcamd: send card data to client '%s'\n", cli->user);
					cli->cardsent = 1;
					break;
				case 0x80:
				case 0x81:
					//debugdump(buf, len, "ECM: ");
					cli->lastecmtime = GetTickCount();
					//cli->ecmnb++; new var for not accepted ecm's
					cli->ecmnb++;
					if (cli->ecm.busy) {
						cli->ecmdenied++;
						// send decode failed
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <|> decode failed to mgcamd client '%s' ch %04x:%06x:%04x, too many ecm requests\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
						break;
					}

					// Check for CAID&SID
					if ( !clicd.caid ) {
						cli->ecmdenied++;
						// send decode failed
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <|> decode failed to mgcamd client '%s' ch %04x:%06x:%04x Invalid CAID\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
						break;
					}
					int i,j,port;
					if (cli->csport[0]) {
						for(i=0; i<MAX_CSPORTS; i++) {
							port = cli->csport[i];
							if (!port) break;
							cs = getcsbyport(port);
							if (cs)
							if (clicd.caid==cs->card.caid) {
								for (j=0; j<cs->card.nbprov;j++) if (clicd.provid==cs->card.prov[j]) break;
								if (j<cs->card.nbprov) break;
							}
						}
						if (!port || !cs) {
							cli->ecmdenied++;
							// send decode failed
							buf[1] = 0; buf[2] = 0;
							cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
							debugf(" <|> decode failed to client '%s' ch %04x:%06x:%04x, Invalid CAID/PROVIDER\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
							break;
						}
					}
					else {
						cs = cfg.cardserver;
						while(cs) {
							if (clicd.caid==cs->card.caid) {
								for (j=0; j<cs->card.nbprov;j++) if (clicd.provid==cs->card.prov[j]) break;
								if (j<cs->card.nbprov) break;
							}
							cs = cs->next;
						}
						if (!cs) {
							cli->ecmdenied++;
							// send decode failed
							buf[1] = 0; buf[2] = 0;
							cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
							debugf(" <|> decode failed to client '%s' ch %04x:%06x:%04x, Invalid CAID/PROVIDER\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
							break;
						}
					}

					// Check for Accepted sids
					if ( !accept_sid(cs,clicd.sid) ) {
						cli->ecmdenied++;
						cs->ecmdenied++;
						// send decode failed
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <|> decode failed to mgcamd client '%s' ch %04x:%06x:%04x SID not accepted\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
						break;
					}

					if ( !accept_ecmlen(len) ) {
						cli->ecmdenied++;
						cs->ecmdenied++;
						// send decode failed
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <|> decode failed to mgcamd client '%s' ch %04x:%06x:%04x, ecm length error(%d)\n", cli->user,clicd.caid,clicd.provid,clicd.sid,len);
						break;
					}

					// ACCEPTED
					memcpy( data, buf, len);

					pthread_mutex_lock(&prg.lockecm); //###

					// Search for ECM
					int ecmid = search_ecmdata_dcw( data,  len, clicd.sid); // dont get failed ecm request from cache
					if ( ecmid!=-1 ) {
						ECM_DATA *ecm=getecmbyid(ecmid);
						ecm->lastrecvtime = GetTickCount();
						//TODO: Add another card for sending ecm
						mg_store_ecmclient(ecmid, cli, clicd.msgid);
						debugf(" <- ecm from mgcamd client '%s' ch %04x:%06x:%04x*\n",cli->user,clicd.caid,clicd.provid,clicd.sid);
						cli->ecm.busy=1;
						cli->ecm.hash = ecm->hash;
					}
					else {
						cs->ecmaccepted++;
						// Setup ECM Request for Server(s)
						ecmid = store_ecmdata(cs->id, buf, len, clicd.sid,clicd.caid,clicd.provid);
						ECM_DATA *ecm=getecmbyid(ecmid);
						mg_store_ecmclient(ecmid, cli, clicd.msgid);
						debugf(" <- ecm from mgcamd client '%s' ch %04x:%06x:%04x (>%dms)\n",cli->user,clicd.caid,clicd.provid,clicd.sid, cli->ecm.dcwtime);
						cli->ecm.busy=1;
						cli->ecm.hash = ecm->hash;
						if (cs->usecache && cfg.cachepeer) {
							pipe_send_cache_find(ecm, cs);
							ecm->waitcache = 1;
							ecm->dcwstatus = STAT_DCW_WAITCACHE;
						} else ecm->dcwstatus = STAT_DCW_WAIT;
					}
					ecm_check_time = mg_dcw_check_time = 0;

					pthread_mutex_unlock(&prg.lockecm); //###
					break;

				// Incoming DCW from client
				case 0xC0:
				case 0xC1:
					cli->lastecmtime = GetTickCount();
					if (!cli->ecm.busy) break;

					pthread_mutex_lock(&prg.lockecm); //###

					ECM_DATA *ecm = getecmbyid( cli->ecm.id );
					if (!ecm) {
						debugf(" <!= wrong cw from Client '%s' ch %04x:%06x:%04x\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->lastecmtime);
						pthread_mutex_unlock(&prg.lockecm);
						break;
					}
					if (ecm->hash!=cli->ecm.hash) {
						debugf(" <!= wrong cw from Client '%s' ch %04x:%06x:%04x\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->lastecmtime);
						pthread_mutex_unlock(&prg.lockecm);
						break;
					}
					if ( (ecm->caid!=clicd.caid) || (ecm->sid!=clicd.sid) || (ecm->provid!=clicd.provid) || (cli->ecm.climsgid!=clicd.msgid) ) {
						debugf(" <!= wrong cw from Client '%s' ch %04x:%06x:%04x\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->lastecmtime);
						pthread_mutex_unlock(&prg.lockecm);
						break;
					}
					if ( (buf[0]&0x81)!=ecm->ecm[0] ) {
						debugf(" <!= wrong cw from Client '%s' ch %04x:%06x:%04x\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->lastecmtime);
						pthread_mutex_unlock(&prg.lockecm);
						break;
					}
					if (buf[2]!=0x10) {
						debugf(" <!= wrong cw from Client '%s' ch %04x:%06x:%04x\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->lastecmtime);
						pthread_mutex_unlock(&prg.lockecm);
						break;
					}
					// Check for DCW
					if (!acceptDCW(&buf[3])) {
						debugf(" <!= wrong cw from Client '%s' ch %04x:%06x:%04x\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->lastecmtime);
						pthread_mutex_unlock(&prg.lockecm);
						break;
					}
					debugf(" <= cw from mgcamd client '%s' ch %04x:%06x:%04x (%dms)\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->lastecmtime);
					if (ecm->dcwstatus!=STAT_DCW_SUCCESS) {
						static char msg[] = "Good dcw from mgcamd Client";
						ecm->statusmsg = msg;
						// Store ECM Answer
						ecm_setdcw( getcsbyid(ecm->csid), ecm, &buf[3], DCW_SOURCE_MGCLIENT, cli->id );
					}
					else {	//TODO: check same dcw between cards
						if ( memcmp(&ecm->cw, &buf[3],16) ) debugf(" !!! different dcw from newcamd client '%s'\n", cli->user);
					}

					pthread_mutex_unlock(&prg.lockecm); //###
					break;

				default:
					if (buf[0]==MSG_KEEPALIVE) {
						// Check for Cache client??
						if (clicd.sid==(('C'<<8)|'H')) { clicd.caid = ('O'<<8)|'K'; cli->cachedcw++; }
						cs_message_send(cli->handle, &clicd, buf, 3, cli->sessionkey);
					}
					else {
						debugf(" mgcamd: unknown message type '%02x' from client '%s'\n",buf[0],cli->user);
						buf[1]=0; buf[2]=0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
					}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// MGCAMD SERVER:  SEND DCW TO CLIENTS
///////////////////////////////////////////////////////////////////////////////


void mg_senddcw_cli(struct mg_client_data *cli)
{
	unsigned char buf[CWS_NETMSGSIZE];
	struct cs_custom_data clicd; // Custom data

	if (cli->ecm.status==STAT_DCW_SENT) {
		debugf(" +> cw send failed to mgcamd client '%s', cw already sent\n", cli->user); 
		return;
	}
	if (cli->handle==INVALID_SOCKET) {
		debugf(" +> cw send failed to mgcamd client '%s', client disconnected\n", cli->user); 
		return;
	}
	if (!cli->ecm.busy) {
		debugf(" +> cw send failed to mgcamd client '%s', no ecm request\n", cli->user); 
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

	clicd.msgid = cli->ecm.climsgid;
	clicd.sid = ecm->sid;
	clicd.caid = ecm->caid;
	clicd.provid = ecm->provid;

	if ( (ecm->hash==cli->ecm.hash)&&(ecm->dcwstatus==STAT_DCW_SUCCESS) ) {
		cli->ecm.lastdcwsrctype = ecm->dcwsrctype;
		cli->ecm.lastdcwsrcid = ecm->dcwsrcid;
		cli->ecm.laststatus=1;
		cli->ecmok++;
		cli->ecmoktime += GetTickCount()-cli->ecm.recvtime;
		buf[0] = ecm->ecm[0];
		buf[1] = 0;
		buf[2] = 0x10;
		memcpy( &buf[3], ecm->cw, 16 );
		cs_message_send( cli->handle, &clicd, buf, 19, cli->sessionkey);
		debugf(" => cw to mgcamd client '%s' ch %04x:%06x:%04x (%dms)\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->ecm.recvtime);
		cli->lastdcwtime = GetTickCount();
	}
	else { //if (ecm->data->dcwstatus==STAT_DCW_FAILED)
		if (enablefreeze) cli->freeze++;
		cli->ecm.laststatus=0;
		cli->ecm.lastdcwsrctype = DCW_SOURCE_NONE;
		cli->ecm.lastdcwsrcid = 0;
		buf[0] = ecm->ecm[0];
		buf[1] = 0;
		buf[2] = 0;
		cs_message_send(  cli->handle, &clicd, buf, 3, cli->sessionkey);
		debugf(" |> decode failed to mgcamd client '%s' ch %04x:%06x:%04x (%dms)\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->ecm.recvtime);
	}
	cli->ecm.busy=0;
	cli->ecm.status = STAT_DCW_SENT;
}

///////////////////////////////////////////////////////////////////////////////

// Check sending cw to clients
uint32 mg_check_sendcw()
{
	struct mg_client_data *cli = cfg.mgcamd.client;
	uint ticks = GetTickCount();
	uint restime = ticks + 10000;
	uint clitime = restime;
	while (cli) {
			if ( (cli->handle!=INVALID_SOCKET)&&(cli->ecm.busy)&&(cli->ecm.status==STAT_ECM_SENT) ) {
				pthread_mutex_lock(&prg.lockecm); //###

				// Check for DCW ANSWER
				ECM_DATA *ecm = getecmbyid(cli->ecm.id);
				if (ecm->hash!=cli->ecm.hash) mg_senddcw_cli( cli );
				struct cardserver_data *cs = getcsbyid(ecm->csid);
				// Check for FAILED
				if (ecm->dcwstatus==STAT_DCW_FAILED) mg_senddcw_cli( cli );
				// Check for SUCCESS
				else if (ecm->dcwstatus==STAT_DCW_SUCCESS) {
					// check for client allowed cw time
					if ( (cli->ecm.recvtime+cli->ecm.dcwtime)<=ticks ) mg_senddcw_cli( cli ); else clitime = cli->ecm.recvtime+cli->ecm.dcwtime;
				}
				// check for timeout
				else if ( (cli->ecm.recvtime+cs->dcwtimeout) < ticks ) {
					mg_senddcw_cli( cli );
				}
				else clitime = cli->ecm.recvtime+cs->dcwtimeout;
				if (restime>clitime) restime = clitime;

				pthread_mutex_unlock(&prg.lockecm); //###
			}
			cli = cli->next;
	}
	return (restime+1);
}


///////////////////////////////////////////////////////////////////////////////
// MGCAMD SERVER: START/STOP
///////////////////////////////////////////////////////////////////////////////

pthread_t mg_cli_tid;
int start_thread_mgcamd()
{
	create_prio_thread(&mg_cli_tid, mg_connect_cli_thread, NULL, 50); // Lock server
	return 0;
}

void done_mgcamd()
{
  if (close(cfg.mgcamd.handle)) debugf("Close failed(%d)",cfg.mgcamd.handle);
}

