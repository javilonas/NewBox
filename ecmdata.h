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

#define MAX_ECM_DATA 5096 // 4096 default

#define ECM_SRV_REQUEST     0
#define ECM_SRV_REPLY_GOOD  1
#define ECM_SRV_REPLY_FAIL  2
#define ECM_SRV_EXCLUDE     3

// cachestatus
#define ECM_CACHE_NONE      0
#define ECM_CACHE_REQ       1
#define ECM_CACHE_REP       2


typedef enum
{
	STAT_DCW_FAILED,	// decode failed
	STAT_DCW_WAIT,		// Wait servers answer
	STAT_DCW_WAITCACHE,	// Wait cached servers answer
	STAT_DCW_SUCCESS	// dcw returned 
} dcwstatus_type;


#define DCW_SOURCE_NONE      0
#define DCW_SOURCE_CACHE     1
#define DCW_SOURCE_SERVER    2

#define DCW_SOURCE_CSCLIENT  3
#define DCW_SOURCE_MGCLIENT  4




typedef struct {
	// Ecm Data
	unsigned int csid; // cardserver unique id

	uint32 recvtime;     // First request time in ms received from client
	uint32 lastrecvtime; // Last request time received from client
	uint32 lastsendtime; // Last request Time sent to server
	uint16 sid;					// Service id
	uint16 caid;				// CA id
	uint32 provid;				// Provider
	int ecmlen;
	uint8 ecm[512];
	uint32 crc;
	uint32 hash;
	// DCW/Status
	dcwstatus_type dcwstatus;
	uint8 cw[32];
	// DCW SOURCE
	int dcwsrctype;
	int dcwsrcid;
	int peerid; // Cache PeerID sending dcw(0=nothing)

	unsigned int checktime; // time when recheck the ecm.

	unsigned char cachestatus;// 0:nothing sent;; 1:request sent;; 2:reply sent

	char *statusmsg; // DCW status message

#ifdef CHECK_NEXTDCW
	// Last Successive ECM/DCW
	struct {	
		int status; //0: nothing, 1: found last decode, -1:error dont search
		uint8 dcw[32];
		int error;
        int ecmid;
        int counter; // successif dcw counter * -1:error, 0: not found, 1: found and checked 1 time, 2: found and checked 2 times ...
        uint dcwchangetime;
	} lastdecode;
#endif

	// SERVERS that received ecm request
	struct {
		unsigned int sendtime; // ECM request sent time
		unsigned int statustime; // Last Status Time
		uint32 srvid; // Server ID 0=nothing
		int flag; // 0=request , 1=reply, 2: excluded(like cccam)
		uint8 dcw[32];
	} server[20]; 

	int waitcache; // 1: Wait for Cache; 0: dont wait
	
	int srvmsgid;

} ECM_DATA;

extern ECM_DATA ecmdata[MAX_ECM_DATA];

void init_ecmdata();
uint32 ecm_crc( uchar *ecm, int ecmlen);
unsigned int hashCode( unsigned char *buf, int count);

ECM_DATA *getecmbyid(int id);
int prevecmid( int ecmid );
int nextecmid( int ecmid );

int store_ecmdata(int csid,uchar *ecm,int ecmlen, unsigned short sid, unsigned short caid, unsigned int provid);
int search_ecmdata( uchar *ecm, int ecmlen, unsigned short sid);
int search_ecmdata_dcw( uchar *ecm, int ecmlen, unsigned short sid);
int search_ecmdata_bymsgid( int msgid );
int search_ecmdata_byhash( uint32 hash );

int ecm_addsrv(ECM_DATA *ecm, unsigned srvid);
int ecm_nbsentsrv(ECM_DATA *ecm);
int ecm_nbwaitsrv(ECM_DATA *ecm);
int ecm_setsrvflag(int ecmid, unsigned int srvid, int flag);
int ecm_setsrvflagdcw(int ecmid, unsigned int srvid, int flag, uchar dcw[32]);
int ecm_getsrvflag(int ecmid, unsigned int srvid);
int ecm_getreplysrvid(int ecmid);

int search_ecmdata_bycw( unsigned char *cw, uint32 hash, unsigned short sid, unsigned short caid, unsigned int provid);

#ifdef CHECK_NEXTDCW
int checkfreeze_setdcw( int ecmid, uchar dcw[32] );
#endif

extern int srvmsgid;
