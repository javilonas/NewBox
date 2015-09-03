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

#define KEEPALIVE_NEWCAMD	80


#if TARGET == 3
#define MAX_SIDS 1024
#else
#define MAX_SIDS 4096
#endif


#define MAX_CSPORTS 512 // 200 default

struct chninfo_data
{
	struct chninfo_data *next;
	unsigned short sid;
	unsigned short caid;
	unsigned int prov;
	char name[0];
};

struct sid_data
{
	struct sid_data *next;
	//uint8 nodeid[8]; // CCcam nodeid card real owner, each server has many cards(locals+remote) and propably each server has different locals
	unsigned short sid;
	//unsigned short caid;
	unsigned int prov;
	int val;
};


#if TARGET == 3
#define CARD_MAXPROV 24 // 16 default
#else
#define CARD_MAXPROV 32 // 32 default
#endif

struct cs_card_data
{
	struct cs_card_data *next;
	unsigned int shareid;			// This is for CCcam
	unsigned int localid;			// This is for Fake CCcam Cards
#ifdef CCCAM_CLI
	unsigned char uphops;		// Max distance to get cards
	unsigned char dnhops;
	uint8 nodeid[8]; // CCcam nodeid card real owner
#endif

	struct sid_data *sids;

	// ECM Statistics
	int ecmerrdcw;  // null DCW/failed DCW checksum (diffeent dcw)
	int ecmnb;	// number of ecm's requested
	int ecmok;	// dcw returned to client
	int ecmoktime;

	unsigned short caid;		// Card CAID
	int  nbprov;				// Nb providers
	unsigned int prov[CARD_MAXPROV];		// Card Providers

};


struct host_data
{
	struct host_data *next;
	char name[256];
	unsigned int ip;
	unsigned int checkiptime;
	unsigned int clip; // client ip
};

/*
ECM total: 0 (average rate: 0/s)
ECM forwards: 0 
ECM cache hits: 0 
ECM denied: 0 
ECM filtered: 0 
ECM failures: 0
EMM total: 0
*/


#define CACHE_091 0
#define CACHE_090 1

#define CACHE_PROG_DEFAULT 0
#define CACHE_PROG_CSP     1
#define CACHE_PROG_NEWBOX  2
#define CACHE_PROG_MULTICS 3

struct cs_cachepeer_data
{
	// Config data
	struct cs_cachepeer_data *next;
	struct host_data *host;
	unsigned short port;
	unsigned short csport[MAX_CSPORTS];
	int fblock0onid;
	int fsendrep;

	//## Runtime Data
	unsigned int id; // unique id
	int outsock;
	int disabled; // 0:Enable/1:Disable Client
	unsigned short recvport;
	unsigned int lastactivity; 
	int ping; // <=0 : inactive
	unsigned int lastpingsent; // last ping sent to peer
	unsigned int lastpingrecv; // last ping received from peer after a ping request

	char program[32];
	char version[32]; // cache version old:090/new:091 (by default 091 new)

	int ipoll;
	//Stat
/*
	int sentreq;
	int sentrep;
	int totreq; // total received requests (+errors)
	int totrep; // total received replies (+errors)
	int rep_badheader; // wrong header
	int rep_badfields; // badfields blocked replies
	int rep_failed; // failed replies
	int rep_baddcw;
*/
	int reqnb; // Total Requests (Good+Bad)
	int reqok; // Good Requests
	int repok;		// Replies without request
	int hitnb;     // All DCW transferred to clients
	int ihitnb;     // Instant Hits

	int hitfwd;  // Hits forwarded to peer
	int ihitfwd; // Instant Hits Forwarded to peer
	
	//Last Used Cached data
	uint16 lastcaid;
	uint32 lastprov;
	uint16 lastsid;
	uint32 lastdecodetime;

};


struct ip_hacker_data {
	struct ip_hacker_data *next;
	uint32 ip;
	char user[256];
	uint32 lastseen;
	int count;
	int nbconn;
};

