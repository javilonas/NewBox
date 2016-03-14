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
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <errno.h>

#include "common.h"
#include "tools.h"
#include "debug.h"
#include "ecmdata.h"

ECM_DATA ecmdata[MAX_ECM_DATA];

int32_t srvmsgid = 0;


void init_ecmdata()
{
	memset(ecmdata, 0, sizeof(ecmdata));
}

uint32 ecm_crc( uchar *ecm, int32_t ecmlen)
{
	uchar checksum[4];
	int32_t counter;

	checksum[3]= ecm[0];
	checksum[2]= ecm[1];
	checksum[1]= ecm[2];
	checksum[0]= ecm[3];
	for ( counter=1; counter< (ecmlen/4) - 4; counter++)
	{
		checksum[3] ^=ecm[counter*4];
		checksum[2] ^=ecm[counter*4+1];
		checksum[1] ^=ecm[counter*4+2];
		checksum[0] ^=ecm[counter*4+3];
	}
	return ( (checksum[3]<<24) | (checksum[2]<<16) | (checksum[1]<<8) | checksum[0] );
}

unsigned int hashCode( unsigned char *buf, int32_t count)
{
	int32_t h = 0;
	int32_t i;
	for (i = 0; i < count; i++) h = 31*h + buf[i];
	return h;
}

ECM_DATA *getecmbyid(int32_t id)
{
	return &ecmdata[id];
}


int32_t prevecmid( int32_t ecmid )
{
	if (ecmid<1) ecmid = MAX_ECM_DATA-1; else ecmid--; 
	return ecmid;
}

int32_t nextecmid( int32_t ecmid )
{
	ecmid++; if (ecmid>=MAX_ECM_DATA) ecmid=0;
	return ecmid;
}

#ifdef CHECK_NEXTDCW
void checkfreeze_storeECM(int32_t ecmid);
#endif

int store_ecmdata(int csid, uchar *ecm,int ecmlen, unsigned short sid, unsigned short caid, unsigned int provid)
{
	uint ticks = GetTickCount();
	srvmsgid = nextecmid(srvmsgid);
	// Check Ecm
	if ( ecmdata[srvmsgid].recvtime && (ecmdata[srvmsgid].recvtime+30000>ticks) ) {
		debugf(" Cannot store ecm request\n");
		return -1;
	}
	memset( &ecmdata[srvmsgid], 0, sizeof(ECM_DATA) );
	memcpy( ecmdata[srvmsgid].ecm, ecm, ecmlen);
	ecmdata[srvmsgid].csid = csid;
	ecmdata[srvmsgid].ecmlen = ecmlen;
	ecmdata[srvmsgid].recvtime = ticks;
	ecmdata[srvmsgid].lastrecvtime = ticks;
	ecmdata[srvmsgid].crc = ecm_crc(ecm, ecmlen);
	ecmdata[srvmsgid].hash = hashCode(ecm+3, ecmlen-3);
	ecmdata[srvmsgid].dcwstatus = STAT_DCW_WAIT;
	ecmdata[srvmsgid].sid = sid;
	ecmdata[srvmsgid].caid = caid;
	ecmdata[srvmsgid].provid = provid;
	ecmdata[srvmsgid].srvmsgid = srvmsgid;
	memset( &ecmdata[srvmsgid].server, 0, sizeof(ecmdata[srvmsgid].server) );
	ecmdata[srvmsgid].waitcache = 0;
	ecmdata[srvmsgid].cachestatus = ECM_CACHE_NONE;
	//ecmdata[srvmsgid].dcwsrctype = DCW_SOURCE_NONE;

#ifdef CHECK_NEXTDCW
	checkfreeze_storeECM(srvmsgid);
#endif

	return srvmsgid;
}

#define TIME_ECMALIVE 60000

