////
// File: th-ecm.c
/////


#define MAXSRVTAB 255

struct srvtab_data
{
	struct cs_server_data *srv;
	struct cs_card_data *card; // selected card
	int uphops;
	int val; // sid value
	unsigned int ecmtime;
	uint32 ecmperhr;
};


struct srvtab_data srvtab[MAXSRVTAB];
struct srvtab_data srvtemp;


int srvtab_arrange(struct cardserver_data *cs, int ecmid, int bestone )
{
	ECM_DATA *ecm = getecmbyid(ecmid);

	int i,j;
	int nbsrv = 0;
	struct cs_server_data *srv;

	memset( srvtab, 0 , sizeof(srvtab) );
	nbsrv = 0;

	// MULTICARD Servers Selection (Newcamd,CCcam,Mgcamd...) ;)
	unsigned int ticks = GetTickCount();
	srv = cfg.server;
	while (srv) {
		if (
			( cs->fallownewcamd&&(srv->type==TYPE_NEWCAMD) )
			|| ( cs->fallowcccam&&(srv->type==TYPE_CCCAM) )
			|| ( cs->fallowradegast&&(srv->type==TYPE_RADEGAST) )
		)
		if (srv->handle>0)
		//if ( !srv->cs || (srv->cs==cs) )
		{
			// Check for CS PORTS
			for(i=0; i<MAX_CSPORTS; i++ ) {
				if (!srv->csport[i]) break;
				if (srv->csport[i]==cs->port) {
					i=0;
					break;
				}
			}
			if (i==0) { // ADD TO PROFILE
				//Check for used servers, dont reuse
				for (i=0; i<20;i++) {
					if (!ecm->server[i].srvid) break;
					if (ecm->server[i].srvid==srv->id) break;
				}
				if ( (i>=20)||(ecm->server[i].srvid!=srv->id) ) {
					// Check for ECM TIMEOUT
					if ( (srv->busy)&&((srv->lastecmtime+10000)<ticks) ) { // timeout
						debugf(" ??? server (%s:%d) doesnt send ecm answer\n", srv->host->name,srv->port);
						srv->ecmtimeout++;
						srv->busy = 0;
						if (srv->ecmtimeout%5==4) if (srv->type==TYPE_CCCAM) cc_disconnect_srv(srv);
					}
					// check for any card to decode
					struct cs_card_data *pcard = srv_findcard( srv, cs, ecm->caid, ecm->provid ); // check if isthere any available card
					if ( pcard ) {
						srvtab[nbsrv].srv = srv;
						srvtab[nbsrv].card = pcard; // default card
						srvtab[nbsrv].uphops = pcard->uphops;
						srvtab[nbsrv].val = 0;
						nbsrv++;
						if (nbsrv>=MAXSRVTAB) break;
					}
				}
			}
		}
		srv = srv->next;
	}



// ARRANGE BY SID/TIME FILTER

	//Check FAILED SIDS
	struct cs_card_data *card=NULL;
	i=0;
	for(j=0; j<nbsrv; j++) {
		// best card to decode is selected, it may there is only worst one but is returned
		srvtab[j].val = sidata_getval( srvtab[j].srv, cs, ecm->caid, ecm->provid, ecm->sid, &card);
//		if ( srvtab[j].val > -100 )
		if ( !cs->maxfailedecm || (srvtab[j].val > -cs->maxfailedecm) ) {  // available card+sid : block card that have decode failed on sid
			if (card) {
				srvtab[j].card = card;
				srvtab[j].uphops = card->uphops;
				if (srvtab[j].srv->type==TYPE_CCCAM)
					if (card->ecmok>10) srvtab[j].ecmtime = card->ecmoktime/card->ecmok; else srvtab[j].ecmtime = 0;
				else
					if (srvtab[j].srv->ecmok>10) srvtab[j].ecmtime = srvtab[j].srv->ecmoktime/srvtab[j].srv->ecmok; else srvtab[j].ecmtime = 0;

				if ( !cs->csvalidecmtime || (srvtab[j].ecmtime<cs->csvalidecmtime) ) {
					if (srvtab[j].srv->type==TYPE_CCCAM) {
						if (card) {
							if (i<j) memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
							i++;
						}
					}
					else {
						if (i<j) memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
						i++;
					}
				}
			}
		}
	}
	srvtab[i].srv = NULL;
	nbsrv = i;

	//Remove Cardservers with delay time
	if (cs->cstimeperecm) {
		ticks = GetTickCount();
		i=0;
		for(j=0; j<nbsrv; j++) {
			//if ( (srvtab[j].srv->host->ip!=0x0100007F)&&(srvtab[j].srv->type==TYPE_NEWCAMD) )
			if ( (srvtab[j].srv->type==TYPE_NEWCAMD) ) {
				unsigned int msperecm = ( (ticks-srvtab[j].srv->connected) + srvtab[j].srv->uptime ) / (srvtab[j].srv->ecmnb+1);
				unsigned int tim;
				if ( msperecm > (2*cs->cstimeperecm) ) tim = 0;
				else if ( msperecm > cs->cstimeperecm ) tim = (2*cs->cstimeperecm)-msperecm;
				else tim = cs->cstimeperecm;
				if ( (srvtab[j].srv->lastecmtime+tim)<=ticks ) {
					if (i<j) memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
					i++;
				}
			}
			else {
				if (i<j) memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
				i++;
			}
		}
		srvtab[i].srv = NULL;
		nbsrv = i;
	}


	// Remove Circular request: check for client ip & srv ip
	i=0;
	for(j=0; j<nbsrv; j++) {
		if (srvtab[j].srv->host->ip!=0x0100007F) {  // NON LOCAL

			// Newcamd
			struct cs_client_data *cscli = cs->client;
			while (cscli) {
				if ( (cscli->handle!=INVALID_SOCKET)&&(cscli->ip==srvtab[j].srv->host->ip)&&(cscli->ecm.busy)&&(cscli->ecm.id==ecmid) ) break; // DELETE SERVER
				cscli = cscli->next;
			}
			if (cscli) continue;
			// CCcam
#ifdef CCCAM_SRV
			struct cc_client_data *cccli = cfg.cccam.client;
			while (cccli) {
				if ( (cccli->handle!=INVALID_SOCKET)&&(cccli->ip==srvtab[j].srv->host->ip)&&(cccli->ecm.busy)&&(cccli->ecm.id==ecmid) ) break; // DELETE SERVER
				cccli = cccli->next;
			}
			if (cccli) continue;
#endif
			// Mgcamd
#ifdef MGCAMD_SRV
/*
			struct mg_client_data *mgcli = cfg.mgcamd.client;
			while (mgcli) {
				if ( (mgcli->handle!=INVALID_SOCKET)&&(mgcli->ip==srvtab[j].srv->host->ip)&&(mgcli->ecm.busy)&&(mgcli->ecm.id==ecmid) ) break; // DELETE SERVER
				mgcli = mgcli->next;
			}
			if (mgcli) continue;
*/
#endif
			// ADD SERVER
			if (i<j) memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
			i++;
		}
		else {
			if (i<j) memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
			i++;
		}
	}
	srvtab[i].srv = NULL;
	nbsrv = i;


	//debugf(" srvtab_arrange(%04x:%06x:%04x) %d\n", ecm->caid, ecm->provid, ecm->sid, nbsrv);

	if (!nbsrv) return -1;

	// Remove Busy Servers
	i=0;
	for(j=0; j<nbsrv; j++) {
		if (!srvtab[j].srv->busy) {
			if (i<j) memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
			i++;
		}
	}
	srvtab[i].srv = NULL;
	nbsrv = i;

	// Arrange by ECM LAST SENT TIME
	for(i=0; i<nbsrv-1; i++)
		for(j=i+1; j<nbsrv; j++)
			if ( srvtab[i].srv->lastecmtime > srvtab[j].srv->lastecmtime ) {
				memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
				memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
				memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
			}

	ticks=GetTickCount();
	for(i=0; i<nbsrv; i++) srvtab[i].ecmperhr = (srvtab[i].srv->ecmnb*3600*1000) / ((ticks-srvtab[i].srv->connected)+srvtab[i].srv->uptime);

	if (!bestone)

		// Arrange by ECM LAST SENT TIME && unbusy state & sid ok
		for(i=0; i<nbsrv-1; i++)
		 for(j=i+1; j<nbsrv; j++)

			if (srvtab[i].srv->busy == srvtab[j].srv->busy ) {
				if ( (srvtab[j].val>=0)&&(srvtab[j].srv->host->ip==0x0100007F)&&(srvtab[j].srv->type==TYPE_NEWCAMD) ) { // check if using local card
					memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
					memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
					memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
				}
				else
				if (srvtab[i].val>=0) {
					if (srvtab[j].val>=0) {
						// Check for card uphops
						if (srvtab[i].uphops > srvtab[j].uphops) {
							memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
							memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
							memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
						}
						else if  ( srvtab[i].ecmperhr > srvtab[j].ecmperhr ) {
							memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
							memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
							memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
						}
					}
				}
				else if (srvtab[i].val==-1) {
					if (srvtab[j].val>=0) {
						memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
						memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
						memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
					}
					else if (srvtab[j].val==-1) {
						if  ( srvtab[i].ecmperhr > srvtab[j].ecmperhr ) {
							memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
							memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
							memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
						}
					}
				}
				else {
					if (srvtab[j].val>=-1) {
						memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
						memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
						memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
					}
					else {
						if  ( srvtab[i].ecmperhr > srvtab[j].ecmperhr ) {
							memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
							memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
							memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
						}
					}
				}
			}
			else {
				if ( srvtab[i].srv->busy ) {
					memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
					memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
					memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
				}
			}

	else

		for(i=0; i<nbsrv-1; i++)
		 for(j=i+1; j<nbsrv; j++)

			if (srvtab[i].srv->busy == srvtab[j].srv->busy ) {

				if (srvtab[i].val>0) {
					if (srvtab[j].val>0) {
						if  ( ( srvtab[i].ecmperhr > srvtab[j].ecmperhr ) ) {
							memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
							memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
							memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
						}
					}
				}
				else if (srvtab[i].val==0) {
					if (srvtab[j].val>0) {
						memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
						memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
						memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
					}
					else if (srvtab[j].val==0) {
						if  ( ( srvtab[i].ecmperhr > srvtab[j].ecmperhr ) ) {
							memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
							memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
							memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
						}
					}
				}
				else {
					if (srvtab[j].val>=0) {
						memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
						memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
						memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
					}
					else {
						if  ( srvtab[i].val < srvtab[j].val ) {
							memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
							memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
							memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
						}
					}
				}
			}
			else {
				if ( srvtab[i].srv->busy ) {
					memcpy( &srvtemp, &srvtab[i], sizeof(struct srvtab_data));
					memcpy( &srvtab[i], &srvtab[j], sizeof(struct srvtab_data));
					memcpy( &srvtab[j], &srvtemp, sizeof(struct srvtab_data));
				}
			}

	return nbsrv;

}