///////////////////////////////////////////////////////////////////////////////

typedef enum
{
	STAT_DCW_SENT,	// no ecm found / DCW was sent to client
	STAT_ECM_SENT,	// ECM was sent to server
	STAT_ECM,		// ECM is waiting to be send
	STAT_DCW		// DCW is waiting to be send
} sendstatus_type;

// CLIENT FLAGS

//client is in config
#define FLAGCLI_CONFIG	1
//clietn is deleted dont receive good cw
#define FLAGCLI_DELETED	4

struct client_info_data
{
	struct client_info_data *next;
	char name[32];
	char value[256];
};

struct cs_client_data
{
	struct cs_client_data *next;
	int disabled; // 0:Enable/1:Disable Client
	unsigned int id; // unique id
	// User/Pass
	char user[64];
	char pass[64];
	//
	unsigned char type; // Clients type: NEWCAMD
	unsigned char flags;
	// Card
	struct cs_card_data card;
	//DCW Config
	unsigned int dcwtime; // minimum time interval (from Receiving ECM to sending CW) to client
	int dcwtimeout; // decode timeout interval in ms
	// Client Info.
	struct client_info_data *info;
	char *realname;

	//## Runtime Data (DYNAMIC)
	unsigned int ip;
	int handle;
	int ipoll;
	uint32 chkrecvtime; // message recv time
	//
	unsigned short progid; // program id ex: 0x4343=>CCcam/ 0x0000=>Generic
	// Connection time
	unsigned int uptime;
	unsigned int connected;
	unsigned int ping; // ping time;
	// Session Key
	unsigned char sessionkey[16];
	// ECM Stat
	int ecmnb;	// ecm number requested by client
	int ecmdenied;	// ecm number requested by client
	int ecmok;	// dcw returned to client
	int ecmoktime;
	unsigned int lastecmtime; // Last ecm time, if it was more than 5mn so reconnect to client
	unsigned int lastdcwtime; // last good dcw time sent to client
	int cachedcw; // dcw from client
	// ECM
	struct {
		int busy; // if ecmbusy dont process anyother ecm until that current ecm was finished
		sendstatus_type status; // answer was sent to client?
		// Ecm Data
		uint32 recvtime; // ECM Receive Time in ms
		int dcwtime; //depend on last ecm time
		int id;
		uint32 hash; // to check for ecm
		int climsgid; // MessageID for the ecm request
		//Last Used Share Saved data
		int lastid;
		uint16 lastcaid;
		uint32 lastprov;
		uint16 lastsid;
		int laststatus; // 1:success, 0:failed
		uint32 lastdecodetime;
		// DCW SOURCE
		int lastdcwsrctype;
		int lastdcwsrcid;
		uint32 lastcardid;
	} ecm;
};


#ifdef RADEGAST_SRV

struct rdgd_client_data { // Connected Client
	struct rdgd_client_data *next;
	int disabled; // 0:Enable/1:Disable Client
	unsigned int id; // unique id

	struct client_info_data *info;
	char *realname;

	//## Runtime Data (DYNAMIC)
	unsigned int ip;
	int handle;
	int ipoll;
	uint32 chkrecvtime; // message recv time

	// Connection time
	unsigned int connected;
	unsigned char type;
	// ECM Stat
	int ecmnb;	// ecm number requested by client
	int ecmdenied;	// ecm number requested by client
	int ecmok;	// dcw returned to client
	int ecmoktime;
	unsigned int lastecmtime; // Last ecm time, if it was more than 5mn so reconnect to client
	unsigned int lastdcwtime; // last good dcw time sent to client
	//
	int freeze; //a freeze: is a decode failed to a channel opened last time within 3mn
	struct {
		int busy; // if ecmbusy dont process anyother ecm until that current ecm was finished
		sendstatus_type status; // answer was sent to client?
		// Ecm Data
		uint32 recvtime; // ECM Receive Time in ms
		int dcwtime; //depend on last ecm time
		int id;
		//Last Used Share Saved data
		uint16 lastcaid;
		uint32 lastprov;
		uint16 lastsid;
		int laststatus;
		// DCW SOURCE
		int lastdcwsrctype;
		int lastdcwsrcid;
		uint32 lastcardid;
		uint32 lastdecodetime;
		char *statmsg; // DCW Status Message
	} ecm;
};

