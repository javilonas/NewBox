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

#include <stdio.h>

#define CACHE_STAT_WAIT     0
#define CACHE_STAT_DCW      1

struct cache_data {
	unsigned char status; // 0:Wait; 1: dcw received
	int32_t recvtime;
	unsigned char tag;
	unsigned short sid;
	unsigned short onid;
	unsigned short caid;
	int32_t hash;
	int32_t prov;
	unsigned char cw[32]; // 16 por defecto
	int32_t peerid;

	uint32_t sendpipe; // Send DCW to cecm_pipe non null send dcw to ecmpipe (it is the ecm recvtime frompipe
	int32_t sendcache; // local cache send status 0:none, 1: request, 2: reply

	//int32_t lastid[2]; // list by caid & tag lastid[0]->0x80  lastid[1]->0x81
};

#if TARGET == 3
#define MAX_CACHETAB 512
#else
#define MAX_CACHETAB 8192
#endif

struct cache_data cachetab[MAX_CACHETAB];
int32_t icachetab=0;
//int32_t fetchlastids[2] = { -1, -1 };

int32_t prevcachetab( int32_t index )
{
	if (index<1) index = MAX_CACHETAB-1; else index--; 
	return index;
}


struct cache_data *cache_new( struct cache_data *newdata )
{
	int32_t i = icachetab;
	memset( &cachetab[i], 0, sizeof(struct cache_data) );
	cachetab[i].status = CACHE_STAT_WAIT; // 0:Wait; 1: dcw received
	cachetab[i].recvtime = GetTickCount();
	cachetab[i].tag = newdata->tag;
	cachetab[i].sid = newdata->sid;
	cachetab[i].onid = newdata->onid;
	cachetab[i].caid = newdata->caid;
	cachetab[i].hash = newdata->hash;
	cachetab[i].prov = newdata->prov;
	//cachetab[i].lastid[0] = fetchlastids[0];
	//cachetab[i].lastid[1] = fetchlastids[1];
	icachetab++;
	if (icachetab>=MAX_CACHETAB) icachetab=0;
	return &cachetab[i];
}

struct cache_data *cache_fetch( struct cache_data *thereq )
{
	int32_t i = icachetab;
	uint32_t ticks = GetTickCount();
	do {
		if (i<1) i = MAX_CACHETAB-1; else i--; 
		if ( (cachetab[i].recvtime+15000)<ticks ) return NULL;
		if ( (cachetab[i].hash==thereq->hash)&&(cachetab[i].caid==thereq->caid)&&(cachetab[i].tag==thereq->tag)&&(cachetab[i].sid==thereq->sid) )
			//if ( (cachetab[i].onid==0)||(cachetab[i].onid==thereq->onid) )
			return &cachetab[i];
	} while (i!=icachetab);
	return NULL;
}

/*
struct cache_data *cache_fetch( struct cache_data *thereq )
{
	fetchlastids[0] = -1;
	fetchlastids[1] = -1;

	uint32_t ticks = GetTickCount();
	int32_t i = icachetab;
	while ( (i=prevcachetab(i)) != icachetab ) {
		if ( (cachetab[i].recvtime+15000)<ticks ) return NULL;
		if (cachetab[i].caid==thereq->caid) { // we have the latest data with same caid
			if ( (cachetab[i].hash==thereq->hash)&&(cachetab[i].tag==thereq->tag)&&(cachetab[i].sid==thereq->sid) ) return &cachetab[i];

			if (cachetab[i].tag==0x80) {
				fetchlastids[0] = i;
				fetchlastids[1] = cachetab[i].lastid[1];
			}
			else {
				fetchlastids[0] = cachetab[i].lastid[0];
				fetchlastids[1] = i;
			}

			while ( (i=cachetab[i].lastid[thereq->tag&1]) >= 0 ) {
				struct cache_data *pcache = &cachetab[i];
				if ( (pcache->recvtime+15000)<ticks ) return NULL;
				if ( (pcache->hash==thereq->hash)&&(pcache->sid==thereq->sid) )
					return pcache;
			}
			return NULL;
		}
	}
	return NULL;
}
*/

int32_t cache_check( struct cache_data *req )
{
	if ( ((req->tag&0xFE)!=0x80)||!req->caid||!req->hash ) return 0;
	if (!cfg.faccept0onid && !req->onid ) return 0;
	return 1;
}


int32_t cache_check_request( unsigned char tag, unsigned short sid, unsigned short onid, unsigned short caid, int32_t hash )
{
	if ( ((tag&0xFE)!=0x80)||!caid||!hash ) return 0;
	if (!cfg.faccept0onid && !onid ) return 0;
	return 1;
}

///////////////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// pipe --> cache
///////////////////////////////////////////////////////////////////////////////

int32_t pipe_send_cache_find( ECM_DATA *ecm, struct cardserver_data *cs)
{
	if ( !cache_check_request(ecm->ecm[0], ecm->sid, cs->onid, ecm->caid, ecm->hash) ) return 0;
	//send pipe to cache
	uchar buf[64]; // 32 por defecto
	buf[0] = PIPE_CACHE_FIND;
	buf[1] = 14; // Data length
	buf[2] = ecm->ecm[0];
	buf[3] = (ecm->sid)>>8; buf[4] = (ecm->sid)&0xff;
	buf[5] = (cs->onid)>>8; buf[6] = (cs->onid)&0xff;
	buf[7] = (ecm->caid)>>8; buf[8] = (ecm->caid)&0xff;
	buf[9] = ecm->hash>>24; buf[10] = ecm->hash>>16; buf[11] = ecm->hash>>8; buf[12] = ecm->hash & 0xff;
	buf[13] = ecm->provid>>16; buf[14] = ecm->provid>>8; buf[15] = ecm->provid & 0xff;
	//debugf(" Pipe Ecm->Cache: PIPE_CACHE_FIND %04x:%04x:%08x\n",ecm->caid, ecm->sid, ecm->hash);
	pipe_send( srvsocks[0], buf, 16);
	return 1;
}

