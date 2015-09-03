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

///////////////////////////////////////////////////////////////////////////////
// PROTO
///////////////////////////////////////////////////////////////////////////////

void cs_disconnect_cli(struct cs_client_data *cli);
void *cs_connect_cli_thread(void *param);
void cs_cli_recvmsg(struct cs_client_data *cli,	struct cardserver_data *cs);
uint cs_check_sendcw();


struct cs_client_data *getnewcamdclientbyid(struct cardserver_data *cs, uint32 id)
{
	struct cs_client_data *cli = cs->client;
	while (cli) {
		if (cli->id==id) return cli;
		cli = cli->next;
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////

void cs_disconnect_cli(struct cs_client_data *cli)
{
	if (cli->handle!=INVALID_SOCKET) {
		debugf(" newcamd: client '%s' disconnected\n", cli->user);
		close(cli->handle);
		cli->handle = INVALID_SOCKET;
		cli->uptime += GetTickCount()-cli->connected;
		cli->connected = 0;
	}
}


struct cs_clicon {
	struct cardserver_data *cs;
	int sock;
	uint32 ip;
};


void *th_cs_connect_cli(struct cs_clicon *param)
{
    char passwdcrypt[120];
	unsigned char keymod[14];
	int i,index;
	unsigned char sessionkey[16];
	struct cs_custom_data clicd;
	unsigned char buf[CWS_NETMSGSIZE];

	struct cardserver_data *cs = param->cs;
	int sock = param->sock;
	uint32 ip = param->ip;
	free(param);
	// Create random deskey
	for (i=0; i<14; i++) keymod[i] = 0xff & rand();
	// Create NewBox ID
	keymod[3] = (keymod[0]^'N') + keymod[1] + keymod[2];
	keymod[7] = keymod[4] + (keymod[5]^'B') + keymod[6];
	keymod[11] = keymod[8] + keymod[9] + (keymod[10]^'x');

//STAT: SEND RND KEY
	// send random des key

	if (send_nonb(sock, keymod, 14, 500)<=0) {
		debugf(" ERR SEND\n");
		close(sock);
		pthread_exit(NULL);
	}

	uint32 ticks = GetTickCount();
	// Calc SessionKey
	//debugf(" DES Key: "); debughex(keymod,14);
	des_login_key_get(keymod, cs->key, 14, sessionkey);
	//debugf(" Login Key: "); debughex(sessionkey,16);

//md5_crypt("pass", "$1$abcdefgh$",passwdcrypt);
//debugf(" passwdcrypt = %s\n",passwdcrypt);

//STAT: LOGIN INFO
	// 3. login info
	i = cs_message_receive(sock, &clicd, buf, sessionkey,3000);
	if (i<=0) {
		if (i==-2) debugf(" %s: (%s) new connection closed, wrong des key\n", cs->name, ip2string(ip));
		else debugf(" %s: (%s) new connection closed, receive timeout\n", cs->name, ip2string(ip));
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
		debugf(" %s: (%s) new connection closed, wrong username length\n", cs->name, ip2string(ip));
		close(sock);
		pthread_exit(NULL);
	}

	pthread_mutex_lock(&prg.lockcli);
	index = 3;
	struct cs_client_data *usr = cs->client;
	int found = 0;
	while (usr) {
		if (!strcmp(usr->user,(char*)&buf[index])) {
			found=1;
			break;
		}
		usr = usr->next;
	}
	if (!found) {
		pthread_mutex_unlock(&prg.lockcli);
		buf[0] = MSG_CLIENT_2_SERVER_LOGIN_NAK;
		buf[1] = 0;
		buf[2] = 0;
		//cs_message_send(sock, NULL, buf, 3, sessionkey);
		debugf(" %s: (%s) new connection closed, unknown user '%s'\n", cs->name, ip2string(ip), &buf[3]);
		close(sock);
		pthread_exit(NULL);
	}

	// Check password
	index += strlen(usr->user) +1;
	__md5_crypt(usr->pass, "$1$abcdefgh$",passwdcrypt);
	if (!strcmp(passwdcrypt,(char*)&buf[index])) {
		//Check Reconnection
		if (usr->handle!=INVALID_SOCKET) {
			debugf(" %s: user already connected '%s' (%s)\n", cs->name, usr->user, ip2string(ip));
			cs_disconnect_cli(usr);
		}
		// Store program id
		usr->progid = clicd.sid;
		//
		buf[0] = MSG_CLIENT_2_SERVER_LOGIN_ACK;
		buf[1] = 0;
		buf[2] = 0;
		// Send NewBox Id&Version
		if (clicd.provid==0x0057484F) { // WHO 
			clicd.provid = 0x004E4278; // MBx
			clicd.sid = REVISION; // Revision
			cs_message_send(sock, &clicd, buf, 3, sessionkey);
		} else cs_message_send(sock, NULL, buf, 3, sessionkey);
		des_login_key_get( cs->key, (unsigned char*)passwdcrypt, strlen(passwdcrypt),sessionkey);
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
		debugf(" %s: client '%s' connected (%s)\n", cs->name, usr->user, ip2string(ip));
		pipe_wakeup( srvsocks[1] );
	}
	else {
		// send NAK
		buf[0] = MSG_CLIENT_2_SERVER_LOGIN_NAK;
		buf[1] = 0;
		buf[2] = 0;
		//cs_message_send(sock, NULL, buf, 3, sessionkey);
		debugf(" newcamd: client '%s' wrong password (%s)\n", usr->user, ip2string(ip));
		close(sock);
	}
	pthread_mutex_unlock(&prg.lockcli);
	pthread_exit(NULL);
}


void *cs_connect_cli_thread(void *param)
{
	int clientsock;
	struct sockaddr_in clientaddr;
	socklen_t socklen = sizeof(struct sockaddr);
	// Connect Clients 
	pthread_t srv_tid;

	while (1) {
		pthread_mutex_lock(&prg.locksrvcs);

		struct pollfd pfd[MAX_CSPORTS];
		int pfdcount = 0;

		struct cardserver_data *cs = cfg.cardserver;
		while(cs) {
			if (cs->handle>0) {
				cs->ipoll = pfdcount;
				pfd[pfdcount].fd = cs->handle;
				pfd[pfdcount++].events = POLLIN | POLLPRI;
			} else cs->ipoll = -1;
			cs = cs->next;
		}

		int retval = poll(pfd, pfdcount, 3000);

		if (retval>0) {
			struct cardserver_data *cs = cfg.cardserver;
			while(cs) {
				if ( (cs->handle>0) && (cs->ipoll>=0) && (cs->handle==pfd[cs->ipoll].fd) ) {
					if ( pfd[cs->ipoll].revents & (POLLIN|POLLPRI) ) {
						clientsock = accept(cs->handle, (struct sockaddr*)&clientaddr, &socklen );
						if (clientsock<=0) {
							//if (errno == EAGAIN || errno == EINTR) continue;
							//else {
								debugf(" newcamd: Accept failed (errno=%d)\n",errno);
							//}
						}
						else {
							//debugf(" New Client Connection...%s\n", ip2string(clientaddr.sin_addr.s_addr) );
						SetSocketKeepalive(clientsock); 
							struct cs_clicon *clicondata = malloc( sizeof(struct cs_clicon) );
							clicondata->cs = cs; 
							clicondata->sock = clientsock; 
							clicondata->ip = clientaddr.sin_addr.s_addr;
							//while(EAGAIN==pthread_create(&srv_tid, NULL, th_cs_connect_cli,clicondata)) usleep(1000); pthread_detach(&srv_tid);
							create_prio_thread(&srv_tid, (threadfn)th_cs_connect_cli,clicondata, 50);
						}
					}
				}
				cs = cs->next;
			}
		}
		pthread_mutex_unlock(&prg.locksrvcs);
		usleep(3000);
	}
	END_PROCESS = 1;
}


void cs_store_ecmclient(struct cardserver_data *cs, int ecmid, struct cs_client_data *cli, int climsgid)
{
	//check for last ecm recv time
	uint32 ticks = GetTickCount();
	cli->ecm.dcwtime = cli->dcwtime;
	cli->ecm.recvtime = ticks;
	cli->ecm.id = ecmid;
	cli->ecm.climsgid = climsgid;
	cli->ecm.status = STAT_ECM_SENT;
}


void cs_cli_recvmsg(struct cs_client_data *cli,	struct cardserver_data *cs)
{
	struct cs_custom_data clicd; // Custom data
	int len;
	unsigned char buf[CWS_NETMSGSIZE];
	unsigned char data[CWS_NETMSGSIZE]; // for other use

	if (cli->handle>0) {
		len = cs_msg_chkrecv(cli->handle);
		if (len==0) {
			debugf(" newcamd: client '%s' read failed %d\n", cli->user,len);
			cs_disconnect_cli(cli);
		}
		else if (len==-1) {
			if (!cli->chkrecvtime) cli->chkrecvtime = GetTickCount();
			else if ( (cli->chkrecvtime+300)<GetTickCount() ) {
				debugf(" newcamd: client '%s' read timeout %d\n", cli->user,len);
				cs_disconnect_cli(cli);
			}
		}
		else if (len>0) {
			cli->chkrecvtime = 0;
			len = cs_message_receive(cli->handle, &clicd, buf, cli->sessionkey,3);
			if (len==0) {
				debugf(" newcamd: client '%s' read failed %d\n", cli->user,len);
				cs_disconnect_cli(cli);
			}
			else if (len<0) {
				debugf(" newcamd: client '%s' read failed %d(%d)\n", cli->user,len,errno);
				cs_disconnect_cli(cli);
			}
			else if (len>0) switch ( buf[0] ) {
				case MSG_CARD_DATA_REQ:
					memset( buf, 0, sizeof(buf) );
					buf[0] = MSG_CARD_DATA;
					if (!cli->card.caid) {
						buf[4] = cs->card.caid>>8;
						buf[5] = cs->card.caid&0xff;
						buf[14] = cs->card.nbprov;
						for(len=0;len<cs->card.nbprov;len++) {
							buf[15+11*len] = cs->card.prov[len]>>16;
							buf[16+11*len] = cs->card.prov[len]>>8;
							buf[17+11*len] = cs->card.prov[len]&0xff;
						}
						cs_message_send(cli->handle, &clicd, buf, 15+11*cs->card.nbprov, cli->sessionkey);
					}
					else {
						buf[4] = cli->card.caid>>8;
						buf[5] = cli->card.caid&0xff;
						buf[14] = cli->card.nbprov;
						for(len=0;len<cli->card.nbprov;len++) {
							buf[15+11*len] = cli->card.prov[len]>>16;
							buf[16+11*len] = cli->card.prov[len]>>8;
							buf[17+11*len] = cli->card.prov[len]&0xff;
						}
						cs_message_send(cli->handle, &clicd, buf, 15+11*cs->card.nbprov, cli->sessionkey);
					}
					//debugf(" newcamd: send card data to client '%s'\n", cli->user);
					break;
				case 0x80:
				case 0x81:
					//debugdump(buf, len, "ECM: ");
					cli->lastecmtime = GetTickCount();
					cli->ecmnb++;
					if (cli->ecm.busy) {
						cli->ecmdenied++;
						// send decode failed
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <|> decode failed to client '%s' ch %04x:%06x:%04x, too many ecm requests\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
						break;
					}
					// Check for caid, accept caid=0
					if (!clicd.caid) clicd.caid = cs->card.caid;
					if (clicd.caid!=cs->card.caid) {
						cli->ecmdenied++;
						// send decode failed
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <|> decode failed to client '%s' ch %04x:%06x:%04x Wrong CAID\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
						break;
					}
					// Check for provid, accept provid==0
					if ( !accept_prov(cs,clicd.provid) ) {
						cli->ecmdenied++;
						cs->ecmdenied++;
						// send decode failed
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <|> decode failed to client '%s' ch %04x:%06x:%04x Wrong PROVIDER\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
						break;
					}
					// Check for Accepted sids
					if ( !accept_sid(cs,clicd.sid) ) {
						cli->ecmdenied++;
						cs->ecmdenied++;
						// send decode failed
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <|> decode failed to client '%s' ch %04x:%06x:%04x SID not accepted\n", cli->user,clicd.caid,clicd.provid,clicd.sid);
						break;
					}
					// Check ECM length
					if ( !accept_ecmlen(len) ) {
						cli->ecmdenied++;
						cs->ecmdenied++;
						buf[1] = 0; buf[2] = 0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
						debugf(" <- ecm length error from client '%s'\n",cli->user);
						break;
					}
					// ACCEPTED
					memcpy( &data[0], &buf[0], len);

					pthread_mutex_lock(&prg.lockecm); //###

					// Search for ECM
					int ecmid = search_ecmdata_dcw( data,  len, clicd.sid); // dont get failed ecm request from cache
					if ( ecmid!=-1 ) {
						ECM_DATA *ecm=getecmbyid(ecmid);
						ecm->lastrecvtime = GetTickCount();
						//TODO: Add another card for sending ecm
						cs_store_ecmclient(cs, ecmid, cli, clicd.msgid);
						debugf(" <- ecm from client '%s' ch %04x:%06x:%04x*\n",cli->user,clicd.caid,clicd.provid,clicd.sid);
						cli->ecm.busy=1;
						cli->ecm.hash = ecm->hash;
					}
					else {
						cs->ecmaccepted++;
						// Setup ECM Request for Server(s)
						ecmid = store_ecmdata(cs->id, &data[0], len, clicd.sid,clicd.caid,clicd.provid);
						ECM_DATA *ecm=getecmbyid(ecmid);
						cs_store_ecmclient(cs, ecmid, cli, clicd.msgid);
						debugf(" <- ecm from client '%s' ch %04x:%06x:%04x (>%dms)\n",cli->user,clicd.caid,clicd.provid,clicd.sid, cli->ecm.dcwtime);
						cli->ecm.busy=1;
						cli->ecm.hash = ecm->hash;
						if (cs->usecache && cfg.cachepeer) {
							pipe_send_cache_find(ecm, cs);
							ecm->waitcache = 1;
							ecm->dcwstatus = STAT_DCW_WAITCACHE;
						} else ecm->dcwstatus = STAT_DCW_WAIT;
					}
					ecm_check_time = cs_dcw_check_time = 0;

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
					debugf(" <= cw from Client '%s' ch %04x:%06x:%04x (%dms)\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->lastecmtime);
					if (ecm->dcwstatus!=STAT_DCW_SUCCESS) {
						static char msg[] = "Good dcw from Newcamd Client";
						ecm->statusmsg = msg;
						// Store ECM Answer
						ecm_setdcw( getcsbyid(ecm->csid), ecm, &buf[3], DCW_SOURCE_CSCLIENT, (ecm->csid<<16)|cli->id );
					}
					else {	//TODO: check same dcw between cards
						if ( memcmp(&ecm->cw, &buf[3],16) ) debugf(" !!! different dcw from newcamd client '%s'\n", cli->user);
					}

					pthread_mutex_unlock(&prg.lockecm); //###
					break;

				default:
					if (buf[0]==MSG_KEEPALIVE) {
						// Check for Cache client??
						if (clicd.sid==(('C'<<8)|'H')) clicd.caid = ('O'<<8)|'K';
						//debugf(" <-> keepalive from/to client '%s'\n",cli->user);
						cs_message_send(cli->handle, &clicd, buf, 3, cli->sessionkey);
					}
					else {
						debugf(" newcamd: unknown message type '%02x' from client '%s'\n",buf[0],cli->user);
						buf[1]=0; buf[2]=0;
						cs_message_send( cli->handle, &clicd, buf, 3, cli->sessionkey);
					}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// DCW
///////////////////////////////////////////////////////////////////////////////

void cs_senddcw_cli(struct cs_client_data *cli)
{
	unsigned char buf[CWS_NETMSGSIZE];
	struct cs_custom_data clicd; // Custom data

	if (cli->ecm.status==STAT_DCW_SENT) {
		debugf(" +> cw send failed to client '%s', cw already sent\n", cli->user); 
		return;
	}
	if (cli->handle==INVALID_SOCKET) {
		debugf(" +> cw send failed to client '%s', client disconnected\n", cli->user); 
		return;
	}
	if (!cli->ecm.busy) {
		debugf(" +> cw send failed to client '%s', no ecm request\n", cli->user); 
		return;
	}

	ECM_DATA *ecm = getecmbyid(cli->ecm.id);

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
		memcpy( &buf[3], &ecm->cw, 16 );
		cs_message_send( cli->handle, &clicd, buf, 19, cli->sessionkey);
		debugf(" => cw to client '%s' ch %04x:%06x:%04x (%dms)\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->ecm.recvtime);
		cli->lastdcwtime = GetTickCount();
	}
	else { //if (ecm->data->dcwstatus==STAT_DCW_FAILED)
		cli->ecm.laststatus=0;
		cli->ecm.lastdcwsrctype = DCW_SOURCE_NONE;
		cli->ecm.lastdcwsrcid = 0;
		buf[0] = ecm->ecm[0];
		buf[1] = 0;
		buf[2] = 0;
		cs_message_send(  cli->handle, &clicd, buf, 3, cli->sessionkey);
		debugf(" |> decode failed to client '%s' ch %04x:%06x:%04x (%dms)\n", cli->user, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-cli->ecm.recvtime);
	}
	cli->ecm.busy=0;
	cli->ecm.status = STAT_DCW_SENT;
}

// Check sending cw to clients
uint cs_check_sendcw()
{
	struct cardserver_data *cs = cfg.cardserver;
	uint restime = GetTickCount() + 10000;
	uint clitime = 0;

	while(cs) {
		if (cs->handle>0) {
			struct cs_client_data *cli = cs->client;
			uint ticks = GetTickCount();
			while (cli) {
				if ( (cli->handle!=INVALID_SOCKET)&&(cli->ecm.busy)&&(cli->ecm.status==STAT_ECM_SENT) ) {
					clitime = ticks+11000;

					pthread_mutex_lock(&prg.lockecm); //###

					// Check for DCW ANSWER
					ECM_DATA *ecm = getecmbyid(cli->ecm.id);
					if (ecm->hash!=cli->ecm.hash) cs_senddcw_cli( cli );
					//debugf(" cs_check_sendcw: %s:%s ch %04x:%06x:%04x\n", cs->name, cli->user, ecm->caid, ecm->provid, ecm->sid);
					// Check for FAILED
					if (ecm->dcwstatus==STAT_DCW_FAILED) {
						cs_senddcw_cli( cli );
					}
					// Check for SUCCESS
					else if (ecm->dcwstatus==STAT_DCW_SUCCESS) {
						// check for client allowed cw time
						if ( (cli->ecm.recvtime+cli->ecm.dcwtime)<=ticks )
							cs_senddcw_cli( cli );
						else
							clitime = cli->ecm.recvtime+cli->ecm.dcwtime;
					}
					// check for timeout
					else if ( (cli->ecm.recvtime+cs->dcwtimeout) <= ticks ) {
						cs_senddcw_cli( cli );
					}
					else clitime = cli->ecm.recvtime+cs->dcwtimeout;
					if (restime>clitime) restime = clitime;

					pthread_mutex_unlock(&prg.lockecm); //###
				}
				cli = cli->next;
			}
		}
		cs = cs->next;
	}
	return (restime+1);
}