#endif


// cs : newcamd
// cc : cccam

struct cardserver_data
{
	struct cardserver_data *next;
	int disabled; // 0:Enable/1:Disable Client
	unsigned int id; // unique id
	char name[64];

	//NEWCAMD SERVER
	struct cs_client_data *client;
	unsigned char key[16];
	int port; // output port
	SOCKET handle;
	int ipoll;

#ifdef RADEGAST_SRV
	struct {
		struct rdgd_client_data *client;
		int port; // output port
		SOCKET handle;
		int ipoll;
	} rdgd;
#endif

	//CARD
	struct cs_card_data card;
	unsigned short onid;

	//DCW Config
	unsigned int dcwtime; // minimum time interval to send decode answer to client
	int dcwtimeout; // decode timeout in ms

#ifdef CHECK_NEXTDCW
	uint16 checkdcw;
#endif

	int maxfailedecm; // Max failed ecm per sid

	int cachetimeout;
	int usecache;
	int cachehits;  // total cache hits
	int cacheihits; // instant cache hits

	int faccept0sid;
	int faccept0provider;
	int faccept0caid;

	int fallowcccam;	// Allow cccam server protocol to decode ecm
	int fallownewcamd;	// Allow newcamd server protocol to decode ecm
	int fallowradegast;
	int fmaxuphops; // allowed cards distance to decode ecm
	int cssendcaid; // flag send caid to servers
	int cssendprovid; // flag send provid to servers
	int cssendsid; // flag send sid to servers

	//Servers Config
	int csmax;		// Maximum sevrer nb available to decode one ecm request
	int csfirst; // on start request servers number
	unsigned int csinterval;    // interval between 2 same ecm request to diffrent server
	unsigned int cstimeout;     // timeout for resending ecm request to server
	unsigned int cstimeperecm;  // min time to senddo a request
	unsigned int csvalidecmtime;  // max server ecm reply time
	int csretry; // Newcamd Retries
	int ccretry; // CCcam Retries
#ifdef RADEGAST_CLI
	int rdgdretry; // Radegast Retries
#endif
	// ECM Stat
	int ecmaccepted;	// accepted ecm
	int ecmdenied;	// denied/filtred ecm
	int ecmok;	// good dcw
	int ecmoktime;

	int ttime[101]; // contains number of dcw/time (0.0s,0.1s,0.2s ... 2.9s)
	int ttimecache[101]; // for cache only
	int ttimecards[101]; // for cards only
	int ttimeclients[101]; // for cards only

	unsigned int lastecmtime; // Last ecm time, if it was more than 5mn so reconnect to client
	unsigned int lastdcwtime; // last good dcw time sent to client
	void *lastecm;

	int isdeniedsids;
	unsigned short *sids; // global Accept sids
	int ecmbusysrv; // nb of ecm returned with busy srv
};



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//server is in config
#define FLAGSRV_CONFIG  1

// Server/client type
#define TYPE_NEWCAMD    1
#define TYPE_CCCAM      2
#define TYPE_GBOX       3
#define TYPE_RADEGAST   4
#define TYPE_CLONE      5
#define TYPE_OSCAM      6
#define TYPE_NEWBOX     7
#define TYPE_MULTICS    8
#define TYPE_MGCAMD     9


