////
// File: srv-radegast.c
/////


void *rdgd_connect_cli_thread(void *param);
void rdgd_getclimsg();
uint rdgd_check_sendcw();


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void rdgd_senddcw_cli(struct rdgd_client_data *cli)
{
	unsigned char buf[CWS_NETMSGSIZE];

	if (cli->ecm.status==STAT_DCW_SENT) {
		debugf(" +> cw send failed to client (%s), cw already sent\n", ip2string(cli->ip));
		return;
	}
	if (cli->handle==INVALID_SOCKET) {
		debugf(" +> cw send failed to client (%s), client disconnected\n", ip2string(cli->ip));
		return;
	}
	if (!cli->ecm.busy) {
		debugf(" +> cw send failed to client (%s), no ecm request\n", ip2string(cli->ip));
		return;
	}

	ECM_DATA *ecm = getecmbyid(cli->ecm.id);
	if (ecm) {
		cli->ecm.lastcaid = ecm->caid;
		cli->ecm.lastprov = ecm->provid;
		cli->ecm.lastsid = ecm->sid;
		cli->ecm.lastdecodetime = GetTickCount()-cli->ecm.recvtime;
	}

	if ( ecm && (ecm->dcwstatus==STAT_DCW_SUCCESS) ) {
		cli->ecm.lastdcwsrctype = ecm->dcwsrctype;
		cli->ecm.lastdcwsrcid = ecm->dcwsrcid;
		cli->ecm.laststatus=1;
		cli->ecmok++;
		cli->ecmoktime += GetTickCount()-cli->ecm.recvtime;
		// Send DCW
		buf[0] = 0x02;
		buf[1] = 0x12;
		buf[2] = 0x05;
		buf[3] = 0x10;
		memcpy( &buf[4], ecm->cw, 16 );
		rdgd_message_send( cli->handle, buf, 0x14);
		debugf(" => cw to client (%s) ch %04x:%06x:%04x (%dms)\n", ip2string(cli->ip), ecm->caid, ecm->provid, ecm->sid, GetTickCount()-cli->ecm.recvtime);
		cli->lastdcwtime = GetTickCount();
	}
	else { //if (ecm->data->dcwstatus==STAT_DCW_FAILED)
		cli->ecm.laststatus=0;
		cli->ecm.lastdcwsrctype = DCW_SOURCE_NONE;
		cli->ecm.lastdcwsrcid = 0;
		buf[0] = 0x02;
		buf[1] = 0x02;
		buf[2] = 0x04;
		buf[3] = 0x00;
		rdgd_message_send( cli->handle, buf, 4);
		debugf(" |> decode failed to client (%s) ch %04x:%06x:%04x (%dms)\n", ip2string(cli->ip), ecm->caid, ecm->provid,ecm->sid, GetTickCount()-cli->ecm.recvtime);
	}
	cli->ecm.busy=0;
	cli->ecm.status = STAT_DCW_SENT;
}



///////////////////////////////////////////////////////////////////////////////

void rdgd_disconnect_cli(struct cardserver_data *cs,struct rdgd_client_data *cli)
{
	if (cli)
	if (cli->handle>0) {
		//pthread_mutex_lock(&prg.lockrdgdcli);
		debugf(" radegast: client (%s) disconnected\n", ip2string(cli->ip));
		close(cli->handle);
		// Remove
		struct rdgd_client_data *n = cs->rdgd.client;
		if (n) {
			if (n==cli) {
				cs->rdgd.client = n->next;
				free(cli);
			}
			else {
				while (n->next) {
					if (cli==n->next) {
						n->next = n->next->next;
						free(cli);
						break;
					}
					n = n->next;
				}
			}
		}
		//pthread_mutex_unlock(&prg.lockrdgdcli);
	}
}