///////////////////////////////////////////////////////////////////////////////
// Check sending ecm to servers
///////////////////////////////////////////////////////////////////////////////

uint32 check_sendecm()
{

// Send priority
// 1.Send to all ecm's that wasnt send yet
// 2.Send to all ecm's that having send only once
// 3.Send to all ecm's that having send only twice
//XXX ...

	uint restime = GetTickCount() + 10000;
	uint ecmtime = restime;
	struct cs_server_data *newsrv;

	int lastecmid = nextecmid(srvmsgid);
	int ecmid = lastecmid;

	while ( (ecmid=prevecmid(ecmid)) != lastecmid ) {

		ECM_DATA *ecm =  getecmbyid(ecmid);
		uint32 ticks = GetTickCount();

		ecm->checktime = 0;

		// CACHE(fallowcache = 1)
		if (ecm->dcwstatus==STAT_DCW_WAITCACHE) {
			struct cardserver_data *cs = getcsbyid( ecm->csid );
			if (!cs) {
				static char msg[] = "Invalid profile id";
				ecm->statusmsg = msg;
				ecm->dcwstatus = STAT_DCW_FAILED;
				continue;
			}
			if (!ecm->waitcache) { // Not done
				//debugf(" send pipe cache\n");
				if (pipe_send_cache_find( ecm, cs)) {
					ecm->waitcache = 1;
					ecmtime = ecm->recvtime+cs->cachetimeout;
					ecm->checktime = ecm->recvtime+cs->cachetimeout;
				}
				else ecm->dcwstatus = STAT_DCW_WAIT;
			}
			else {
				if ( ( (ecm->recvtime+cs->cachetimeout)<=ticks ) ) ecm->dcwstatus = STAT_DCW_WAIT;
				else {
					//debugf(" Wait for cache\n");
					ecmtime = ecm->recvtime+cs->cachetimeout;
					ecm->checktime = ecm->recvtime+cs->cachetimeout;
				}
			}

			if (restime>ecmtime) restime = ecmtime;
		}


		// SEND ECM

		if ( (ecm->dcwstatus==STAT_DCW_WAIT) ) {
			// Check Profile
			struct cardserver_data *cs = getcsbyid( ecm->csid );
			if (!cs) {
				static char msg[] = "Invalid profile id";
				ecm->statusmsg = msg;
				ecm->dcwstatus = STAT_DCW_FAILED;
				cs_dcw_check_time=0;
#ifdef MGCAMD_SRV
				mg_dcw_check_time = 0;
#endif
#ifdef CCCAM_SRV
				cc_dcw_check_time = 0;
#endif
#ifdef FREECCCAM_SRV
				frcc_dcw_check_time = 0;
#endif
#ifdef RADEGAST_SRV
				rdgd_dcw_check_time = 0;
#endif
				continue;
			}
			//check for decode failed
			int nbservers = ecm_nbsentsrv(ecm);
			int waitsrv = ecm_nbwaitsrv(ecm);
			// Check for Max used Servers
			if ( (cs->csmax>0) && (nbservers>=cs->csmax) ) {
				if (!waitsrv) {
					static char msg[] = "Decode failed, max servers is reached and no more servers to wait";
					ecm->statusmsg = msg;
					ecm->dcwstatus = STAT_DCW_FAILED;
					cs_dcw_check_time=0;
#ifdef MGCAMD_SRV
					mg_dcw_check_time = 0;
#endif
#ifdef CCCAM_SRV
					cc_dcw_check_time = 0;
#endif
#ifdef FREECCCAM_SRV
					frcc_dcw_check_time = 0;
#endif
#ifdef RADEGAST_SRV
					rdgd_dcw_check_time = 0;
#endif
					// Send DCW to Cache if not sent
					if ( cs->usecache && (ecm->cachestatus==ECM_CACHE_REQ) ) {
						//pipe_send_cache_reply(ecm,cs); // Send failed Cache Reply
						ecm->cachestatus = ECM_CACHE_REP;
					}
				}
			}
			// Check for ECM TimeOut
			else if ( (ticks-ecm->recvtime)>=(2*cs->dcwtimeout) ) {
				if (!waitsrv) {
					static char msg[] = "Decode failed";
					ecm->statusmsg = msg;
					ecm->dcwstatus = STAT_DCW_FAILED;
					cs_dcw_check_time=0;
#ifdef MGCAMD_SRV
					mg_dcw_check_time = 0;
#endif
#ifdef CCCAM_SRV
					cc_dcw_check_time = 0;
#endif
#ifdef FREECCCAM_SRV
					frcc_dcw_check_time = 0;
#endif
#ifdef RADEGAST_SRV
					rdgd_dcw_check_time = 0;
#endif
					// Send DCW to Cache if not sent
					if ( cs->usecache && (ecm->cachestatus==ECM_CACHE_REQ) ) {
						//pipe_send_cache_reply(ecm,cs); // Send failed Cache Reply
						ecm->cachestatus = ECM_CACHE_REP;
					}
				}
			}
			// Check for ECM sending
			else if ((ticks-ecm->lastrecvtime)<=cs->cstimeout) { // ~ cfg.cardserver.dcwtimeout*2/3 is the cardserver timeout
				// check for decode failed with no remaining server to wait, send to new cardserver
				if ( !waitsrv
					|| ((ticks-ecm->lastsendtime)>=cs->csinterval)
				) {
					//debugf(" check_sendecm(): ECM -> CardServer '%s' ch %04x:%06x:%04x\n", cs->name,ecm->caid,ecm->provid,ecm->sid);
					//newsrv = NULL;
					if ( srvtab_arrange(cs, ecmid, nbservers > 0 )==-1 ) { // No more servers to decode
						//debugf(" sendecm: no servers found to decode\n");
						if (!waitsrv) {
							static char msg[] = "No servers found to decode";
							ecm->statusmsg = msg;
							ecm->dcwstatus = STAT_DCW_FAILED;
							cs_dcw_check_time=0;
#ifdef MGCAMD_SRV
							mg_dcw_check_time = 0;
#endif
#ifdef CCCAM_SRV
							cc_dcw_check_time = 0;
#endif
#ifdef FREECCCAM_SRV
							frcc_dcw_check_time = 0;
#endif
#ifdef RADEGAST_SRV
							rdgd_dcw_check_time = 0;
#endif
							// Send DCW to Cache if not sent
							if ( cs->usecache && (ecm->cachestatus==ECM_CACHE_REQ) ) {
								//pipe_send_cache_reply(ecm,cs); //Send Failed Cache Reply
								ecm->cachestatus=ECM_CACHE_REP;
							}
						}
						else {
							ecm->checktime = ecm->lastrecvtime+cs->cstimeout;
							ecmtime = ecm->lastrecvtime+cs->cstimeout;
							if (restime>ecmtime) restime = ecmtime;
						}
						continue;
					}
					else if (srvtab[0].srv) {
						newsrv = srvtab[0].srv;
						newsrv->busycard = srvtab[0].card;
						newsrv->busycardid = srvtab[0].card->shareid;
					}
					else {
						static char msg[] = "Wait for available servers";
						ecm->statusmsg = msg;
						continue;
					}
					// SEND ECM
					if ( newsrv && !newsrv->busy ) {// time to send ecm

						if ((ticks-ecm->lastrecvtime+cs->csinterval)<cs->cstimeout) {
							ecm->checktime = ecm->lastsendtime+cs->csinterval;
							ecmtime = ecm->lastsendtime+cs->csinterval;
						}
						else {
							ecm->checktime = ecm->lastrecvtime+cs->dcwtimeout;
							ecmtime = ecm->lastrecvtime+cs->dcwtimeout;
						}

						//debugf(" sendecm: selected server to decode (%s:%d)\n", newsrv->host->name, newsrv->port);
						if (newsrv->type==TYPE_NEWCAMD) {
							if (cs_sendecm_srv(cs, newsrv, ecm)>0) {
								ecm->lastsendtime = GetTickCount();
								debugf(" -> ecm to server%d (%s:%d) ch %04x:%06x:%04x\n", nbservers, newsrv->host->name, newsrv->port, ecm->caid, ecm->provid, ecm->sid);
								newsrv->lastecmtime = GetTickCount();
								newsrv->ecmnb++;
								newsrv->busy=1;
								newsrv->busyecmid = ecmid;
								newsrv->busyecmhash = ecm->hash;
								newsrv->retry=0;
								ecm_addsrv(ecm, newsrv->id);
								if ( cs->usecache && !nbservers && (ecm->cachestatus==ECM_CACHE_NONE) ) {
									if ( pipe_send_cache_request(ecm,cs) ) ecm->cachestatus = ECM_CACHE_REQ; else ecm->cachestatus = ECM_CACHE_REP;
								}
							}
						}
#ifdef CCCAM_CLI
						else if (newsrv->type==TYPE_CCCAM) {
							if (cc_sendecm_srv(newsrv, ecm)>0) {
								ecm->lastsendtime = GetTickCount();
								debugf(" -> ecm to CCcam server%d (%s:%d) ch %04x:%06x:%04x\n", nbservers, newsrv->host->name, newsrv->port, ecm->caid, ecm->provid, ecm->sid);
								newsrv->lastecmtime = GetTickCount();
								newsrv->ecmnb++;
								struct cs_card_data *card = cc_getcardbyid( newsrv, newsrv->busycardid );
								if (card) card->ecmnb++;
								newsrv->busy=1;
								newsrv->busyecmid = ecmid;
								newsrv->busyecmhash = ecm->hash;
								newsrv->retry=0;
								ecm_addsrv(ecm, newsrv->id);
								if ( cs->usecache && !nbservers && (ecm->cachestatus==ECM_CACHE_NONE) ) {
									if ( pipe_send_cache_request(ecm,cs) ) ecm->cachestatus = ECM_CACHE_REQ; else ecm->cachestatus = ECM_CACHE_REP;
								}
							}
						}
#endif

#ifdef RADEGAST_CLI
						else if (newsrv->type==TYPE_RADEGAST) {
							if (rdgd_sendecm_srv(newsrv, ecm)>0) {
								ecm->lastsendtime = GetTickCount();
								debugf(" -> ecm to Radegast server%d (%s:%d) ch %04x:%06x:%04x\n", nbservers, newsrv->host->name, newsrv->port, ecm->caid, ecm->provid, ecm->sid);
								newsrv->lastecmtime = GetTickCount();
								newsrv->ecmnb++;
								newsrv->busy=1;
								newsrv->busyecmid = ecmid;
								newsrv->busyecmhash = ecm->hash;
								newsrv->retry=0;
								ecm_addsrv(ecm, newsrv->id);
								if ( cs->usecache && !nbservers && (ecm->cachestatus==ECM_CACHE_NONE) ) {
									if ( pipe_send_cache_request(ecm,cs) ) ecm->cachestatus = ECM_CACHE_REQ; else ecm->cachestatus = ECM_CACHE_REP;
								}
							}
						}
#endif
						if (restime>ecmtime) restime = ecmtime;
					}
				}
			}
			else if (!waitsrv) {
				static char msg[] = "no more time to send request";
				ecm->statusmsg = msg;
				ecm->dcwstatus = STAT_DCW_FAILED;
				cs_dcw_check_time=0;
#ifdef MGCAMD_SRV
				mg_dcw_check_time = 0;
#endif
#ifdef CCCAM_SRV
				cc_dcw_check_time = 0;
#endif
#ifdef FREECCCAM_SRV
				frcc_dcw_check_time = 0;
#endif
#ifdef RADEGAST_SRV
				rdgd_dcw_check_time = 0;
#endif
				// Send DCW to Cache if not sent
				if ( cs->usecache && (ecm->cachestatus==ECM_CACHE_REQ) ) {
					//pipe_send_cache_reply(ecm,cs); //Send Failed Cache Reply
					ecm->cachestatus = ECM_CACHE_REP;
				}
			}
		}
	}

	return (restime+1);
}


