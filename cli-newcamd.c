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


static char string_mgcamd[] = "Mgcamd";

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void cs_disconnect_srv(struct cs_server_data *srv)
{
	static char msg[]= "Disconnected";
	srv->statmsg = msg;
	// Disconnect server
	debugf(" newcamd: server (%s:%d) disconnected\n", srv->host->name, srv->port);
	close(srv->handle);
	srv->handle = INVALID_SOCKET;
	if (srv->busy) ecm_setsrvflag(srv->busyecmid, srv->id, ECM_SRV_EXCLUDE);
	srv->uptime += GetTickCount()-srv->connected;
	srv->keepalivetime = 0;
	srv->keepalivesent = 0;
	srv->host->checkiptime = 0; // maybe ip changed
	// Remove Cards
	while (srv->card) {
		struct cs_card_data *card = srv->card;
		srv->card = srv->card->next;
		free(card);
	}
}

int cs_connect_srv(struct cs_server_data *srv, int fd)
{
	char passwdcrypt[120];
	unsigned char keymod[14];
	int i,index,len;
	unsigned char buf[CWS_NETMSGSIZE];
	unsigned char sessionkey[16];
	// INIT
	srv->progname = NULL;
	memset( srv->version, 0, sizeof(srv->version) );
	memset( srv->build, 0, 32);
	//
	if( recv_nonb(fd, keymod, 14,5000) != 14 ) {
		static char msg[]= "Server does not return init sequence";
		srv->statmsg = msg;
		debugf(" newcamd: server does not return init sequence\n");
		return -1;
	}
	// Check newbox
	int isnewbox = 0;
	uchar a = (keymod[0]^'N') + keymod[1] + keymod[2];
	uchar b = keymod[4] + (keymod[5]^'B') + keymod[6];
	uchar c = keymod[8] + keymod[9] + (keymod[10]^'x');
	if ( (a==keymod[3])&&(b==keymod[7])&&(c==keymod[11]) ) {
		isnewbox = 1;
	}

	//debugdump(keymod,14,"Recv DES Key: ");
	des_login_key_get(keymod, srv->key, 14, sessionkey);  
	//debugdump(sessionkey,16,"Login Key: ");

	// 3. Send login info
	struct cs_custom_data clicd; // Custom data
	memset( &clicd, 0, sizeof(clicd));
	//clicd.sid =  0x4343; // CCcam
	clicd.sid =  cfg.newcamdclientid; // Mgcamd

	index = 3;
	buf[0] = MSG_CLIENT_2_SERVER_LOGIN;
	buf[1] = 0;
	strcpy( (char*)&buf[3], srv->user);
	index += strlen(srv->user)+1;

	__md5_crypt(srv->pass, "$1$abcdefgh$",passwdcrypt);
	//debugf(" passwdcrypt = %s\n",passwdcrypt);
	strcpy((char*)buf+index, (char*)passwdcrypt);
	index+=strlen(passwdcrypt)+1;
	if (isnewbox) clicd.provid=0x0057484F;
	cs_message_send(fd, &clicd, buf, index, sessionkey);
	srv->ping = GetTickCount();
	// 3.1 Get login answer
	len = cs_message_receive(fd, &clicd, buf, sessionkey,5000);
	if (len<3) {
		static char msg[]= "Login error";
		srv->statmsg = msg;
		debugf(" newcamd: login answer length error (%d) from server (%s:%d)\n",len, srv->host->name,srv->port);
		return INVALID_SOCKET;
	}
	if ( buf[0] == MSG_CLIENT_2_SERVER_LOGIN_NAK ) {
		static char msg[]= "Invalid user/pass";
		srv->statmsg = msg;
		debugf(" newcamd: login failed to server (%s:%d)\n", srv->host->name,srv->port);
		return INVALID_SOCKET;
	}
	else if( buf[0] != MSG_CLIENT_2_SERVER_LOGIN_ACK ) {
		static char msg[]= "Login error";
		srv->statmsg = msg;
		debugf(" newcamd: Error, expected MSG_CLIENT_2_SERVER_LOGIN_ACK\n");
		return INVALID_SOCKET;
	}
	else debugf(" newcamd: connect to server (%s:%d) user '%s'\n", srv->host->name, srv->port, srv->user);
	// Ping
	srv->ping = GetTickCount()-srv->ping;
	// mgcamd protocol version?
	if ( (clicd.sid==0x6E73)&&( (clicd.provid>>24)==0x14) ) srv->progname = string_mgcamd;
	//
	static char msg[]= "Connected";
	srv->statmsg = msg;
	des_login_key_get( srv->key, (uint8*)passwdcrypt, strlen(passwdcrypt), sessionkey);
//debugdump(sessionkey,16,"sessionkey: ");
	memcpy( srv->sessionkey, sessionkey,16);

	// 4. Send MSG_CARD_DATA_REQ
	memset( &clicd, 0, sizeof(clicd) );
	clicd.msgid = 1;
	buf[0]=MSG_CARD_DATA_REQ;
	buf[1]=0; buf[2]=0;
	cs_message_send(fd, &clicd, buf, 3, srv->sessionkey);
	len = cs_message_receive( fd, NULL, buf, srv->sessionkey,3000);
	if (len==0) {
		static char msg[]= "Disconnected";
		srv->statmsg = msg;
		debugf(" newcamd: client disconnected\n");
		return INVALID_SOCKET;
	}
	else if (len<0) {
		static char msg[]= "failed to receive card data";
		srv->statmsg = msg;
		debugf(" newcamd: Error Recv MSG_CARD_DATA (%d)\n",len);
		return INVALID_SOCKET;
	}
	if (buf[0]!=MSG_CARD_DATA) {
		static char msg[]= "failed to receive card data";
		srv->statmsg = msg;
		debugf(" newcamd: expected MSG_CARD_DATA\n");
		return INVALID_SOCKET;
	}

	//pthread_mutex_lock(&prg.locksrv);

	else if (len<15) {
		debugf(" newcamd: MSG_CARD_DATA, length error (%d)\n",len);
		//return INVALID_SOCKET;
	}
	else {
		// 5. Parse CAID and PROVID(s)
		if (buf[4] || buf[5]) { // CAID != 0x0000
			struct cs_card_data *pcard = malloc( sizeof(struct cs_card_data) );
			memset(pcard, 0, sizeof(struct cs_card_data) );
			pcard->next = srv->card;
			pcard->caid = ((buf[4]<<8) | buf[5]);
			pcard->nbprov = buf[14];
			pcard->uphops = 1;
			pcard->sids = NULL;
			if (pcard->nbprov>CARD_MAXPROV) pcard->nbprov = CARD_MAXPROV;
			debugf(" newcamd: caid %04x providers %d\n", pcard->caid,buf[14]);
			for( i=0; i<pcard->nbprov; i++ ) {
				pcard->prov[i] = (buf[15+11*i]<<16)|(buf[16+11*i]<<8)|buf[17+11*i];
				debugf(" newcamd: provider %02d = %06X\n", i+1, pcard->prov[i]);
			}
			srv->card = pcard;
		} else debugf(" newcamd: caid 0\n");
	}

	srv->keepalivesent = GetTickCount();
	srv->cscached = 0;

	srv->connected = GetTickCount();
	srv->keepalivetime = GetTickCount();

	srv->busy = 0;
	srv->lastecmoktime = 0;
	srv->lastecmtime = 0;
	srv->lastdcwtime = 0;
	srv->chkrecvtime = 0;
	srv->handle = fd;
	pipe_wakeup( srvsocks[1] );
	//pthread_mutex_unlock(&prg.locksrv);
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int cs_sendecm_srv(struct cardserver_data *cs, struct cs_server_data *srv, ECM_DATA *ecm)
{
	unsigned char buf[CWS_NETMSGSIZE];
	struct cs_custom_data srvcd; // Custom data

	if (!cs->cssendsid) srvcd.sid = 0; else srvcd.sid = ecm->sid;
	srvcd.caid = ecm->caid;
	srvcd.provid = ecm->provid;
	srvcd.msgid = ecm->srvmsgid;

	memcpy( &buf[0], &ecm->ecm[0], ecm->ecmlen );
	return cs_message_send(  srv->handle, &srvcd, buf, ecm->ecmlen, srv->sessionkey);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void cs_srv_recvmsg(struct cs_server_data *srv)
{
	struct cs_custom_data srvcd; // Custom data
	int len;
	ECM_DATA *ecm;
	unsigned char buf[CWS_NETMSGSIZE];

	if ( (srv->handle<=0)||(srv->type!=TYPE_NEWCAMD) ) return;

	len = cs_msg_chkrecv(srv->handle);
	if (len==0) {
		debugf(" newcamd: server (%s:%d) read failed %d\n", srv->host->name, srv->port, len);
		cs_disconnect_srv(srv);
	}
	else if (len==-1) {
		if (!srv->chkrecvtime) srv->chkrecvtime = GetTickCount();
		else if ( (srv->chkrecvtime+300)<GetTickCount() ) {
			debugf(" newcamd: server (%s:%d) wrong data\n", srv->host->name, srv->port);
			cs_disconnect_srv(srv);
		}
	}
	else if (len>0) {
		srv->chkrecvtime = 0;
		len = cs_message_receive(srv->handle, &srvcd, buf, srv->sessionkey,3);
		if (len==0) {
			debugf(" newcamd: server (%s:%d) read failed %d\n", srv->host->name, srv->port, len);
			cs_disconnect_srv(srv);
		}
		else if (len<0) {
			debugf(" newcamd: server (%s:%d) read failed %d(%d)\n", srv->host->name, srv->port, len, errno);
			cs_disconnect_srv(srv);
		}
		else if (len>0) {
			switch ( buf[0] ) {
				case 0x80:
				case 0x81:
					srv->lastdcwtime = GetTickCount();
					if (!srv->busy) {
						debugf(" [!] dcw error from server (%s:%d), unknown ecm request\n",srv->host->name,srv->port);
						break;
					}
					srv->busy = 0;
					if (srvcd.msgid!=srv->busyecmid) {
						debugf(" [!] dcw error from server (%s:%d), wrong message-id!!!\n",srv->host->name,srv->port);
						break;
					}

					pthread_mutex_lock(&prg.lockecm); //###

					ecm = getecmbyid(srv->busyecmid);
					if (!ecm) {
						debugf(" [!] dcw error from server (%s:%d), ecm not found!!!\n",srv->host->name,srv->port);
						pthread_mutex_unlock(&prg.lockecm); //###
						break;
					}
					// check for ECM???
					if (ecm->hash!=srv->busyecmhash) {
						debugf(" [!] dcw error from server (%s:%d), ecm deleted!!!\n",srv->host->name,srv->port);
						pthread_mutex_unlock(&prg.lockecm); //###
						break;
					}

					if (buf[2]==0x10) {
						// Check for DCW
						if (!acceptDCW(&buf[3])) {
							srv->ecmerrdcw ++;
							pthread_mutex_unlock(&prg.lockecm); //###
							break;
						}
						srv->ecmok++;
						srv->lastecmoktime = GetTickCount()-srv->lastecmtime;
						srv->ecmoktime += srv->lastecmoktime;

						ecm_setsrvflagdcw( ecm->srvmsgid, srv->id, ECM_SRV_REPLY_GOOD,&buf[3] );
						debugf(" <= cw from server (%s:%d) ch %04x:%06x:%04x (%dms)\n", srv->host->name,srv->port, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-srv->lastecmtime);

						if (ecm->dcwstatus!=STAT_DCW_SUCCESS) {
							static char msg[] = "Good dcw from Newcamd server";
							ecm->statusmsg = msg;
							// Store ECM Answer
							ecm_setdcw( getcsbyid(ecm->csid), ecm, &buf[3], DCW_SOURCE_SERVER, srv->id );
						}
						else {	//TODO: check same dcw between cards
							srv->ecmerrdcw ++;
							if ( memcmp(&ecm->cw, &buf[3],16) ) debugf(" !!! different dcw from server (%s:%d)\n",srv->host->name,srv->port);
						}
						// ADD IN SID LIST
						struct cardserver_data *cs=getcsbyid(ecm->csid);
						if (cs) {
							cardsids_update( srv->busycard, ecm->provid, ecm->sid, 1);
							srv_cstatadd( srv, cs->id, 1 , srv->lastecmoktime);
						}
					}
					else {
						struct cardserver_data *cs=getcsbyid(ecm->csid);
						if ( cs && (ecm->dcwstatus!=STAT_DCW_SUCCESS) && (srv->retry<cs->csretry) ) {
							if ( (GetTickCount()-ecm->recvtime)<cs->cstimeout )
							if (cs_sendecm_srv(cs, srv, ecm)>0) {
								srv->retry++;
								ecm->lastsendtime = GetTickCount();
								debugf(" (RE) -> ecm to server (%s:%d) ch %04x:%06x:%04x\n",srv->host->name,srv->port,ecm->caid,ecm->provid,ecm->sid);
								srv->lastecmtime = GetTickCount();
								srv->ecmnb++;
								srv->busy=1;
								srv->busyecmid = ecm->srvmsgid;
								pthread_mutex_unlock(&prg.lockecm); //###
								break;
							}
						}

						ecm_setsrvflag(ecm->srvmsgid, srv->id, ECM_SRV_REPLY_FAIL);
						debugf(" <| decode failed from server (%s:%d) ch %04x:%06x:%04x (%dms)\n", srv->host->name,srv->port, ecm->caid,ecm->provid,ecm->sid, GetTickCount()-srv->lastecmtime);
						// ADD IN SID LIST
						if (cs) {
							cardsids_update( srv->busycard, ecm->provid, ecm->sid, -1);
							srv_cstatadd( srv, cs->id, 0 , 0);
						}
						ecm_check_time = 0;
					}
					pthread_mutex_unlock(&prg.lockecm); //###
					break;

				// ADD CARD
				case 0xD3:
					if (srvcd.caid) { // CAID != 0x0000
						struct cs_card_data *pcard;
						pcard = malloc( sizeof(struct cs_card_data) );
						memset(pcard, 0, sizeof(struct cs_card_data) );
						pcard->caid = srvcd.caid;
						pcard->nbprov = 1;
						pcard->prov[0] = srvcd.provid;
						pcard->uphops = 1;
						pcard->sids = NULL;
						//debugf(" (ext-add-card) caid %04x provider 01 = %06X\n", pcard->caid, pcard->prov[0]);
						pcard->next = srv->card;
						srv->card = pcard;
					}
					break;

				default:
					if (buf[0]==MSG_KEEPALIVE) {
						if (srv->keepalivesent) {
							if ( ( srvcd.sid==(('C'<<8)|'H') ) && ( srvcd.caid==(('O'<<8)|'K') ) ) srv->cscached = 1;
							srv->ping = GetTickCount() - srv->keepalivetime;
							//debugf(" <- keepalive from server (%s:%d) ping %dms\n",srv->host->name,srv->port,srv->ping);
							srv->keepalivesent = 0;
						}
						//else debugf(" <- Error keepalive from server (%s:%d)\n",srv->host->name,srv->port);
					} else {
						debugf(" unknown message type '%02x' CAID:%04X PROVID:%06X from server (%s:%d)\n",buf[0],srvcd.caid,srvcd.provid,srv->host->name,srv->port);
					}
			}
			srv->keepalivetime = GetTickCount();
		}
	}
}


void cs_check_keepalive(struct cs_server_data *srv)
{
	struct cs_custom_data clicd; // Custom data
	unsigned char buf[CWS_NETMSGSIZE];

	if ( (srv->handle<=0)||(srv->type!=TYPE_NEWCAMD) ) return;

	// Check for sending keep alive
	if (!srv->keepalivesent) {
		if ( srv->keepalivetime+(KEEPALIVE_NEWCAMD*1000) < GetTickCount() ) {
			buf[0] = MSG_KEEPALIVE;
			buf[1] = 0;
			buf[2] = 0;
			//debugf(" -> keepalive to server (%s:%d)\n",srv->host->name,srv->port);
			cs_message_send( srv->handle, NULL, buf, 3, srv->sessionkey);
			srv->keepalivetime = GetTickCount();
			srv->keepalivesent = 1;
		}
	}
	else {
		if ( srv->keepalivesent+10000 < GetTickCount() ) { ///???
			// Send keepalive Caching Check???
			clicd.msgid = 0;
			clicd.sid = ('C'<<8) | 'H';
			clicd.caid = 0;
			clicd.provid = 0;
			buf[0] = MSG_KEEPALIVE;
			buf[1] = 0;
			buf[2] = 0;
			cs_message_send( srv->handle, &clicd, buf, 3, srv->sessionkey);
			srv->keepalivesent = GetTickCount();
		}
	}
}