int32_t pipe_send_cache_request( ECM_DATA *ecm, struct cardserver_data *cs)
{
	if ( !cache_check_request(ecm->ecm[0], ecm->sid, cs->onid, ecm->caid, ecm->hash) ) return 0;
	//send pipe to cache
	uchar buf[64]; // 32 por defecto
	buf[0] = PIPE_CACHE_REQUEST;
	buf[1] = 11; // Data length
	buf[2] = ecm->ecm[0];
	buf[3] = (ecm->sid)>>8;
	buf[4] = (ecm->sid)&0xff;
	buf[5] = (cs->onid)>>8;
	buf[6] = (cs->onid)&0xff;
	buf[7] = (ecm->caid)>>8;
	buf[8] = (ecm->caid)&0xff;
	buf[9] = ecm->hash>>24;
	buf[10] = ecm->hash>>16;
	buf[11] = ecm->hash>>8;
	buf[12] = ecm->hash & 0xff;
	// Send Profile ID
	//buf[13] = (cs->id)>>8; buf[14] = (cs->id)&0xff; // test
	//debugf(" Pipe Ecm->Cache: PIPE_CACHE_FIND %04x:%04x:%08x\n",ecm->caid, ecm->sid, ecm->hash);
	pipe_send( srvsocks[0], buf, 13);
	return 1;
}

int32_t pipe_recv_cache_request(uchar *buf,struct cache_data *req)
{
	req->tag = buf[2];
	req->sid = (buf[3]<<8) | buf[4];
	req->onid = (buf[5]<<8) | buf[6];
	req->caid = (buf[7]<<8) | buf[8];
	req->hash = (buf[9]<<24) | (buf[10]<<16) | (buf[11]<<8) | buf[12];
	return ( (buf[13]<<8)|buf[14] );
}