///////////////////////////////////////////////////////////////////////////////
// SEND ECM THREAD
///////////////////////////////////////////////////////////////////////////////

void recv_ecm_pipe()
{
	uint16 sid;
	uint16 caid;
	uint32 hash;
	uchar buf[1024];
	int ecmid;
	struct pollfd pfd[2];

 do {
	int len = pipe_recv( srvsocks[0], buf);
	if (len>0) {
		switch(buf[0]) {

			case PIPE_LOCK:
				pthread_mutex_lock(&prg.lockmain);
				pthread_mutex_unlock(&prg.lockmain);
				break;

			case PIPE_CACHE_FIND_FAILED:
				//debugf(" ecm check PIPE_CACHE_FIND_FAILED\n");
				//Search for ecm
				sid = (buf[3]<<8) | buf[4];
				caid = (buf[7]<<8) | buf[8];
				hash = (buf[9]<<24) | (buf[10]<<16) | (buf[11]<<8) | (buf[12]);
				ecmid = search_ecmdata_byhash( hash );
				if (ecmid!=-1) {
					ECM_DATA *ecm = getecmbyid(ecmid);
					if ( (ecm->caid==caid)&&(ecm->hash==hash)&&(ecm->sid==sid)&&(ecm->dcwstatus==STAT_DCW_WAITCACHE) ) {
						ecm->dcwstatus = STAT_DCW_WAIT;
					}
				} // else debugf(" pipe: ecm not found (%08X)\n",hash);
				break;

			case PIPE_CACHE_FIND_SUCCESS:  // SET DCW
				//Search for ecm
				sid = (buf[3]<<8) | buf[4];
				caid = (buf[7]<<8) | buf[8];
				hash = (buf[9]<<24) | (buf[10]<<16) | (buf[11]<<8) | (buf[12]);
				int peerid = (buf[13]<<8) | buf[14];
				//debugf(" ecm check PIPE_CACHE_FIND_SUCCESS %04x:%04x:%08x\n",caid, sid, hash); debughex(buf,len);
				int ecmid = search_ecmdata_byhash( hash );
				if (ecmid!=-1) {
					ECM_DATA *ecm = getecmbyid(ecmid);
					if ( (ecm->caid==caid)&&(ecm->hash==hash)&&(ecm->sid==sid)&&(ecm->dcwstatus!= STAT_DCW_SUCCESS) ) {
						struct cardserver_data *cs = getcsbyid(ecm->csid);
						struct cs_cachepeer_data *peer = getpeerbyid(peerid);
						if (cs&&peer) ecm_setdcw( cs, ecm, buf+15, DCW_SOURCE_CACHE, peer->id);
					}
				} // else debugf(" pipe: ecm not found (%08X)\n",hash);
				break;
		}
	}


	pfd[0].fd = srvsocks[0];
	pfd[0].events = POLLIN | POLLPRI;
 } while (poll(pfd, 1, 0)>0);
}