int32_t search_ecmdata( uchar *ecm, int32_t ecmlen, unsigned short sid)
{
	int32_t i;
	uint32 ticks = GetTickCount();
	uint32 crc = ecm_crc(ecm, ecmlen);
	for (i=0; i<MAX_ECM_DATA; i++) {
		if ( (ticks-ecmdata[i].recvtime) < TIME_ECMALIVE )
		if (sid==ecmdata[i].sid)
		if (crc==ecmdata[i].crc) return i;
	}
	return -1;
}

int32_t search_ecmdata_bymsgid( int32_t msgid )
{
	int32_t i;
	uint32 ticks = GetTickCount();
	for (i=0; i<MAX_ECM_DATA; i++) {
		if ( (ticks-ecmdata[i].recvtime) < TIME_ECMALIVE )
		if (msgid==ecmdata[i].srvmsgid) return i;
	}
	return -1;
}

int search_ecmdata_dcw( uchar *ecm, int ecmlen, unsigned short sid)
{
	int32_t i;
	uint32 ticks = GetTickCount();
	uint32 crc = ecm_crc(ecm, ecmlen);
	for (i=0; i<MAX_ECM_DATA; i++) {
		if ( (ticks-ecmdata[i].recvtime) < 10000 )
		if (ecmdata[i].dcwstatus!=STAT_DCW_FAILED)
		if (ecmlen==ecmdata[i].ecmlen)
		if (crc==ecmdata[i].crc)
		if ( !sid || !ecmdata[i].sid || (sid==ecmdata[i].sid) )
		if ( !memcmp(ecm, ecmdata[i].ecm, ecmlen) ) return i;
	}
	return -1;
}

int32_t search_ecmdata_byhash( uint32 hash )
{
	int32_t i;
	uint32 ticks = GetTickCount();
	for (i=0; i<MAX_ECM_DATA; i++) {
		if ( (ticks-ecmdata[i].recvtime) < TIME_ECMALIVE )
		if (hash==ecmdata[i].hash) return i;
	}
	return -1;
}



int ecm_addsrv(ECM_DATA *ecm, unsigned int srvid)
{
	int32_t i;
	uint32 ticks = GetTickCount();
	for(i=0; i<20; i++) {
		if (!ecm->server[i].srvid) {
			ecm->server[i].srvid = srvid;
			ecm->server[i].flag = ECM_SRV_REQUEST;
			ecm->server[i].sendtime = ticks;
			ecm->server[i].statustime = ticks; // last changed status time
			return 1;
		}
	}
	return 0;
}

int32_t ecm_nbservers(ECM_DATA *ecm)
{
	int32_t i;
	int32_t count=0;
	for(i=0; i<20; i++) {
		if (ecm->server[i].srvid) {
			count++;
		} else break;
	}
	return count;
}


int32_t ecm_nbsentsrv(ECM_DATA *ecm)
{
	int32_t i;
	int32_t count=0;
	for(i=0; i<20; i++) {
		if (ecm->server[i].srvid) {
			if (ecm->server[i].flag!=ECM_SRV_EXCLUDE) count++;
		} else break;
	}
	return count;
}

int32_t ecm_nbwaitsrv(ECM_DATA *ecm)
{
	int32_t i;
	int32_t count=0;
	for(i=0; i<20; i++) {
		if (ecm->server[i].srvid) {
			if (ecm->server[i].flag==ECM_SRV_REQUEST) count++;
		} else break;
	}
	return count;
}

int ecm_setsrvflag(int ecmid, unsigned int srvid, int flag)
{
	int32_t i;
	ECM_DATA *ecm = getecmbyid(ecmid);
	for(i=0; i<20; i++) {
		if (ecm->server[i].srvid) {
			if (ecm->server[i].srvid==srvid) {
				ecm->server[i].flag=flag;
				ecm->server[i].statustime = GetTickCount();
				return 1;
			}
		} else break;
	}
	return 0;
}