struct cs_server_data
{
	struct cs_server_data *next;
	int disabled; // 0:Enable/1:Disable Client
	unsigned int id; // unique id
	// Config DATA
	unsigned char type; // Clients type: NEWCAMD/CCcam
	unsigned short csport[MAX_CSPORTS];
	struct host_data *host;
	int port;
	char user[64];
	char pass[64];
	//Newcamd
	unsigned char key[16];

	// DYNAMIC DATA
	char *progname; // Known names
	char version[32];
	unsigned char flags;

	unsigned char sessionkey[16];

	int cscached; // flag for newcamd cached servers

	int error;
	char *statmsg; // Connection Status Message
	//CCcam additional data
#ifdef CCCAM_CLI
	struct cc_crypt_block sendblock;	// crypto state block
	struct cc_crypt_block recvblock;	// crypto state block
	unsigned char nodeid[8];
	char build[32];
	unsigned char uphops; // share limit
#endif

	//Connection Data
	SOCKET handle;
	int ipoll;
	uint32 chkrecvtime; // message recv time
	unsigned int connected;
	unsigned int uptime; // in ms

	struct cs_card_data *card;

	// TCP Connection Keepalive
	unsigned int ping; // ping time;
	unsigned int keepalivetime;	// CON: last Keepalive sent time / DIS: start time for trying connection 
	int keepalivesent;	// CON: Keepalive packet was sent to server and we have no response. / DIS: Retry number of reconnection

	// ECM Statistics
	int ecmtimeout; // number of errors for timeout (no cw returned by server)
	int ecmerrdcw;  // null DCW/failed DCW checksum (diffeent dcw)
	int ecmnb;	// total number of ecm requests
	int ecmok;	// dcw returned to client
	int ecmoktime;
	int ecmperhr;

	// CURRENT ECM DATA
	int busy; // cardserver is busy (ecm was sent) / or not (no ecm sent/dcw returned)
	int busyecmid; // ecm id
	uint32 busyecmhash; // to check for ecm
	struct cs_card_data *busycard; // card
	uint32 busycardid; // card

	int retry; // nb of retries of the current ecm request(for cccam, also for newcamd resend another ecm)

	unsigned int lastecmoktime;
	unsigned int lastecmtime; // Last ecm time, if it was more than 5mn so reconnect to client
	unsigned int lastdcwtime; // last good dcw time sent to client

	struct {
		int csid;
		int ecmnb;
		int ecmok;
		uint ecmoktime;
		int hits; // Ecm hits got from this server 
	} cstat[MAX_CSPORTS];

};


#ifdef CCCAM_SRV

struct cc_client_data { // Connected Client
	//### Config Data (STATIC)
	struct cc_client_data *next;
	int disabled; // 0:Enable/1:Disable Client
	unsigned int id; // unique id
	//fline
	char user[64];
	char pass[64];
	unsigned char dnhops;		// Max Down Hops
	unsigned int dcwtime;
	unsigned short csport[MAX_CSPORTS]; // Max 16
	unsigned char uphops;		// Max distance to get cards
	unsigned char shareemus;		// Client use our emu
	unsigned char allowemm;		// Client has rights for au
	// Client Info Data
	struct client_info_data *info;
	char *realname;
	struct tm enddate;
	struct host_data *host;

	//## Runtime Data (DYNAMIC)
	unsigned int ip;
	int handle;				// SOCKET
	int ipoll;
	uint32 chkrecvtime; // message recv time
	// CCcam Connection Data
	struct cc_crypt_block sendblock;	// crypto state block
	struct cc_crypt_block recvblock;	// crypto state block
	// Connection time
	unsigned int uptime;
	unsigned int connected;

	unsigned char nodeid[8];
	char version[32];
	char build[32];

	int cardsent; // flag

	// ECM Stat
	int ecmnb;	// ecm number requested by client
	int ecmdenied;	// ecm number requested by client
	int ecmok;	// dcw returned to client
	int ecmoktime;
	unsigned int lastecmtime; // Last ecm time, if it was more than 5mn so reconnect to client
	unsigned int lastdcwtime; // last good dcw time sent to client