char *src2string(int srctype, int srcid, char *prestr, char *ret)
{
	static char ss1[] = "server";
	static char ss2[] = "cache peer";
	static char ss3[] = "newcamd client";
	
	if (srctype==DCW_SOURCE_SERVER) {
		struct cs_server_data *srv = getsrvbyid(srcid);
		if (srv)
			sprintf( ret,"%s From server (%s:%d)", prestr, srv->host->name, srv->port);
		else
			sprintf( ret,"%s From Unknow server (id=%d)", prestr, srcid);
		return ss1;
	}
	else if (srctype==DCW_SOURCE_CACHE) {
		struct cs_cachepeer_data *peer = getpeerbyid(srcid);
		if (peer)
			sprintf( ret,"%s From cache peer (%s:%d)", prestr, peer->host->name, peer->port);
		else
			sprintf( ret,"%s From Unknown cache peer (id=%d)", prestr, srcid);
		return ss2;
	}

	else if (srctype==DCW_SOURCE_CSCLIENT) {
		// srcid =  (csid<<16)|cliid;
		struct cardserver_data *cs = getcsbyid( srcid>>16 );
		if (cs) {
			struct cs_client_data *cli = getnewcamdclientbyid( cs, srcid&0xffff );
			if (cli) {
				sprintf( ret,"%s From newcamd client '%s'", prestr, cli->user);
				return ss3;
			}
		}
		sprintf( ret,"%s From Unknown newcamd client (id=%x)", prestr, srcid);
		return ss3;
	}
	else if (srctype==DCW_SOURCE_MGCLIENT) {
		// srcid =  (csid<<16)|cliid;
		struct mg_client_data *cli = getmgcamdclientbyid( srcid );
		if (cli)
			sprintf( ret,"%s From mgcamd client '%s'", prestr, cli->user);
		else
			sprintf( ret,"%s From Unknown mgcamd client (id=%d)", prestr, srcid);
		return ss3;
	}

	else {
		sprintf( ret,"%s From Unknown Source (%d/%d)", prestr, srctype, srcid);
	}
	return NULL;
}