int ecm_setsrvflagdcw(int ecmid, unsigned int srvid, int flag, uchar dcw[16])
{
	int32_t i;
	ECM_DATA *ecm = getecmbyid(ecmid);
	for(i=0; i<20; i++) {
		if (ecm->server[i].srvid) {
			if (ecm->server[i].srvid==srvid) {
				ecm->server[i].flag=flag;
				ecm->server[i].statustime = GetTickCount();
				memcpy(ecm->server[i].dcw, dcw, 16);
				return 1;
			}
		} else break;
	}
	return 0;
}

int ecm_getsrvflag(int ecmid, unsigned int srvid)
{
	int32_t i;
	ECM_DATA *ecm = getecmbyid(ecmid);
	for(i=0; i<20; i++) {
		if (ecm->server[i].srvid) {
			if (ecm->server[i].srvid==srvid) {
				return ecm->server[i].flag;
			}
		} else break;
	}
	return 0;
}

int32_t ecm_getreplysrvid(int32_t ecmid)
{
	int32_t i;
	ECM_DATA *ecm = getecmbyid(ecmid);
	for(i=0; i<20; i++)
		if ( ecm->server[i].srvid && (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) ) return ecm->server[i].srvid;

	for(i=0; i<20; i++)
		if ( ecm->server[i].srvid && (ecm->server[i].flag==ECM_SRV_REPLY_FAIL) ) return ecm->server[i].srvid;

	return 0;
}