	int freeze; //a freeze: is a decode failed to a channel opened last time within 3mn

	struct {
		int busy; // if ecmbusy dont process anyother ecm until that current ecm was finished
		sendstatus_type status; // answer was sent to client?
		unsigned int cardid;
		// Ecm Data
		uint32 recvtime; // ECM Receive Time in ms
		int dcwtime; //depend on last ecm time
		int id; //ecmid
		uint32 hash; // to check for ecm
		//Last Used Share Saved data
		int lastid;
		uint16 lastcaid;
		uint32 lastprov;
		uint16 lastsid;
		int laststatus;
		uint8 lastdcw[16];
		int lastdcwsrctype;
		int lastdcwsrcid;
		uint32 lastcardid;
		uint32 lastdecodetime;
		char *statmsg; // DCW Status Message
	} ecm;
};

#endif

#ifdef CCCAM

struct cccam_server {
#ifdef CCCAM_SRV
	struct cc_client_data *client;
	unsigned char key[16];
	int port; // output port
	SOCKET handle;
	int ipoll;
	unsigned short csport[MAX_CSPORTS]; // default cards
	int dcwtime;
#endif
	unsigned char nodeid[8];
	char version[32];
	char build[32];
#ifdef FREECCCAM_SRV
	char user[64];
	char pass[64];
	int maxusers;
#endif

	struct cs_card_data *fakecard;
};

#endif





#ifdef MGCAMD_SRV

struct mg_client_data
{
	struct mg_client_data *next;
	int disabled; // 0:Enable/1:Disable Client
	unsigned int id; // unique id
	// NEWCAMD SPECIFIC DATA
	char user[64];
	char pass[64];

	unsigned short csport[MAX_CSPORTS];

	// Client Info Data
	struct client_info_data *info;
	char *realname;
	struct tm enddate;
	struct host_data *host;
	//DCW Config
	unsigned int dcwtime; // minimum time interval (from Receiving ECM to sending CW) to client
	int dcwtimeout; // decode timeout interval in ms

	//## Runtime Data (DYNAMIC)
	unsigned int ip;
	SOCKET handle;
	int ipoll;
	uint32 chkrecvtime; // message recv time

	unsigned short progid; // program id ex: 0x4343=>CCcam/ 0x0000=>Generic
	unsigned char sessionkey[16];

	// Connection time
	unsigned int uptime;
	unsigned int connected;
	unsigned int ping; // ping time;

	int cardsent; // flag 0:none, 1:default, 2:all
	// ECM Stat
	int ecmnb;	// ecm number requested by client
	int ecmdenied;	// ecm number requested by client
	int ecmok;	// dcw returned to client
	int ecmoktime;
	unsigned int lastecmtime; // Last ecm time, if it was more than 5mn so reconnect to client
	unsigned int lastdcwtime; // last good dcw time sent to client
	int freeze;
	int cachedcw; // dcw from client

	struct {
		int busy; // if ecmbusy dont process anyother ecm until that current ecm was finished
		sendstatus_type status; // answer was sent to client?
		// Ecm Data
		uint32 recvtime; // ECM Receive Time in ms
		int dcwtime; //depend on last ecm time
		int id;
		uint32 hash; // to check for ecm
		int climsgid;
		//Last Used Share Saved data
		int lastid;
		uint16 lastcaid;
		uint32 lastprov;
		uint16 lastsid;
		int laststatus;
		// DCW SOURCE
		int lastdcwsrctype;
		int lastdcwsrcid;
		uint32 lastcardid;
		uint32 lastdecodetime;
	} ecm;
};


struct mgcamd_server {
	struct mg_client_data *client;

	int handle;
	int ipoll;
	int port; // output port
	unsigned short csport[MAX_CSPORTS]; // default cards
	unsigned char key[16];
	int dcwtime;
};

#endif