void *rdgd_connect_cli_thread(void *param)
{
	int clientsock;
	struct sockaddr_in clientaddr;
	socklen_t socklen = sizeof(struct sockaddr);

	while (1) {
		pthread_mutex_lock(&prg.lockrdgdsrv);

		struct pollfd pfd[MAX_CSPORTS];
		int pfdcount = 0;

		struct cardserver_data *cs = cfg.cardserver;
		while(cs) {
			if (cs->rdgd.handle>0) {
				cs->rdgd.ipoll = pfdcount;
				pfd[pfdcount].fd = cs->rdgd.handle;
				pfd[pfdcount++].events = POLLIN | POLLPRI;
			} else cs->rdgd.ipoll = -1;
			cs = cs->next;
		}

		int retval = poll(pfd, pfdcount, 3000);

		if (retval>0) {
			struct cardserver_data *cs = cfg.cardserver;
			while(cs) {
				if ( (cs->rdgd.handle>0) && (cs->rdgd.ipoll>=0) && (cs->rdgd.handle==pfd[cs->rdgd.ipoll].fd) ) {
					if ( pfd[cs->rdgd.ipoll].revents & (POLLIN|POLLPRI) ) {
						clientsock = accept(cs->rdgd.handle, (struct sockaddr*)&clientaddr, &socklen );
						if (clientsock<0) {
							if (errno == EAGAIN || errno == EINTR) continue;
							else {
								debugf(" Radegast: Accept failed (errno=%d)\n",errno);
								usleep(1000);
							}
						}
						else {
							// ADD TO DB
							struct rdgd_client_data *cli = malloc( sizeof(struct rdgd_client_data) );
							memset( cli, 0, sizeof(struct rdgd_client_data) );
							cli->chkrecvtime = 0;
							cli->handle = clientsock; 
							cli->ip = clientaddr.sin_addr.s_addr;
							SetSocketKeepalive(clientsock); 
							pthread_mutex_lock(&prg.lockrdgdcli);
							cli->next = cs->rdgd.client;
							cs->rdgd.client = cli;
							debugf(" radegast: client (%s) connected\n", ip2string(cli->ip));
							pthread_mutex_unlock(&prg.lockrdgdcli);
							pipe_wakeup( srvsocks[1] );
						}
					}
				}
				cs = cs->next;
			}
		}
		pthread_mutex_unlock(&prg.lockrdgdsrv);
		usleep(3000);
	}
	END_PROCESS = 1;
}



void rdgd_store_ecmclient(struct cardserver_data *cs, int ecmid, struct rdgd_client_data *cli)
{
	//check for last ecm recv time
	uint32 ticks = GetTickCount();
	cli->ecm.recvtime = ticks;
	cli->ecm.id = ecmid;
    cli->ecm.status = STAT_ECM_SENT;
}


int accept_ecm (struct cardserver_data *cs, uint16 caid, uint32 provid, uint16 sid)
{
	// Check for caid, accept caid=0
	if ( !accept_caid(cs,caid) ) return 0;

	// Check for provid, accept provid==0
	if ( !accept_prov(cs,provid) ) return 0;

	// Check for Accepted sids
	if ( !accept_sid(cs,sid) ) return 0;

	return 1;
}


