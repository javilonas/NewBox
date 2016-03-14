#if 0
# 
# Copyright (c) 2014 - 2016 Javier Sayago <admin@lonasdigital.com>
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

#define MAX_ECM_DATA 4096

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
	STAT_DCW_FAILED, // decode failed
	STAT_DCW_WAIT, // Wait servers answer
	STAT_DCW_WAITCACHE, // Wait cached servers answer
	STAT_DCW_SUCCESS // dcw returned 
} dcwstatus_type;


#define DCW_SOURCE_NONE      0
#define DCW_SOURCE_CACHE     1
#define DCW_SOURCE_SERVER    2

#define DCW_SOURCE_CSCLIENT  3
#define DCW_SOURCE_MGCLIENT  4




typedef struct {
	// Ecm Data
	unsigned int csid; // cardserver unique id

	uint32_t recvtime;     // First request time in ms received from client
	uint32_t lastrecvtime; // Last request time received from client
	uint32_t lastsendtime; // Last request Time sent to server
	uint16_t sid; // Service id
	uint16_t caid; // CA id
	uint32_t provid; // Provider
	int32_t ecmlen;
	uint8_t ecm[512];
	uint32_t crc;
	uint32_t hash;
	// DCW/Status
	dcwstatus_type dcwstatus;
	uint8_t cw[32];
	// DCW SOURCE
	int32_t dcwsrctype;
	int32_t dcwsrcid;
	int32_t peerid; // Cache PeerID sending dcw(0=nothing)

	unsigned int checktime; // time when recheck the ecm.

	unsigned char cachestatus;// 0:nothing sent;; 1:request sent;; 2:reply sent

	char *statusmsg; // DCW status message

#ifdef CHECK_NEXTDCW
	// Last Successive ECM/DCW
	struct {	
		int32_t status; //0: nothing, 1: found last decode, -1:error dont search
		uint8_t dcw[32];
		int32_t error;
		int32_t ecmid;
		int32_t counter; // successif dcw counter * -1:error, 0: not found, 1: found and checked 1 time, 2: found and checked 2 times ...
		uint dcwchangetime;
	} lastdecode;
#endif

	// SERVERS that received ecm request
	struct {
		unsigned int sendtime; // ECM request sent time
		unsigned int statustime; // Last Status Time
		uint32_t srvid; // Server ID 0=nothing
		int32_t flag; // 0=request , 1=reply, 2: excluded(like cccam)
		uint8_t dcw[32];
	} server[20]; 

	int32_t waitcache; // 1: Wait for Cache; 0: dont wait
	
	int32_t srvmsgid;

} ECM_DATA;

extern ECM_DATA ecmdata[MAX_ECM_DATA];

void init_ecmdata();
uint32_t ecm_crc( uchar *ecm, int32_t ecmlen);
unsigned int hashCode( unsigned char *buf, int count);

ECM_DATA *getecmbyid(int32_t id);
int32_t prevecmid( int32_t ecmid );
int32_t nextecmid( int32_t ecmid );

int store_ecmdata(int csid,uchar *ecm,int ecmlen, unsigned short sid, unsigned short caid, unsigned int provid);
int search_ecmdata( uchar *ecm, int ecmlen, unsigned short sid);
int search_ecmdata_dcw( uchar *ecm, int ecmlen, unsigned short sid);
int32_t search_ecmdata_bymsgid( int32_t msgid );
int32_t search_ecmdata_byhash( uint32_t hash );

int ecm_addsrv(ECM_DATA *ecm, unsigned srvid);
int32_t ecm_nbsentsrv(ECM_DATA *ecm);
int32_t ecm_nbwaitsrv(ECM_DATA *ecm);
int ecm_setsrvflag(int ecmid, unsigned int srvid, int flag);
int ecm_setsrvflagdcw(int ecmid, unsigned int srvid, int flag, uchar dcw[32]);
int ecm_getsrvflag(int ecmid, unsigned int srvid);
int32_t ecm_getreplysrvid(int32_t ecmid);

int search_ecmdata_bycw( unsigned char *cw, uint32_t hash, unsigned short sid, unsigned short caid, unsigned int provid);

#ifdef CHECK_NEXTDCW
int32_t checkfreeze_setdcw( int32_t ecmid, uchar dcw[32] );
#endif

extern int32_t srvmsgid;