struct dcw_data {
	struct dcw_data *next;
	uchar dcw[16];
};


// Configurable Data
struct config_data
{
	// Latest Identifiers
	int clientid;
	int serverid;
	int cardserverid; // Profiles
	int cachepeerid; // 

	struct dcw_data *bad_dcw;

	// CACHE SERVERS
	struct {
		int trackermode; // 0: normal mode, 1: trackermode
	} cache;

	struct cs_cachepeer_data *cachepeer;
	int cachesock;
	int cacheport;
	int cachecwtimeout;
	int cachehits;  // Total Hits
	int cacheihits; // Instant Hits
	int cachereq; // Request sent
	int cacherep; // Replies
	int non0onid;
	int faccept0onid;

	//SERVERS
	int newcamdclientid;
	struct cs_server_data *server;

	// Host List
	struct host_data *host; 

	//CS PROFILES
	struct cardserver_data *cardserver;
#ifdef CCCAM
	int cccamclientid;
	struct cccam_server cccam;
#endif
#ifdef FREECCCAM_SRV
	struct cccam_server freecccam;
#endif
#ifdef MGCAMD_SRV
	int mgcamdclientid;
	struct mgcamd_server mgcamd;
#endif

	//WEBIF
#ifdef HTTP_SRV
	struct {
		int port;
		int handle;
		char user[64];
		char pass[64];
	} http;
#endif

	void *lastecm;

	uint msgwaittime; // in ms
	uint cachewaittime; // in ms
};

// Static Data
struct program_data
{
	int restart;
	struct timeval exectime; // last dcw time sent to client
	struct chninfo_data *chninfo;

	//PROCESS_ID
	pid_t pid_main;
	pid_t pid_cfg;
	pid_t pid_dns;
	pid_t pid_srv;
	pid_t pid_msg;
	pid_t pid_cache;

	//THREAD_ID
	pthread_t tid_cfg;
	pthread_t tid_dns;
	pthread_t tid_srv;
	pthread_t tid_msg;
	pthread_t tid_cache;

	pthread_t tid_date;
	pid_t pid_date;
	pthread_mutex_t lockthreaddate;

	pthread_mutex_t lockcache;

	pthread_mutex_t lock;		// ECM DATA(main data)
	pthread_mutex_t lockcli;	// CS Clients data
	pthread_mutex_t locksrv;	// CS Servers data
	// THREADS
	pthread_mutex_t locksrvth; // Servers connection thread
	pthread_mutex_t locksrvcs; // Newcamd server
	pthread_mutex_t lockdnsth; // DNS lookup Thread

#ifdef CCCAM_SRV
	pthread_mutex_t lockcccli; // CCcam Clients data
	pthread_mutex_t locksrvcc; // CCcam server
#endif
#ifdef FREECCCAM_SRV
	pthread_mutex_t lockfreecccli; // FreeCCcam Clients data
	pthread_mutex_t locksrvfreecc; // FreeCCcam server
#endif
	pthread_mutex_t lockrdgdcli; // Radegast Clients
	pthread_mutex_t lockrdgdsrv; // Radegast Server

#ifdef MGCAMD_SRV
	pthread_mutex_t lockclimg;	// CCcam Clients data
	pthread_mutex_t locksrvmg; // CCcam server
#endif

	pthread_mutex_t lockmain; // Check ECM/DCW Thread
	pthread_mutex_t lockecm;

	pthread_mutex_t lockhttp; // http Thread
	pthread_mutex_t lockdns;

	pthread_mutex_t lockdcw;
};

extern char config_file[512];
extern char config_badcw[512];
extern char config_channelinfo[512];
extern char cccam_nodeid[8];

void init_config(struct config_data *cfg);
int read_config(struct config_data *cfg);
int read_chinfo( struct program_data *prg );

void reread_config( struct config_data *cfg );
int check_config(struct config_data *cfg);
int done_config(struct config_data *cfg);