void rdgd_cli_recvmsg(struct rdgd_client_data *cli, struct cardserver_data *cs)
{
	int len;
	unsigned char buf[300];
	int i;

	if (cli->handle>0) {
		len = rdgd_check_message(cli->handle);
		if (len==0) {
			debugf(" radegast: client (%s) read failed %d\n", ip2string(cli->ip),len);
			rdgd_disconnect_cli(cs,cli);
		}
		else if (len==-1) {
			if (!cli->chkrecvtime) cli->chkrecvtime = GetTickCount();
			else if ( (cli->chkrecvtime+300)<GetTickCount() ) {
				debugf(" radegast: client (%s) read failed %d\n", ip2string(cli->ip),len);
				rdgd_disconnect_cli(cs,cli);
			}
		}
		else if (len>0) {
			cli->chkrecvtime = 0;
			len = rdgd_message_receive(cli->handle, buf, 0);
			if (len==0) {
				debugf(" radegast: client (%s) read failed %d\n", ip2string(cli->ip),len);
				rdgd_disconnect_cli(cs,cli);
			}
			else if (len<0) {
				debugf(" radegast: client '%s' read failed %d(%d)\n", ip2string(cli->ip),len,errno);
				rdgd_disconnect_cli(cs,cli);
			}
			else if (len>0) switch ( buf[0] ) {
				case 0x01: // ECM
					cli->lastecmtime = GetTickCount();
					cli->ecmnb++;
					cli->ecm.id = -1; // ECM DENIED
					if (cli->ecm.busy) {
						cli->ecmdenied++;
						rdgd_senddcw_cli(cli);
						break;
					}
					uint16 caid = 0;
					uint32 provid = 0;
					unsigned char ecmdata[300];
					memset(ecmdata, 0, sizeof(ecmdata));
					int ecmlen=0;
					int index = 2;
					while (index<len) {
						//entry = buf[index];	
						//len = buf[index+1];
					    switch (buf[index]) {
							case  2: // CAID (upper byte only, oldstyle)
								caid = buf[index+2]<<8;
								break;
							case 10: // CAID
								caid = (buf[index+2]<<8) | buf[index+3];
								break;
							case  3: // ECM DATA
								ecmlen = buf[index+1];
								memcpy( ecmdata, &buf[index+2], ecmlen);
								break;
							case  6: // PROVID (ASCII)
								for (i=0; i<buf[index+1]; i++) provid = (provid<<4) | hexvalue(buf[index+2+i]);
								//provid = hex2int(char *src); // 6
								break;
							case  7: // KEYNR (ASCII), not needed
								break;
							case  8: // ECM PROCESS PID ?? don't know, not needed
								break;
						}
						index += buf[index+1]+2;
					}
					if (!accept_ecm(cs,caid,provid,0)) {
						rdgd_senddcw_cli(cli);
						break;
					}
					// ACCEPTED
					// Search for ECM
					int ecmid = search_ecmdata_dcw( ecmdata, ecmlen, 0); // dont get failed ecm request from cache
					if ( ecmid!=-1 ) {
						ECM_DATA *ecm=getecmbyid(ecmid);
						ecm->lastrecvtime = GetTickCount();
						//TODO: Add another card for sending ecm
						rdgd_store_ecmclient(cs, ecmid, cli);
						debugf(" <- ecm from client (%s) ch %04x:%06x:%04x*\n", ip2string(cli->ip), caid, provid, 0);
						cli->ecm.busy=1;
						cli->ecm.hash = ecm->hash;
					}
					else {
						cs->ecmaccepted++;
						// Setup ECM Request for Server(s)
						ecmid = store_ecmdata(cs->id, ecmdata, ecmlen, 0, caid, provid);
						ECM_DATA *ecm=getecmbyid(ecmid);
						rdgd_store_ecmclient(cs, ecmid, cli);
						debugf(" <- ecm from client (%s) ch %04x:%06x:%04x\n", ip2string(cli->ip), caid, provid, 0);
						cli->ecm.busy=1;
						cli->ecm.hash = ecm->hash;
						if (cs->usecache && cfg.cachepeer) {
							pipe_send_cache_find(ecm, cs);
							ecm->waitcache = 1;
							ecm->dcwstatus = STAT_DCW_WAITCACHE;
						} else ecm->dcwstatus = STAT_DCW_WAIT;
					}
					ecm_check_time = rdgd_dcw_check_time = 0;
					break;
				default:
					buf[0] = 0x81;
					buf[1] = 0;
					rdgd_message_send(cli->handle,buf,2);
					//radegast_send(answer);
					//cs_log("unknown request %02X, len=%d", buf[0], buf[1]);
			}
		}
	}
}


// Check sending cw to clients
uint rdgd_check_sendcw()
{
	struct cardserver_data *cs = cfg.cardserver;
	uint restime = GetTickCount() + 10000;
	uint clitime = 0;

	while(cs) {
		if (cs->rdgd.handle>0) {
			struct rdgd_client_data *cli = cs->rdgd.client;
			uint ticks = GetTickCount();
			while (cli) {
				if ( (cli->handle!=INVALID_SOCKET)&&(cli->ecm.busy) ) {
					clitime = ticks+11000;
					// Check for DCW ANSWER
					ECM_DATA *ecm = getecmbyid(cli->ecm.id);
					if (ecm) {
						// Check for FAILED
						if (ecm->dcwstatus==STAT_DCW_FAILED) rdgd_senddcw_cli( cli );
						// Check for SUCCESS
						else if (ecm->dcwstatus==STAT_DCW_SUCCESS) rdgd_senddcw_cli( cli );
						// check for timeout
						else if ( (cli->ecm.recvtime+cs->dcwtimeout) < ticks ) rdgd_senddcw_cli( cli ); else clitime = cli->ecm.recvtime+cs->dcwtimeout;
					}
					else rdgd_senddcw_cli( cli ); // failed
					if (restime>clitime) restime = clitime;
				}
				cli = cli->next;
			}
		}
		cs = cs->next;
	}
	return (restime+1);
}