void ecm_setdcw( struct cardserver_data *cs, ECM_DATA *ecm, uchar dcw[16], int srctype, int srcid)
{
	pthread_mutex_lock(&prg.lockdcw);

	if (ecm->dcwstatus!=STAT_DCW_SUCCESS) {
		// Search for SAME DCW
		int ecmid = search_ecmdata_bycw( dcw, ecm->hash, ecm->sid, ecm->caid, ecm->provid);
		if (ecmid==-1) {

#ifdef CHECK_NEXTDCW
			if (cs && cs->checkdcw)
			if ( !checkfreeze_setdcw(ecm->srvmsgid,dcw) ) {
				pthread_mutex_unlock(&prg.lockdcw);
				return;
			}
#endif

			int instant = (ecm->dcwstatus==STAT_DCW_WAITCACHE);
			ecm->dcwsrctype = srctype;
			ecm->dcwsrcid = srcid;
			ecm->dcwstatus = STAT_DCW_SUCCESS;
			memcpy( ecm->cw, dcw, 16 );

			if (cs) {
				uint ecmtime = GetTickCount()-ecm->recvtime;
				if (ecmtime<cs->dcwtimeout) {
					cs->ecmok++;
					cs->ecmoktime += ecmtime;
					int time = (ecmtime+50)/100;
					if (time<100) cs->ttime[time]++; else cs->ttime[100]++;

					if (srctype==DCW_SOURCE_CACHE) {
						struct cs_cachepeer_data *peer = getpeerbyid(srcid);
						if (peer) {
							// setup peer last used cache
							peer->lastcaid = ecm->caid;
							peer->lastprov = ecm->provid;
							peer->lastsid = ecm->sid;
							peer->lastdecodetime = ecmtime;

							peer->hitnb++;
							cs->cachehits++;
							cfg.cachehits++;
							if (instant) {
								peer->ihitnb++;
								cs->cacheihits++;
								cfg.cacheihits++;
							}
						}
						if (time<100) cs->ttimecache[time]++; else cs->ttimecache[100]++;
					}

					else if (srctype==DCW_SOURCE_SERVER) {
						if (time<100) cs->ttimecards[time]++; else cs->ttimecards[100]++;
					}

					else if (srctype==DCW_SOURCE_CSCLIENT) {
						// srcid =  (csid<<16)|cliid;
						struct cardserver_data *cs = getcsbyid( srcid>>16 );
						if (cs) {
							struct cs_client_data *cli = getnewcamdclientbyid( cs, srcid&0xffff );
							if (cli) cli->cachedcw++;
						}
						//
						if (time<100) cs->ttimeclients[time]++; else cs->ttimeclients[100]++;
					}
					else if (srctype==DCW_SOURCE_MGCLIENT) {
						struct mg_client_data *cli = getmgcamdclientbyid(srcid);
						if (cli) cli->cachedcw++;
						if (time<100) cs->ttimeclients[time]++; else cs->ttimeclients[100]++;
					}

					// Send DCW to Cache if not sent
					if ( cs->usecache && (ecm->cachestatus==ECM_CACHE_REQ) ) {
						pipe_send_cache_reply(ecm,cs); //Send Good Cache Reply
						ecm->cachestatus = ECM_CACHE_REP;
					}
				}
			}
			// Send DCW to clients
			cs_dcw_check_time=0;
#ifdef MGCAMD_SRV
			mg_dcw_check_time = 0;
#endif
#ifdef CCCAM_SRV
			cc_dcw_check_time = 0;
#endif
#ifdef FREECCCAM_SRV
			frcc_dcw_check_time = 0;
#endif
#ifdef RADEGAST_SRV
			rdgd_dcw_check_time = 0;
#endif

			// Send to Newcamd Cached Servers
			int i;
			for( i=0; i<20; i++ ) {
				if (!ecm->server[i].srvid) break;
				if (ecm->server[i].flag==ECM_SRV_REQUEST) {
					struct cs_server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (!srv) continue;
					if (!srv->busy) continue;
					if ( (srv->type==TYPE_NEWCAMD)&&(srv->cscached) ) { // Send DCW to server
						struct cs_custom_data srvcd;
						unsigned char buf[32];
						srvcd.msgid = srv->busyecmid;
						srvcd.caid = ecm->caid;
						srvcd.sid = ecm->sid;
						srvcd.provid = ecm->provid;
						buf[0] = ecm->ecm[0] | 0x40; // 0xC0 | 0xC1
						buf[2] = 0x10;
						memcpy(&buf[3], &ecm->cw,16);
						cs_message_send( srv->handle, &srvcd, buf, 19, srv->sessionkey);
					}
				}
			}
		}
	}

	pthread_mutex_unlock(&prg.lockdcw);
}