int32_t pipe_send_cache_reply( ECM_DATA *ecm, struct cardserver_data *cs)
{
	if ( !cache_check_request(ecm->ecm[0], ecm->sid, cs->onid, ecm->caid, ecm->hash) ) return 0;
	uchar buf[64]; // 32 por defecto
	buf[0] = PIPE_CACHE_REPLY;
	buf[2] = ecm->ecm[0];
	buf[3] = (ecm->sid)>>8;
	buf[4] = (ecm->sid)&0xff;
	buf[5] = (cs->onid)>>8;
	buf[6] = (cs->onid)&0xff;
	buf[7] = (ecm->caid)>>8;
	buf[8] = (ecm->caid)&0xff;
	buf[9] = ecm->hash>>24;
	buf[10] = ecm->hash>>16;
	buf[11] = ecm->hash>>8;
	buf[12] = ecm->hash & 0xff;

	if (ecm->dcwstatus==STAT_DCW_SUCCESS) {
		memcpy(buf+13, ecm->cw, 16);
		buf[1] = 11+16;
		pipe_send( srvsocks[0], buf, 13+16);
	}
	else {
		buf[1] = 11;
		pipe_send( srvsocks[0], buf, 13);
	}
	return 1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


#define TYPE_REQUEST   1
#define TYPE_REPLY     2
#define TYPE_PINGREQ   3
#define TYPE_PINGRPL   4
#define TYPE_RESENDREQ 5

SOCKET outsock;

void sendtopeer( struct cs_cachepeer_data *peer, unsigned char *buf, int32_t len)
{

	if (peer->host->ip && peer->port) {
		struct sockaddr_in si_other;
		int32_t slen=sizeof(si_other);
		memset((char *) &si_other, 0, sizeof(si_other));
		si_other.sin_family = AF_INET;
		si_other.sin_port = htons( peer->port );
		si_other.sin_addr.s_addr = peer->host->ip;

		if (flag_debugnet) {
			debugf(" cache: send data (%d) to peer (%s:%d)\n", len, peer->host->name,peer->port);
			debughex(buf,len);
		}

		while (1) {
			struct pollfd pfd;
			pfd.fd = peer->outsock;
			pfd.events = POLLOUT;
			int32_t retval = poll(&pfd, 1, 10);
			if (retval>0) {
				if ( pfd.revents & (POLLOUT) ) {
					sendto(peer->outsock, buf, len, 0, (struct sockaddr *)&si_other, slen);
				} else debugf(" cache: error sending data\n");
				break;
			}
			else if (retval<0) {
				if ( (errno!=EINTR)&&(errno!=EAGAIN) ) {
					debugf(" cache: error sending data\n");
					break;
				}
			}
			else debugf(" cache: error sending data\n");
		}

	}
}


void cache_send_request(struct cache_data *pcache,struct cs_cachepeer_data *peer)
{
	uchar buf[32]; //16 por defecto
	//01 80 00CD 0001 0500 8D1DB359
	buf[0] = TYPE_REQUEST;
	buf[1] = pcache->tag;
	buf[2] = pcache->sid>>8;
	buf[3] = pcache->sid;
	buf[4] = pcache->onid>>8;
	buf[5] = pcache->onid&0xff;
	buf[6] = pcache->caid>>8;
	buf[7] = pcache->caid;
	buf[8] = pcache->hash>>24;
	buf[9] = pcache->hash>>16;
	buf[10] = pcache->hash>>8;
	buf[11] = pcache->hash;
	//peer->sentreq++;
	sendtopeer(peer, buf, 12);
	//*debugf(" [CACHE] >> Cache Request to (%s:%d) %04x:%04x:%08x\n", peer->host->name, peer->port, pcache->caid,pcache->sid,pcache->hash);
}


void cache_send_reply(struct cache_data *pcache,struct cs_cachepeer_data *peer)
{
	uchar buf[64];
	//Common Data
	buf[0] = TYPE_REPLY;
	buf[1] = pcache->tag;
	buf[2] = pcache->sid>>8;
	buf[3] = pcache->sid;
	buf[4] = pcache->onid>>8;
	buf[5] = pcache->onid&0xff;
	buf[6] = pcache->caid>>8;
	buf[7] = pcache->caid;
	buf[8] = pcache->hash>>24;
	buf[9] = pcache->hash>>16;
	buf[10] = pcache->hash>>8;
	buf[11] = pcache->hash;
	buf[12] = pcache->tag;

//	if (pcache->status==CACHE_STAT_WAIT) sendtopeer(peer, buf, 13); // Failed
//	else {
	if (pcache->status==CACHE_STAT_DCW) {
		//peer->sentrep++;

		// <1:type> <1:ecmtag> <2:sid> <2:onid> <2:caid> <4:hash> <1:ecmtag> <16dcw> <2:connector namelen> <n:connector name>
		memcpy( buf+13, pcache->cw, 16);
		// Connector name
		//buf[29] = 0; buf[30] = strlen(cs->name); memcpy(buf+31, cs->name, strlen(cs->name) ); 
		sendtopeer(peer, buf, 29);
	}
}


void cache_send_resendreq(struct cache_data *pcache,struct cs_cachepeer_data *peer)
{
	// <1:TYPE_RESENDREQ> <2:port> <1:ecmtag> <2:sid> <2:onid> <2:caid> <4:hash>
	uchar buf[64];
	buf[0] = TYPE_RESENDREQ;
	//Port
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = cfg.cacheport>>8;
	buf[4] = cfg.cacheport&0xff;

	buf[5] = pcache->tag;
	buf[6] = pcache->sid>>8;
	buf[7] = pcache->sid;
	buf[8] = pcache->onid>>8;
	buf[9] = pcache->onid&0xff;
	buf[10] = pcache->caid>>8;
	buf[11] = pcache->caid;
	buf[12] = pcache->hash>>24;
	buf[13] = pcache->hash>>16;
	buf[14] = pcache->hash>>8;
	buf[15] = pcache->hash;
	sendtopeer(peer, buf, 16);
}

void cache_send_ping(struct cs_cachepeer_data *peer)
{
// 03 00 00 01 2F EE B1 CB 54 00 00 D8 03 
	unsigned char buf[128]; //32 default
	struct timeval tv;
	gettimeofday( &tv, NULL );
	buf[0] = TYPE_PINGREQ;

	// Self Program Use
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0; // ping packet number
	buf[4] = peer->id>>8; 
	buf[5] = peer->id&0xff;
	// Multics Identification
	buf[6] = 0; // unused must be zero (for next use)
	buf[7] = 'N'^buf[1]^buf[2]^buf[3]^buf[4]^buf[5]^buf[6]; // Multics Checksum
	buf[8] = 'N'; // Multics ID
	//Port
	buf[9] = 0;
	buf[10] = 0;
	buf[11] = cfg.cacheport>>8;
	buf[12] = cfg.cacheport&0xff;
	//Program
	buf[13] = 0x01; //ID
	buf[14] = 7; //LEN
	buf[15] = 'N'; buf[16] = 'e'; buf[17] = 'w'; buf[18] = 'B'; buf[19] = 'o'; buf[20] = 'x';
	buf[21] = 0;
	//Version
	buf[22] = 0x02; //ID
	buf[23] = 3; //LEN
	buf[24] = 'v'; buf[25] = '0'+(REVISION/10); buf[26] = '0'+(REVISION%10);
	buf[27] = 0;
	buf[28] = 0;
	sendtopeer( peer, buf, 29);
}

///////////////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////////////


struct cs_cachepeer_data *getpeerbyip(uint32_t ip)
{
	struct cs_cachepeer_data *peer = cfg.cachepeer;
	while(peer) {
		if (peer->host->ip==ip) return peer;
		peer = peer->next;
	}
	return NULL;
}

struct cs_cachepeer_data *getpeerbyaddr(uint32_t ip, uint16_t port)
{
	struct cs_cachepeer_data *peer = cfg.cachepeer;
	while(peer) {
		if ( (peer->host->ip==ip)&&(peer->recvport==port) ) return peer;
		peer = peer->next;
	}
	return NULL;
}

struct cs_cachepeer_data *getpeerbyid(int32_t id)
{
	struct cs_cachepeer_data *peer = cfg.cachepeer;
	while(peer) {
		if (peer->id==id) return peer;
		peer = peer->next;
	}
	return NULL;
}



///////////////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////////////

void cache_recvmsg()
{
	int32_t recv_ip;
	unsigned short recv_port;
	unsigned char buf[1024];
	struct sockaddr_in si_other;
	socklen_t slen=sizeof(si_other);
	uint ticks = GetTickCount();
	struct cs_cachepeer_data *peer;

	int32_t received = recvfrom( cfg.cachesock, buf, sizeof(buf), 0, (struct sockaddr*)&si_other, &slen);
	memcpy( &recv_ip, &si_other.sin_addr, 4);
	recv_port = ntohs(si_other.sin_port);

	if (received>0) {
		if (flag_debugnet) {
			debugf(" cache: recv data (%d) from address (%s:%d)\n", received, ip2string(recv_ip), recv_port );
			debughex(buf,received);
		}
		// Store Data
		struct cache_data req;

		switch(buf[0]) {
				case TYPE_REQUEST:
					// Check Peer
					peer = getpeerbyaddr(recv_ip,recv_port);
					if (!peer) {
						peer = getpeerbyip(recv_ip);
						if (!peer) break;
					}
					peer->lastactivity = ticks;
					//peer->totreq++;
					// Check Multics diferent version
					if ( !strcmp("MultiCS",peer->program) && (!strcmp("r63",peer->version)||!strcmp("r64",peer->version)||!strcmp("r65",peer->version)||!strcmp("r66",peer->version)||!strcmp("r67",peer->version)||!strcmp("r68",peer->version)||!strcmp("r69",peer->version)||!strcmp("r70",peer->version)||!strcmp("r71",peer->version)||!strcmp("r72",peer->version)||!strcmp("r73",peer->version)||!strcmp("r74",peer->version)||!strcmp("r75",peer->version)||!strcmp("r76",peer->version)||!strcmp("r77",peer->version)||!strcmp("r78",peer->version)||!strcmp("r79",peer->version)||!strcmp("r80",peer->version)||!strcmp("r81",peer->version)||!strcmp("r82",peer->version)||!strcmp("r83",peer->version)||!strcmp("r84",peer->version)||!strcmp("r85",peer->version)) ) break;
					// Check CSP
					if (received==20) { // arbiter number
						strcpy(peer->program,"CSP");
						break;
					}
					// Check Status
					if (peer->disabled) break;
					// Get DATA
					req.tag = buf[1];
					req.sid = (buf[2]<<8) | buf[3];
					req.onid = (buf[4]<<8) | buf[5];
					req.caid = (buf[6]<<8) | buf[7];
					req.hash = (buf[8]<<24) | (buf[9]<<16) | (buf[10]<<8) |buf[11];
					// Check Cache Request
					if (!cache_check(&req)) break;
					//
					peer->reqnb++;
					// ADD CACHE
					struct cache_data *pcache = cache_fetch( &req );
					if (pcache==NULL) {
						//*debugf(" [CACHE] << Cache Request from %s %04x:%04x:%08x\n", peer->host->name, req.caid, req.sid, req.hash);
						pcache = cache_new( &req );
						if (cfg.cache.trackermode) {
							// Send REQUEST to all Peers
							struct cs_cachepeer_data *p = cfg.cachepeer;
							while (p) {
								if (!p->disabled)
								if (p->host->ip && p->port)
								if ( (p->lastactivity+75000)>ticks )
								if ( !p->fblock0onid || pcache->onid )
									cache_send_request(pcache,p);
								p = p->next;
							}
							pcache->sendcache = 1;
							cfg.cachereq++;
						}
					}
					else if (!cfg.cache.trackermode) {
						if ( (pcache->status==CACHE_STAT_DCW)&&(pcache->sendcache!=2) ) {
							//debugf(" [CACHE] << Request Reply >> to peer %s %04x:%04x:%08x\n", peer->host->name, req.caid, req.sid, req.hash);
							peer->ihitfwd++;
							peer->hitfwd++;
							cache_send_reply(pcache,peer);
						}
					}
					break;


				case TYPE_REPLY:
					// Check Peer
					peer = getpeerbyaddr(recv_ip,recv_port);
					if (!peer) {
						peer = getpeerbyip(recv_ip);
						if (!peer) break;
					}
					peer->lastactivity = ticks;

					//peer->totrep++;
					// Check Multics diferent version
					if ( !strcmp("MultiCS",peer->program) && (!strcmp("r63",peer->version)||!strcmp("r64",peer->version)||!strcmp("r65",peer->version)||!strcmp("r66",peer->version)||!strcmp("r67",peer->version)||!strcmp("r68",peer->version)||!strcmp("r69",peer->version)||!strcmp("r70",peer->version)||!strcmp("r71",peer->version)||!strcmp("r72",peer->version)||!strcmp("r73",peer->version)||!strcmp("r74",peer->version)||!strcmp("r75",peer->version)||!strcmp("r76",peer->version)||!strcmp("r77",peer->version)||!strcmp("r78",peer->version)||!strcmp("r79",peer->version)||!strcmp("r80",peer->version)||!strcmp("r81",peer->version)||!strcmp("r82",peer->version)||!strcmp("r83",peer->version)||!strcmp("r84",peer->version)||!strcmp("r85",peer->version)) ) break;

					// Check Status
					if (peer->disabled) break;

					// 02 80 00CD 0001 0500 8D1DB359 80  // failed
					// 02 80 00CD 0001 0500 8D1DB359 80 00CD 0000 0500  63339F359A663232B73158405A255DDC  // OLD
					// 02 80 001F 0001 0100 9A3BA1C1 80 BC02DB99DE3D526D5702D42D4C249505  0005 6361726431 // NEW
					if (buf[12]!=buf[1]) {
						//peer->rep_badheader++;
						break;
					}
					req.tag = buf[1];
					req.sid = (buf[2]<<8) | buf[3];
					req.onid = (buf[4]<<8) | buf[5];
					req.caid = (buf[6]<<8) | buf[7];
					req.hash = (buf[8]<<24) | (buf[9]<<16) | (buf[10]<<8) |buf[11];
					// Check Cache Request
					if (!cache_check(&req)) {
						//peer->rep_badfields++;
						break;
					}
					//
					if (received==13) { // FAILED
						//peer->rep_failed++;
						//*debugf(" [CACHE] <| Failed Cache Reply from %s (CAID:%04x SID:%04x ONID:%04x)\n", peer->host->name, req.caid, req.sid, req.onid);
						// NOTHING TO DO
						break;
					}
					else if (received>=29) {
						// 02 80 001F 0001 0100 9A3BA1C1  80  BC02DB99DE3D526D5702D42D4C249505  0005 6361726431 // NEW
						if ( !acceptDCW(buf+13) ) {
							//peer->rep_baddcw++;
							break;
						}
						//*debugf(" [CACHE] << Good Cache Reply from %s %04x:%04x:%08x (ONID:%04x)\n", peer->host->name, req.caid, req.sid, req.hash, req.onid);
						peer->repok++; // Request+Reply

						// Search for Cache data
						struct cache_data *pcache = cache_fetch( &req );
						if (pcache==NULL) pcache = cache_new( &req );

						if (pcache->status!=CACHE_STAT_DCW) {
							//*debugf(" [CACHE] Update Cache DCW %04x:%04x:%08x\n", pcache->caid, pcache->sid, pcache->hash);
							pcache->peerid = peer->id;
							memcpy(pcache->cw, buf+13, 16);
							pcache->status = CACHE_STAT_DCW;
							if (pcache->sendpipe) {
								uchar buf[128]; // 32 por defecto
								buf[0] = PIPE_CACHE_FIND_SUCCESS;
								buf[1] = 11+2+16; // Data length
								buf[2] = pcache->tag;
								buf[3] = pcache->sid>>8; buf[4] = pcache->sid&0xff;
								buf[5] = pcache->onid>>8; buf[6] = pcache->onid&0xff;
								buf[7] = pcache->caid>>8; buf[8] = pcache->caid&0xff;
								buf[9] = pcache->hash>>24; buf[10] = pcache->hash>>16; buf[11] = pcache->hash>>8; buf[12] = pcache->hash & 0xff;
								buf[13] = peer->id>>8; buf[14] = peer->id&0xff;
								memcpy( buf+15, pcache->cw, 16);
								//*debugf(" pipe Cache->Ecm: PIPE_CACHE_FIND_SUCCESS %04x:%04x:%08x\n",pcache->caid, pcache->sid, pcache->hash); // debughex(buf, 13+16);
								pipe_send( srvsocks[1], buf, 13+2+16);
								//pcache->sendpipe = 0;
							}

							if (cfg.cache.trackermode) {
								// Send REQUEST to all Peers
								struct cs_cachepeer_data *p = cfg.cachepeer;
								while (p) {
									if (!p->disabled)
									if (p->host->ip && p->port)
									if ( (p->lastactivity+75000)>ticks )
									if ( !p->fblock0onid || pcache->onid )
										cache_send_reply(pcache,p);
									p = p->next;
								}
								pcache->sendcache = 2;
								cfg.cacherep++;
							}

						}
						else if ( pcache->sendpipe && memcmp(pcache->cw, buf+13, 16) ) {
							// resend to server
							pcache->peerid = peer->id;
							memcpy(pcache->cw, buf+13, 16);
							pcache->status = CACHE_STAT_DCW;

							uchar buf[128]; // 32 por defecto
							buf[0] = PIPE_CACHE_FIND_SUCCESS;
							buf[1] = 11+2+16; // Data length
							buf[2] = pcache->tag;
							buf[3] = pcache->sid>>8; buf[4] = pcache->sid&0xff;
							buf[5] = pcache->onid>>8; buf[6] = pcache->onid&0xff;
							buf[7] = pcache->caid>>8; buf[8] = pcache->caid&0xff;
							buf[9] = pcache->hash>>24; buf[10] = pcache->hash>>16; buf[11] = pcache->hash>>8; buf[12] = pcache->hash & 0xff;
							buf[13] = peer->id>>8; buf[14] = peer->id&0xff;
							memcpy( buf+15, pcache->cw, 16);
							pipe_send( srvsocks[1], buf, 13+2+16);
						}
					}
					break;

				// 03 00 00 01 2F EE B1 CB 54 00 00 D8 03 
				case TYPE_PINGREQ:
					// Check Peer
					peer = cfg.cachepeer;
					int32_t port = (buf[11]<<8)|buf[12];
					struct cs_cachepeer_data *peerip = NULL;
					while (peer) {
						if (peer->host->ip==recv_ip) {
							if (peer->port==port) { // Peer is found
								// Check Status
								if (peer->disabled) break;
								//
								peer->lastactivity = ticks;
								// Send Reply
								buf[0] = TYPE_PINGRPL;
								sendtopeer( peer, buf, 9);
								peer->recvport = recv_port;
								//if (!peer->ping) peer->lastpingsent = 0;
								// Check for NewBox Checksum
								if ( buf[8]=='N' && buf[7]==('N'^buf[1]^buf[2]^buf[3]^buf[4]^buf[5]^buf[6]) ) {
									strcpy(peer->program,"NewBox");
								}
								else strcpy(peer->program,"CSP");
								// Check for extended reply
								int32_t index = 13;
								while (received>index) {
									if ( (index+buf[index+1]+2)>received ) break;
									switch(buf[index]) {
										case 0x01:
											if (buf[index+1]<64)/*32*/ { memcpy(peer->program, buf+index+2, buf[index+1]); peer->program[buf[index+1]] = 0; }
											break;
										case 0x02:
											if (buf[index+1]<64)/*32*/ { memcpy(peer->version, buf+index+2, buf[index+1]); peer->version[buf[index+1]] = 0; }
											break;
									}
									index += 2+buf[index+1];
								}
								break;
							}
							peerip = peer;
						}
						peer = peer->next;
					}
					if (!peer) {
						if (peerip) {
							// Check for peer reuse
							peer = cfg.cachepeer;
							while (peer) {
								if ( (peer->port==port) && !strcmp(peer->host->name,peerip->host->name) ) break;
								peer = peer->next;
							}
							if (!peer) {
								// add new peer
								peer = malloc( sizeof(struct cs_cachepeer_data) );
								memset( peer, 0, sizeof(struct cs_cachepeer_data) );
								peer->host = peerip->host;
								peer->port = port;
								peer->next = cfg.cachepeer;
								peer->id = cfg.cachepeerid;
								peer->outsock = CreateClientSockUdp();
								peer->lastactivity = ticks;
								cfg.cachepeerid++;
								cfg.cachepeer = peer;
								// Send Reply
								buf[0] = TYPE_PINGRPL;
								sendtopeer( peer, buf, 9);
							}
							else {
								peer->host->checkiptime = 0;
							}
						}
/*
						else {
							// Send Reply
							buf[0] = TYPE_PINGRPL;
							sendtoip( recv_ip, port, buf, 9);
						}
*/
					}
					break;

				case TYPE_PINGRPL:
					// Check for good Reply Packet
					if (buf[8]!='N') break;
					if ( buf[7]!=('N'^buf[1]^buf[2]^buf[3]^buf[4]^buf[5]^buf[6]) ) break;
					// Get Peer
					int32_t peerid = (buf[4]<<8) | buf[5];
					peer = cfg.cachepeer;
					while (peer) {
						if ( (peer->host->ip==recv_ip)&&(peer->id==peerid) ) {
							peer->lastactivity = ticks;
							peer->recvport = recv_port;
							peer->lastpingrecv = GetTickCount();
							if (peer->ping>0)
								peer->ping = (peer->ping+peer->lastpingrecv-peer->lastpingsent)/2;
							else
								peer->ping = peer->lastpingrecv-peer->lastpingsent;
							//debugf(" Cache: Ping Reply from (%s:%d) = %dms\n", peer->host->name, peer->port, peer->ping);
							break;
						}
						peer = peer->next;
					}
					break;

				case TYPE_RESENDREQ:
					// <1:TYPE_RESENDREQ> <4:port> <1:ecmtag> <2:sid> <2:onid> <2:caid> <4:hash>
					//0000:  05 0523 80 00CD 0001 0500 8D1DB359
					// Check Peer
					peer = getpeerbyaddr(recv_ip,recv_port);
					if (!peer) peer = getpeerbyip(recv_ip);
					if (!peer) break;

					peer->lastactivity = ticks;
					// Check Multics diferent version
					if ( !strcmp("MultiCS",peer->program) && (!strcmp("r63",peer->version)||!strcmp("r64",peer->version)||!strcmp("r65",peer->version)||!strcmp("r66",peer->version)||!strcmp("r67",peer->version)||!strcmp("r68",peer->version)||!strcmp("r69",peer->version)||!strcmp("r70",peer->version)||!strcmp("r71",peer->version)||!strcmp("r72",peer->version)||!strcmp("r73",peer->version)||!strcmp("r74",peer->version)||!strcmp("r75",peer->version)||!strcmp("r76",peer->version)||!strcmp("r77",peer->version)||!strcmp("r78",peer->version)||!strcmp("r79",peer->version)||!strcmp("r80",peer->version)||!strcmp("r81",peer->version)||!strcmp("r82",peer->version)||!strcmp("r83",peer->version)||!strcmp("r84",peer->version)||!strcmp("r85",peer->version)) ) break;
					// Check Status
					if (peer->disabled) break;

					if (received>=16) { // Good Packet
						// <1:TYPE_RESENDREQ> <4:port> <1:ecmtag> <2:sid> <2:onid> <2:caid> <4:hash>
						struct cache_data req;
						req.tag = buf[5];
						req.sid = (buf[6]<<8) | buf[7];
						req.onid = (buf[8]<<8) | buf[9];
						req.caid = (buf[10]<<8) | buf[11];
						req.hash = (buf[12]<<24) | (buf[13]<<16) | (buf[14]<<8) | buf[15];
						// Check Cache Request
						if (!cache_check(&req)) break;
						struct cache_data *pcache = cache_fetch( &req );
						if ( (pcache!=NULL)&&(pcache->status==CACHE_STAT_DCW) ) {
							buf[4] = TYPE_REPLY;
							buf[16] = buf[3];
							memcpy( buf+17, pcache->cw, 16);
							sendtopeer( peer, buf+4, 29);
						}
					}
					break;

				default:
					debugf(" Cache: Unknown msg type\n"); debughex(buf,received);
					break;
			}
	}
}

void cache_pipe_recvmsg()
{
	uchar buf[512]; // 300 por defecto
	struct cache_data req;
	struct cs_cachepeer_data *peer;
	uint ticks = GetTickCount();
	struct cache_data *pcache;

	int32_t len = pipe_recv( srvsocks[1], buf );
	if (len>0) {
		//debugf(" Recv from Cache Pipe\n"); debughex(buf,len);
		switch(buf[0]) {
			case PIPE_LOCK:
				pthread_mutex_lock(&prg.lockmain);
				pthread_mutex_unlock(&prg.lockmain);
				break;
			case PIPE_CACHE_FIND:
				//Check for cachepeer
				req.tag = buf[2];
				req.sid = (buf[3]<<8) | buf[4];
				req.onid = (buf[5]<<8) | buf[6];
				req.caid = (buf[7]<<8) | buf[8];
				req.hash = (buf[9]<<24) | (buf[10]<<16) | (buf[11]<<8) | (buf[12]);
				req.prov = (buf[13]<<16) | (buf[14]<<8) | (buf[15]);
				// Check Cache Request
				if (!cache_check(&req)) {
					// Send find failed
					buf[0] = PIPE_CACHE_FIND_FAILED;
					buf[1] = 11;
					pipe_send( srvsocks[1], buf, 13);
					break;
				}
				pcache = cache_fetch( &req );
				if (pcache==NULL) {
					pcache = cache_new( &req );
					pcache->sendpipe = 1;
					// Send find failed
					buf[0] = PIPE_CACHE_FIND_FAILED;
					buf[1] = 11;
					//*debugf(" Pipe Cache->Ecm: PIPE_CACHE_FIND_FAILED\n"); debughex(buf,len);
					pipe_send( srvsocks[1], buf, 13);
				}
				else {
					pcache->prov = req.prov;
					//if (!pcache->sid) pcache->sid = req.sid;
					//if (!pcache->caid) pcache->caid = req.caid;
					if (pcache->status==CACHE_STAT_DCW) {
						struct cs_cachepeer_data *peer = getpeerbyid(pcache->peerid);
						if (peer) {
							peer->lastcaid = pcache->caid;
							peer->lastprov = pcache->prov;
							peer->lastsid = pcache->sid;
							peer->lastdecodetime = 0; //ticks - ecm->recvtime;
							buf[13] = peer->id>>8; buf[14] = peer->id & 0xff;
							memcpy( buf+15, pcache->cw, 16);
							buf[0] = PIPE_CACHE_FIND_SUCCESS;
							buf[1] = 11+2+16;
							//*debugf(" Pipe Cache->Ecm: PIPE_CACHE_FIND_SUCCESS\n"); debughex(buf,len);
							pipe_send( srvsocks[1], buf, 13+2+16);
							pcache->sendpipe = 0;
						}// else buf[13]=0; buf[14]=0;
					}
					else if (pcache->status==CACHE_STAT_WAIT) {
						pcache->sendpipe = 1;
						buf[0] = PIPE_CACHE_FIND_WAIT;
						buf[1] = 11;
						//debugf(" Pipe Cache->Ecm: PIPE_CACHE_FIND_WAIT\n");
						pipe_send( srvsocks[1], buf, 13);
					}
					else  {
						pcache->sendpipe = 1;
						// Send find failed
						buf[0] = PIPE_CACHE_FIND_FAILED;
						buf[1] = 11;
						//*debugf(" Pipe Cache->Ecm: PIPE_CACHE_FIND_FAILED\n"); debughex(buf,len);
						pipe_send( srvsocks[1], buf, 13);
					}
				}
				//debugf(" Cache Req Sendpipe = %d\n", pcache->sendpipe);
				break;

			case PIPE_CACHE_REQUEST:
				// Setup Cache Request
				req.tag = buf[2];
				req.sid = (buf[3]<<8) | buf[4];
				req.onid = (buf[5]<<8) | buf[6];
				req.caid = (buf[7]<<8) | buf[8];
				req.hash = (buf[9]<<24) | (buf[10]<<16) | (buf[11]<<8) |buf[12];
				// Check Cache Request
				if (!cache_check(&req)) break;
				//
				pcache = cache_fetch( &req );
				if (pcache==NULL) pcache = cache_new( &req );
				// Send Request if not sent
				if (pcache->sendcache==0) {
					pcache->sendcache = 1;
					cfg.cachereq++;
					peer = cfg.cachepeer;
					while (peer) {
						if (!peer->disabled)
						if (peer->host->ip && peer->port)
						if ( (peer->lastactivity+75000)>ticks )
						if ( !peer->fblock0onid || pcache->onid )
							cache_send_request(pcache,peer);
						peer = peer->next;
					}
				}
				break;

			case PIPE_CACHE_REPLY:
				// Setup Cache Request
				req.tag = buf[2];
				req.sid = (buf[3]<<8) | buf[4];
				req.onid = (buf[5]<<8) | buf[6];
				req.caid = (buf[7]<<8) | buf[8];
				req.hash = (buf[9]<<24) | (buf[10]<<16) | (buf[11]<<8) |buf[12];
				// Check Cache Request
				if (!cache_check(&req)) break;
				//
				pcache = cache_fetch( &req );
				if (pcache==NULL) pcache = cache_new( &req );
				// Send Request if not sent
				if (pcache->sendcache==0) {
					pcache->sendcache = 1;
					cfg.cachereq++;
					peer = cfg.cachepeer;
					while (peer) {
						if (!peer->disabled)
						if (peer->host->ip && peer->port)
						if ( (peer->lastactivity+75000)>ticks )
						if ( !peer->fblock0onid || pcache->onid )
							cache_send_request(pcache,peer);
						peer = peer->next;
					}
				}
				//Set DCW
				if (buf[1]==27) {
					if (pcache->status!=CACHE_STAT_DCW) { // already sent
						memcpy( pcache->cw, buf+13, 16); //dcw
						pcache->status = CACHE_STAT_DCW;
						pcache->peerid = 0; // from local
					}
				}
				// Send Reply
				if (pcache->sendcache==1) {
					pcache->sendcache = 2;
					if (pcache->status==CACHE_STAT_DCW) cfg.cacherep++;
					peer = cfg.cachepeer;
					while (peer) {
						if (!peer->disabled)
						if (peer->host->ip && peer->port)
						if ( (peer->lastactivity+75000)>ticks )
						if ( !peer->fblock0onid || pcache->onid )
							cache_send_reply(pcache,peer);
						peer = peer->next;
					}
				}
				break;
		}//switch
	}//if
}

void cache_check_peers()
{
	struct cs_cachepeer_data *peer = cfg.cachepeer;
	while (peer) {
		if ( (peer->host->ip)&&(peer->port) ) {
			uint ticks = GetTickCount();
			if (peer->ping==0) { // inactive
				if ( (!peer->lastpingsent)||((peer->lastpingsent+5000)<ticks) ) { // send every 15s
					cache_send_ping(peer);
					peer->lastpingsent = ticks;
					peer->lastpingrecv = 0;
					peer->ping = -1;
				}
			}
			else if (peer->ping==-1) { // inactive
				if ( (!peer->lastpingsent)||((peer->lastpingsent+10000)<ticks) ) { // send every 15s
					cache_send_ping(peer);
					peer->lastpingsent = ticks;
					peer->lastpingrecv = 0;
					peer->ping = -2;
				}
			}
			else if (peer->ping<=-2) { // inactive
				if ( (!peer->lastpingsent)||((peer->lastpingsent+15000)<ticks) ) { // send every 15s
					cache_send_ping(peer);
					peer->lastpingsent = ticks;
					peer->lastpingrecv = 0;
				}
			}
			else if (peer->ping>0) {
				if ( (!peer->lastpingrecv)&&((peer->lastpingsent+5000)<ticks) ) {
					cache_send_ping(peer);
					peer->lastpingsent = ticks;
					peer->lastpingrecv = 0;
					peer->ping = 0;
					peer->host->checkiptime = 0; // maybe ip changed
				}
				else if ( (peer->lastpingsent+60000)<ticks ) { // send every 75s
					cache_send_ping(peer);
					peer->lastpingsent = ticks;
					peer->lastpingrecv = 0;
				}
			}
		}
		peer = peer->next;
	}
}


///////////////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////////////

void *cache_thread(void *param)
{
	uint32_t chkticks = 0;
	prg.pid_cache = syscall(SYS_gettid);
	while(1) {
		if (cfg.cachesock>0) {
			// Check Peers Ping
			if ( GetTickCount()>(chkticks+3000) ) {
				cache_check_peers();
				chkticks = GetTickCount();
			}
			struct pollfd pfd;
			pfd.fd = cfg.cachesock;
			pfd.events = POLLIN | POLLPRI;
			int32_t retval = poll(&pfd, 1, 3000);

			if ( retval>0 ) {
				if ( pfd.revents & (POLLIN|POLLPRI) ) {
					pthread_mutex_lock( &prg.lockcache );

					do {
						cache_recvmsg();
						pfd.fd = cfg.cachesock;
						pfd.events = POLLIN | POLLPRI;
						if ( poll(&pfd, 1, 0) <1 ) break;
					} while (pfd.revents & (POLLIN|POLLPRI));

					pthread_mutex_unlock( &prg.lockcache );
				}
			}

		} else usleep( 30000 );
	}
	close(cfg.cachesock);
}


void *cachepipe_thread(void *param)
{
	while(1) {
		struct pollfd pfd;
		pfd.fd = srvsocks[1];
		pfd.events = POLLIN | POLLPRI;
		int32_t retval = poll(&pfd, 1, 3000);
		if ( retval>0 ) {
			if ( pfd.revents & (POLLIN|POLLPRI) ) {
				pthread_mutex_lock( &prg.lockcache );

				cache_pipe_recvmsg();

/*				do {
					cache_pipe_recvmsg();
					pfd.fd = srvsocks[1];
					pfd.events = POLLIN | POLLPRI;
					if ( poll(&pfd, 1, 0) <1 ) break;
				} while (pfd.revents & (POLLIN|POLLPRI));
*/
				pthread_mutex_unlock( &prg.lockcache );
			}
		}
	}
}


int32_t start_thread_cache()
{
	create_prio_thread(&prg.tid_cache, (threadfn)cache_thread,NULL, 50);
	create_prio_thread(&prg.tid_cache, (threadfn)cachepipe_thread,NULL, 20);
	return 0;
}