int search_ecmdata_bycw( unsigned char *cw, uint32 hash, unsigned short sid, unsigned short caid, unsigned int provid)
{
	int32_t i;
	for (i=0; i<MAX_ECM_DATA; i++) {
		if (ecmdata[i].dcwstatus==STAT_DCW_SUCCESS)
		if (provid==ecmdata[i].provid)
		if (caid==ecmdata[i].caid)
		if (sid==ecmdata[i].sid)
		if (hash!=ecmdata[i].hash)
		if (!memcmp(cw, ecmdata[i].cw, 16)) return i;
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#ifdef CHECK_NEXTDCW

/*

- search for ecm request of same channel, must be newest one and has decode success
- 

*/

// Get Last DCW for the same Channel
void checkfreeze_storeECM(int32_t ecmid)
{
	// find after storing ecm
	ECM_DATA *ecm = getecmbyid(ecmid);
	if (!ecm) return;
	if (ecm->lastdecode.status<0) return; // error
	if (ecm->lastdecode.status>0) return; // already done!!!

	if ( (ecm->lastdecode.status==0)&&(ecm->dcwstatus!=STAT_DCW_SUCCESS) ) {
		//debugf(" \n[SROTE ECM] New (%04x:%06x:%04x/%08x)\n",ecm->caid, ecm->provid, ecm->sid, ecm->hash);
		int32_t oldecmid = ecmid;
		while ( (oldecmid=prevecmid(oldecmid)) != ecmid ) {
			ECM_DATA *oldecm = getecmbyid(oldecmid);
			if (!oldecm) break;
			if ((ecm->recvtime-oldecm->recvtime)>60000) break;
			if ( (oldecm->dcwstatus==STAT_DCW_SUCCESS)
				&&(oldecm->ecmlen==ecm->ecmlen)&&(oldecm->ecm[0]!=ecm->ecm[0])
				&&(oldecm->caid==ecm->caid)&&(oldecm->provid==ecm->provid)&&(oldecm->sid==ecm->sid)
			) {
/*
				char str1[512];
				array2hex( oldecm->cw, str1, 16);
				debugf("Found (%04x:%06x:%04x/%08x) [%s] Stat:%d Cntr:%d Time:%d\n", oldecm->caid, oldecm->provid, oldecm->sid, oldecm->hash, str1, oldecm->lastdecode.status, oldecm->lastdecode.counter, oldecm->lastdecode.dcwchangetime);
*/
				if ( (oldecm->lastdecode.status==1)&&(oldecm->lastdecode.counter>1) ) {
					if ((ecm->recvtime-oldecm->recvtime)<(oldecm->lastdecode.dcwchangetime*12/10)) {
						// found one (not sure)
						ecm->lastdecode.status = 1;
						ecm->lastdecode.counter = oldecm->lastdecode.counter;
						// get average of ecm change time
						ecm->lastdecode.dcwchangetime = (oldecm->lastdecode.dcwchangetime*oldecm->lastdecode.counter+(ecm->recvtime-oldecm->recvtime)) / (oldecm->lastdecode.counter+1);
						ecm->lastdecode.dcwchangetime = ((ecm->lastdecode.dcwchangetime+300)/1000)*1000;
						memcpy( ecm->lastdecode.dcw, oldecm->cw, 16); // Store latest DCW
					}
				}
				else {
					// maybe??
					ecm->lastdecode.status = 1;
					ecm->lastdecode.counter = oldecm->lastdecode.counter;
					ecm->lastdecode.dcwchangetime = ecm->recvtime-oldecm->recvtime;
					memcpy( ecm->lastdecode.dcw, oldecm->cw, 16); // Store latest DCW
				}

				break;
			}
		}
	}
}

// return 0:wrong dcw, 1: good dcw
int32_t checkfreeze_setdcw( int32_t ecmid, uchar dcw[16] )
{
	char nullcw[8] = "\0\0\0\0\0\0\0\0";
	ECM_DATA *ecm = getecmbyid(ecmid);
	if (!ecm) return 1;
	if (ecm->lastdecode.status!=1) return 1; // no old successful decode

/*
	char str1[512];
	char str2[512];
	array2hex( ecm->lastdecode.dcw, str1, 16);
	array2hex( dcw, str2, 16);
	debugf(" \n[SET DCW] (%04x:%06x:%04x/%08x) Cntr:%d\nOLD[%s] -- NEW[%s]\n",ecm->caid, ecm->provid, ecm->sid, ecm->hash, ecm->lastdecode.counter, str1, str2);
*/
	if (ecm->lastdecode.counter<3) {
		// Check consecutif cw
		if ( memcmp(dcw,ecm->lastdecode.dcw,16) && ( !memcmp(dcw,ecm->lastdecode.dcw,8)||!memcmp(dcw+8,ecm->lastdecode.dcw+8,8) ) )
			ecm->lastdecode.counter++;
		else
			ecm->lastdecode.counter = 0;
		//debugf("OK1 New Cntr:%d\n",ecm->lastdecode.counter);
		return 1;
	}
	else {
		// Check similar cws // or null cw1/cw2 ==> no more consec.
		if ( memcmp(dcw,ecm->lastdecode.dcw,16) ) { // must be not the same
			if ( (!memcmp(dcw,ecm->lastdecode.dcw,8)||!memcmp(dcw+8,ecm->lastdecode.dcw+8,8)) ) {
				ecm->lastdecode.counter++;
				//debugf("OK2 New Cntr:%d\n",ecm->lastdecode.counter);
				//if (ecm->lastdecode.error) 	fdebugf(" ***(GOOD%d)SET DCW (%04x:%06x:%04x/%08x) Cntr:%d\nOLD[%s] -- NEW[%s]\n",ecm->lastdecode.error, ecm->caid, ecm->provid, ecm->sid, ecm->hash, ecm->lastdecode.counter, str1, str2);
				return 1;
			}
			else if ( (!memcmp(dcw,nullcw,8)||!memcmp(dcw+8,nullcw,8)) ) {
				// Remove consec
				ecm->lastdecode.counter = 0;
				//debugf("OK3 New Cntr:%d\n",ecm->lastdecode.counter);
				return 1;
			}
		}
	}
	ecm->lastdecode.error++;
	//fdebugf(" (FAILED%d) SET DCW (%04x:%06x:%04x/%08x) Cntr:%d\nOLD[%s] -- NEW[%s]\n",ecm->lastdecode.error, ecm->caid, ecm->provid, ecm->sid, ecm->hash, ecm->lastdecode.counter, str1, str2);
	//debugf("FAILED\n");
	return 0;
}


#endif