#define TYPE_SERVER            1
#define TYPE_CLIENT_NEWCAMD    2
#define TYPE_CLIENT_CCCAM      3
#define TYPE_CLIENT_MGCAMD     4


struct recvmsg_data {
	int type;
	union {
		struct cs_server_data *srv;
		struct cc_client_data *cccli;
		struct mg_client_data *mgcli;
		struct cs_client_data *cscli;
	};
	struct cardserver_data *cs;
	pthread_t lastthreadid;
};


void *recv_msg (struct recvmsg_data *data)
{
	if (data->type==TYPE_SERVER) {
		if (data->srv->type==TYPE_NEWCAMD) cs_srv_recvmsg(data->srv);
#ifdef CCCAM_CLI
		else if (data->srv->type==TYPE_CCCAM) cc_srv_recvmsg(data->srv);
#endif
#ifdef RADEGAST_CLI
		else if (data->srv->type==TYPE_RADEGAST) rdgd_srv_recvmsg(data->srv);
#endif
	}
	else if (data->type==TYPE_CLIENT_CCCAM) {
		cc_cli_recvmsg(data->cccli);
	}
	else if (data->type==TYPE_CLIENT_MGCAMD) {
		mg_cli_recvmsg(data->mgcli);
	}
	else if (data->type==TYPE_CLIENT_NEWCAMD) {
		cs_cli_recvmsg(data->cscli,data->cs);
	}

	if ( !pthread_equal(data->lastthreadid, prg.tid_msg) ) {
		pthread_join(data->lastthreadid, NULL);
	}

	free( data );
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// RECEIVE MESSAGES THREAD
///////////////////////////////////////////////////////////////////////////////

struct pollfd pfd[4096];

int pfdcount = 0;

void *recv_msg_thread(void *param)
{

	prg.pid_msg = syscall(SYS_gettid);

	prg.tid_msg = pthread_self();

	uint32 katicks = GetTickCount()+ 20000; // KeepAlive for servers

	while(1) {

		pthread_t lastthreadid = pthread_self();

		// getmintime
		uint32 mintime = cs_dcw_check_time;
#ifdef MGCAMD_SRV
		if (mintime>mg_dcw_check_time) mintime=mg_dcw_check_time;
#endif
#ifdef CCCAM_SRV
		if (mintime>cc_dcw_check_time) mintime=cc_dcw_check_time;
#endif
#ifdef FREECCCAM_SRV
		if (mintime>frcc_dcw_check_time) mintime=frcc_dcw_check_time;
#endif
#ifdef RADEGAST_SRV
		if (mintime>rdgd_dcw_check_time) mintime=rdgd_dcw_check_time;
#endif
		if (mintime>ecm_check_time) mintime=ecm_check_time;

		uint ticks = GetTickCount();
		uint ms;
		if (mintime>(ticks+10)) ms = mintime-ticks; else ms = 10;
		//debugf(" poll timeout = %dms\n",ms);
		pfdcount = 0;

		// PIPE
		pfd[pfdcount].fd = srvsocks[0];
		pfd[pfdcount++].events = POLLIN | POLLPRI;

		//Servers
		struct cs_server_data *srv = cfg.server;
		while (srv) {
			if (srv->handle>0) {
				srv->ipoll = pfdcount;
				pfd[pfdcount].fd = srv->handle;
				pfd[pfdcount++].events = POLLIN | POLLPRI;
			} else srv->ipoll = -1;
			srv = srv->next;
		}
		// Check KA for newcamd Servers
		if (katicks<ticks) {
			struct cs_server_data *srv = cfg.server;
			while (srv) {
				if ( (srv->handle>0)&&(srv->type==TYPE_NEWCAMD) ) cs_check_keepalive(srv);
				srv = srv->next;
			}
			katicks = ticks + 10000;
		}



#ifdef CCCAM_SRV
		if (cfg.cccam.handle>0) {
			struct cc_client_data *cccli = cfg.cccam.client;
			while (cccli) {
				if (cccli->handle>0) {
					cccli->ipoll = pfdcount;
					pfd[pfdcount].fd = cccli->handle;
					pfd[pfdcount++].events = POLLIN | POLLPRI;
				} else cccli->ipoll = -1;
				cccli = cccli->next;
			}
		}
#endif

#ifdef FREECCCAM_SRV
		// srv-cccam
		if (cfg.freecccam.handle>0) {
			struct cc_client_data *fcccli = cfg.freecccam.client;
			while (fcccli) {
				if (fcccli->handle>0) {
					fcccli->ipoll = pfdcount;
					pfd[pfdcount].fd = fcccli->handle;
					pfd[pfdcount++].events = POLLIN | POLLPRI;
				} else fcccli->ipoll = -1;
				fcccli = fcccli->next;
			}
		}
#endif

#ifdef MGCAMD_SRV
		if (cfg.mgcamd.handle>0) {
			struct mg_client_data *mgcli = cfg.mgcamd.client;
			while (mgcli) {
				if (mgcli->handle>0) {
					mgcli->ipoll = pfdcount;
					pfd[pfdcount].fd = mgcli->handle;
					pfd[pfdcount++].events = POLLIN | POLLPRI;
				} else mgcli->ipoll = -1;
				mgcli = mgcli->next;
			}
		}
#endif

		struct cs_client_data *cscli;
		struct cardserver_data *cs = cfg.cardserver;
		while (cs) {
			if (cs->handle>0) {
				cscli = cs->client;
				while (cscli) {
					if (cscli->handle>0) {
						cscli->ipoll = pfdcount;
						pfd[pfdcount].fd = cscli->handle;
						pfd[pfdcount++].events = POLLIN | POLLPRI;
					} else cscli->ipoll = -1;
					cscli = cscli->next;
				}
			}
			cs = cs->next;
		}


#ifdef RADEGAST_SRV
		cs = cfg.cardserver;
		while (cs) {
			if (cs->rdgd.handle>0) {
				pthread_mutex_lock(&prg.lockrdgdcli);
				struct rdgd_client_data *rdgdcli = cs->rdgd.client;
				while (rdgdcli) {
					if (rdgdcli->handle>0) {
						rdgdcli->ipoll = pfdcount;
						pfd[pfdcount].fd = rdgdcli->handle;
						pfd[pfdcount++].events = POLLIN | POLLPRI;
					} else rdgdcli->ipoll = -1;
					rdgdcli = rdgdcli->next;
				}
				pthread_mutex_unlock(&prg.lockrdgdcli);
			}
			cs = cs->next;
		}
#endif

		int retval = poll(pfd, pfdcount, ms);

		if ( retval>0 ) {

			usleep(10000);

			/// SERVERS
			pthread_mutex_lock(&prg.locksrv);
			struct cs_server_data *srv = cfg.server;
			while (srv) {
				if ( (srv->handle>0)&&(srv->ipoll>=0)&&(srv->handle==pfd[srv->ipoll].fd) ) {
					if ( pfd[srv->ipoll].revents & (POLLHUP|POLLNVAL) ) {
						if (srv->type==TYPE_NEWCAMD) cs_disconnect_srv(srv);
#ifdef CCCAM_CLI
						else if (srv->type==TYPE_CCCAM) cc_disconnect_srv(srv);
#endif
#ifdef RADEGAST_CLI
						else if (srv->type==TYPE_RADEGAST) rdgd_disconnect_srv(srv);
#endif
					}
					else if ( pfd[srv->ipoll].revents & (POLLIN|POLLPRI) ) {
						if (srv->type==TYPE_NEWCAMD) cs_srv_recvmsg(srv);
#ifdef CCCAM_CLI
						else if (srv->type==TYPE_CCCAM) cc_srv_recvmsg(srv);
#endif
#ifdef RADEGAST_CLI
						else if (srv->type==TYPE_RADEGAST) rdgd_srv_recvmsg(srv);
#endif
					}
				}
				srv = srv->next;
			}
			pthread_mutex_unlock(&prg.locksrv);

			/// CLIENTS
			// Newcamd Clients
			cs = cfg.cardserver;
			while (cs) {
				if (cs->handle>0) {
					pthread_mutex_lock(&prg.lockcli);
					cscli = cs->client;
					while (cscli) {
						if ( (cscli->handle>0) && (cscli->ipoll>=0) && (cscli->handle==pfd[cscli->ipoll].fd) ) {
							if ( pfd[cscli->ipoll].revents & (POLLHUP|POLLNVAL) ) cs_disconnect_cli(cscli);
							else if ( pfd[cscli->ipoll].revents & (POLLIN|POLLPRI) ) {
								cs_cli_recvmsg(cscli,cs);
							}
						}
						cscli = cscli->next;
					}
					pthread_mutex_unlock(&prg.lockcli);
				}
				cs = cs->next;
			}


#ifdef RADEGAST_SRV
			// Radegast Clients
			cs = cfg.cardserver;
			while (cs) {
				if (cs->rdgd.handle>0) {
					pthread_mutex_lock(&prg.lockrdgdcli);
					struct rdgd_client_data *rdgdcli = cs->rdgd.client;
					while (rdgdcli) {
						if ( (rdgdcli->handle>0)&&(rdgdcli->ipoll>=0)&&(rdgdcli->handle==pfd[rdgdcli->ipoll].fd) ) {
							if ( pfd[rdgdcli->ipoll].revents & (POLLHUP|POLLNVAL) ) rdgd_disconnect_cli(cs,rdgdcli);
							else if ( pfd[rdgdcli->ipoll].revents & (POLLIN|POLLPRI) ) rdgd_cli_recvmsg(rdgdcli,cs);
						}
						rdgdcli = rdgdcli->next;
					}
					pthread_mutex_unlock(&prg.lockrdgdcli);
				}
				cs = cs->next;
			}
#endif


#ifdef MGCAMD_SRV
			if (cfg.mgcamd.handle>0) {
				pthread_mutex_lock(&prg.lockclimg);
				struct mg_client_data *mgcli = cfg.mgcamd.client;
				while (mgcli) {
					if ( (mgcli->handle>0)&&(mgcli->ipoll>=0)&&(mgcli->handle==pfd[mgcli->ipoll].fd) ) {
						if ( pfd[mgcli->ipoll].revents & (POLLHUP|POLLNVAL) ) mg_disconnect_cli(mgcli);
						else if ( pfd[mgcli->ipoll].revents & (POLLIN|POLLPRI) ) {
							mg_cli_recvmsg(mgcli);
						}
					}
					mgcli = mgcli->next;
				}
				pthread_mutex_unlock(&prg.lockclimg);
			}
#endif


#ifdef CCCAM_SRV
			// CCcam Clients
			if (cfg.cccam.handle>0) {
				pthread_mutex_lock(&prg.lockcccli);
				struct cc_client_data *cccli = cfg.cccam.client;
				while (cccli) {
					if ( (cccli->handle>0)&&(cccli->ipoll>=0)&&(cccli->handle==pfd[cccli->ipoll].fd) ) {
						if ( pfd[cccli->ipoll].revents & (POLLHUP|POLLNVAL) ) cc_disconnect_cli(cccli);
						else if ( pfd[cccli->ipoll].revents & (POLLIN|POLLPRI) ) {
							cc_cli_recvmsg(cccli);
						}
					}
					cccli = cccli->next;
				}
				pthread_mutex_unlock(&prg.lockcccli);
			}
#endif

#ifdef FREECCCAM_SRV
			// FreeCCcam Clients
			if (cfg.freecccam.handle>0) {
				pthread_mutex_lock(&prg.lockfreecccli);
				struct cc_client_data *fcccli = cfg.freecccam.client;
				while (fcccli) {
					if ( (fcccli->handle>0)&&(fcccli->ipoll>=0)&&(fcccli->handle==pfd[fcccli->ipoll].fd) ) {
						if ( pfd[fcccli->ipoll].revents & (POLLHUP|POLLNVAL) ) cc_disconnect_cli(fcccli);
						else if ( pfd[fcccli->ipoll].revents & (POLLIN|POLLPRI) ) freecc_cli_recvmsg(fcccli);
					}
					fcccli = fcccli->next;
				}
				pthread_mutex_unlock(&prg.lockfreecccli);
			}
#endif

			//
			if ( pfd[0].revents & (POLLIN|POLLPRI) ) recv_ecm_pipe();
		}
		else if ( retval<0 ) {
			debugf(" thread receive messages: poll error %d(errno=%d)\n", retval, errno);
		}

		if ( !pthread_equal(lastthreadid, prg.tid_msg) ) {
//			debugf(" Wait for thread...\n");
			pthread_join(lastthreadid, NULL);
		}

		/////
		// CHECK SEND ECM/DCW
		/////
		ticks = GetTickCount()+3;
		/// SERVERS
		if (ecm_check_time<ticks) {
			pthread_mutex_lock(&prg.locksrv);
			ecm_check_time = check_sendecm();
			pthread_mutex_unlock(&prg.locksrv);
		}

		/// CLIENTS
		// Newcamd Clients
		if (cs_dcw_check_time<ticks) {
			pthread_mutex_lock(&prg.lockcli);
			cs_dcw_check_time = cs_check_sendcw();
			//debugf(" cs_dcw_check_time = %dms\n",cs_dcw_check_time-ticks);
			pthread_mutex_unlock(&prg.lockcli);
		}

#ifdef RADEGAST_SRV
		// Radegast Clients
		if (rdgd_dcw_check_time<ticks) {
			pthread_mutex_lock(&prg.lockrdgdcli);
			rdgd_dcw_check_time = rdgd_check_sendcw();
			pthread_mutex_unlock(&prg.lockrdgdcli);
		}
#endif

#ifdef MGCAMD_SRV
		if (mg_dcw_check_time<ticks) {
			if (cfg.mgcamd.handle>0) {
				pthread_mutex_lock(&prg.lockclimg);
				mg_dcw_check_time = mg_check_sendcw();
				pthread_mutex_unlock(&prg.lockclimg);
			} else mg_dcw_check_time = 0xffffffff;
		}
#endif


#ifdef CCCAM_SRV
		// CCcam Clients
		if (cc_dcw_check_time<ticks) {
			if (cfg.cccam.handle>0) {
				pthread_mutex_lock(&prg.lockcccli);
				cc_dcw_check_time = cc_check_sendcw();
				pthread_mutex_unlock(&prg.lockcccli);
			} else cc_dcw_check_time = 0xffffffff;
		}
#endif

#ifdef FREECCCAM_SRV
		// FreeCCcam Clients
		if (frcc_dcw_check_time<ticks) {
			if (cfg.freecccam.handle>0) {
				pthread_mutex_lock(&prg.lockfreecccli);
				frcc_dcw_check_time = freecc_check_sendcw();
				pthread_mutex_unlock(&prg.lockfreecccli);
			} else frcc_dcw_check_time = 0xffffffff;
		}
#endif

		//usleep( cfg.msgwaittime*1000 );
	}
}

int start_thread_recv_msg()
{
	create_prio_thread(&prg.tid_msg, (threadfn)recv_msg_thread, NULL, 50);
	return 0;
}

