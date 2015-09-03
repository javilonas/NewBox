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
#include <stddef.h>

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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> 
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>

#include <time.h>


#include "common.h"
#include "convert.h"
#include "tools.h"
#include "debug.h"
#include "ecmdata.h"
#include "sockets.h"


#ifdef CCCAM
#include "msg-cccam.h"
#endif

#include "parser.h"
#include "config.h"
#include "main.h"

void cs_disconnect_cli(struct cs_client_data *cli);
void cs_disconnect_srv(struct cs_server_data *srv);
#ifdef CCCAM_SRV
void cc_disconnect_cli(struct cc_client_data *cli);
#endif
#ifdef MGCAMD_SRV
void mg_disconnect_cli(struct mg_client_data *cli);
#endif


///////////////////////////////////////////////////////////////////////////////
// HOST LIST
///////////////////////////////////////////////////////////////////////////////

struct host_data *add_host( struct config_data *cfg, char *hostname)
{
	struct host_data *host = cfg->host;
	// Search
	while (host) {
		if ( !strcmp(host->name, hostname) ) return host;
		host = host->next;
	}
	// Create
	host = malloc( sizeof(struct host_data) );
	memset( host, 0, sizeof(struct host_data) );
	strcpy( host->name, hostname );
	host->ip = 0;
	host->checkiptime = 0;
	host->next = cfg->host;
	cfg->host = host;
	return host;
}

void free_allhosts( struct config_data *cfg )
{
	while (cfg->host) {
		struct host_data *host = cfg->host;
		cfg->host = host->next;
		free(host);
	}
}


///////////////////////////////////////////////////////////////////////////////
// READ CONFIG
///////////////////////////////////////////////////////////////////////////////

// Default Newcamd DES key 
int cc_build[] = { 2892, 2971, 3094, 3165, 0 };

uint8 defdeskey[14] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14 };

char *cc_version[] = { "2.0.8", "2.0.9", "2.0.11", "2.1.1", "2.1.2", "2.1.3", "2.1.4", "2.2.0", "2.2.1", "2.3.0", "2.3.1", "3.0.1", "" };

char *uppercase(char *str)
{
	int i;
	for(i=0;;i++) {
		switch(str[i]) {
			case 'a'...'z':
				str[i] = str[i] - ('a'-'A');
				break;
			case 0:
				return str;
		}
	}
}

char currentline[10240];


void init_config(struct config_data *cfg)
{
	// Init config data
	memset( cfg, 0, sizeof(struct config_data) );

	cfg->clientid = 1;
	cfg->serverid = 1;
	cfg->cardserverid = 0x64; // Start like in CCcam 
	cfg->cachepeerid = 1;
	cfg->mgcamdclientid = 1;

#ifdef HTTP_SRV
	cfg->http.port = 5500;
	cfg->http.handle = INVALID_SOCKET;
#endif

	// CACHE
	cfg->cachecwtimeout=0;
	cfg->cacheport = 0;
	cfg->cachehits = 0;
	cfg->cachesock = 0;
	cfg->faccept0onid = 1;

#ifdef CCCAM
	//2.2.1 build 3316
	strcpy(cfg->cccam.version, cc_version[0]);
	sprintf( cfg->cccam.build, "%d", cc_build[0]);
#endif

#ifdef CCCAM_SRV
	cfg->cccamclientid = 1;
	cfg->cccam.client = NULL;
	memcpy(cfg->cccam.nodeid, cccam_nodeid, 8);
	cfg->cccam.handle = INVALID_SOCKET;
	cfg->cccam.port = 0;
	cfg->cccam.dcwtime = 0;
#endif

#ifdef FREECCCAM_SRV
	//2.2.1 build 3316
	strcpy(cfg->freecccam.version, cc_version[0]);//"2.0.11");
	sprintf(cfg->freecccam.build, "%d", cc_build[0]);

	cfg->freecccam.client = NULL;
	memcpy(cfg->freecccam.nodeid, cccam_nodeid, 8);
	cfg->freecccam.handle = INVALID_SOCKET;
	cfg->freecccam.port = 0;
	cfg->freecccam.dcwtime = 0;
#endif

#ifdef MGCAMD_SRV
	cfg->mgcamd.client = NULL;
	cfg->mgcamd.handle = INVALID_SOCKET;
	cfg->mgcamd.port = 0;
	cfg->mgcamd.dcwtime = 0;
	// MGcamd default key
	memcpy(cfg->mgcamd.key, defdeskey, 14);
#endif

	cfg->msgwaittime = 0;
	cfg->cachewaittime = 30;
}


void init_cardserver(struct cardserver_data *cs)
{
	memset( cs, 0, sizeof(struct cardserver_data) );

	cs->dcwtime = 0;
	cs->dcwtimeout = 3000;

	cs->port = 8000;

	cs->csmax = 0;		// Max cs per ecm ( 0:unlimited )
	cs->csinterval = 1000;	// interval between 2 same ecm to diffrent cs
	cs->cstimeout = 2000; // timeout for resending ecm to cs
	cs->cstimeperecm = 0; // min time to do a request
	cs->csvalidecmtime = 0; // cardserver max ecm reply time
	//cs->ccretry = 0; // Max number of retries for CCcam servers

	// Flags
	cs->faccept0sid = 1;
	cs->faccept0provider = 1;
	cs->faccept0caid = 1;
	cs->fallowcccam = 1;    // Allow cccam server protocol to decode ecm
	cs->fallownewcamd = 1;  // Allow newcamd server protocol to decode ecm
	cs->fallowradegast = 1;
	cs->fmaxuphops = 3;     // allowed cards distance to decode ecm
	cs->cssendcaid = 1;
	cs->cssendprovid = 1;
	cs->cssendsid = 1;
	memcpy(cs->key, defdeskey, 14);
	cs->usecache = 1;
}

void init_server(struct cs_server_data *srv)
{
	memset(srv,0,sizeof(struct cs_server_data) );
	memcpy( srv->key, defdeskey, 14);
	srv->handle = INVALID_SOCKET;
}



struct global_user_data
{
	struct global_user_data *next;
	char user[64];
	char pass[64];
	unsigned short csport[MAX_CSPORTS];
};


int read_config(struct config_data *cfg)
{
	int len,i;
	char str[255];
	FILE *fhandle;
	int nbline = 0;
	struct cardserver_data *cardserver = NULL; // Must be pointer for local server pointing
	struct cs_client_data *usr;
	struct cs_server_data *srv;
	int err = 0; // no error
	struct cardserver_data defaultcs;

	struct global_user_data *guser=NULL;

	// Open Config file
	fhandle = fopen(config_file,"rt");
	if (fhandle==0) {
		debugf(" file not found '%s'\n",config_file);
		return -1;
	} 
	debugf(" config: parsing file '%s'\n",config_file);
	// Init defaultcs
	init_cardserver(&defaultcs);
	//
	while (!feof(fhandle)) {
		memset(currentline, 0, sizeof(currentline) );
		if ( !fgets(currentline, 10239, fhandle) ) break;
		while (1) {
			char *pos;
			// Remove Comments
			pos = currentline;
			while (*pos) { 
				if (*pos=='#') {
					*pos=0;
					break;
				}
				pos++;
			}
			// delete from the end '\r' '\n' ' ' '\t'
			pos = currentline + strlen(currentline) - 1 ;
			while ( (*pos=='\r')||(*pos=='\n')||(*pos=='\t')||(*pos==' ') ) { 
				*pos=0; pos--;
				if (pos<=currentline) break;
			}
			if (*pos=='\\') {
				if ( !fgets(pos, 10239-(pos-currentline), fhandle) ) {
					*pos=0;
					break;
				}
			} else break;
		}
		//printf("%s\n", currentline);
		iparser = currentline;
		nbline++;
		parse_spaces();

		if (*iparser==0) continue;

		if (*iparser=='[') {
			iparser++;
			parse_value(str, "\r\n]" );
			if (*iparser!=']') {
				debugf(" config(%d,%d): ']' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;

			cardserver = malloc( sizeof(struct cardserver_data) );
			memcpy( cardserver, &defaultcs, sizeof(struct cardserver_data) );
			// Newcamd Port
			cardserver->port = defaultcs.port;
			if (defaultcs.port) defaultcs.port++;
			// Name
			strcpy(cardserver->name, str);
			//cardserver->next = cfg->cardserver;	cfg->cardserver = cardserver;
			cardserver->next = NULL;
			// ADD to CFG
			struct cardserver_data *tmpcs = cfg->cardserver;
			if (tmpcs) {
				while (tmpcs) {
					if (tmpcs->next) tmpcs = tmpcs->next; else break;
				}
				tmpcs->next = cardserver;
			} else cfg->cardserver = cardserver;
			continue;
		}

		if ( (*iparser<'A') || ((*iparser>'Z')&&(*iparser<'a')) || (*iparser>'z') ) continue; // each line iparser with a word
		len = parse_name(str);
		uppercase(str);
		err = 0; // no error


		if (!strcmp(str,"N")) {
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			struct cs_server_data tsrv;
			memset(&tsrv,0,sizeof(struct cs_server_data) );
			tsrv.type = TYPE_NEWCAMD;
			parse_str(str);
			tsrv.host = add_host(cfg, str);
			parse_int(str);
			tsrv.port = atoi( str );
			int lastport = 0;
			// Check for CWS
			parse_spaces();
			if (*iparser==':') {
				iparser++;
				if ( parse_int(str) ) lastport = atoi( str );
			} else lastport = tsrv.port;
			parse_str(tsrv.user);
			parse_str(tsrv.pass);
			for(i=0; i<14; i++)
				if ( parse_hex(str)!=2 ) {
					debugf(" config(%d,%d): Error reading DES-KEY\n",nbline,iparser-currentline);
					err++;
					break;
				} 
				else {
					tsrv.key[i] = hex2int(str);
				}
			if (err) {
				continue;
			}

			parse_spaces();
			if (*iparser=='{') { // Get Ports List
				iparser++;
				i = 0;
				while (i<MAX_CSPORTS) {
					if ( parse_int(str)>0 ) {
						tsrv.csport[i] = atoi(str);
						int j;
						for (j=0; j<i; j++) if ( tsrv.csport[j] && (tsrv.csport[j]==tsrv.csport[i]) ) break;
						if (j>=i) i++; else tsrv.csport[i] = 0;
					}
					else break;
					parse_spaces();
					if (*iparser==',') iparser++;
				}
				parse_spaces();
				if (*iparser!='}') debugf(" config(%d,%d): '}' expected\n",nbline,iparser-currentline);
			}
			tsrv.handle = INVALID_SOCKET;

			// Create Servers
			for (i=tsrv.port; i<=lastport; i++) {
				struct cs_server_data *srv = malloc( sizeof(struct cs_server_data) );
				memcpy(srv,&tsrv,sizeof(struct cs_server_data) );
				srv->port = i;
				srv->next = cfg->server;
				cfg->server = srv;
			}
		}
/*
		if (!strcmp(str,"N")) {
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			srv = malloc( sizeof(struct cs_server_data) );
			memset(srv,0,sizeof(struct cs_server_data) );
			srv->type = TYPE_NEWCAMD;
			parse_str(str);
			srv->host = add_host(cfg, str);
			parse_int(str);
			srv->port = atoi( str );
			parse_str(&srv->user[0]);
			parse_str(&srv->pass[0]);
			for(i=0; i<14; i++)
				if ( parse_hex(str)!=2 ) {
					debugf(" config(%d,%d): Error reading DES-KEY\n",nbline,iparser-currentline);
					err++;
					break;
				} 
				else {
					srv->key[i] = hex2int(str);
				}
			if (err) {
				free(srv);
				continue;
			}

			parse_spaces();
			if (*iparser=='{') { // Get Ports List
				iparser++;
				i = 0;
				while (i<MAX_CSPORTS) {
					if ( parse_int(str)>0 ) {
						srv->csport[i] = atoi(str);
						int j;
						for (j=0; j<i; j++) if ( srv->csport[j] && (srv->csport[j]==srv->csport[i]) ) break;
						if (j>=i) i++; else srv->csport[i] = 0;
					}
					else break;
					parse_spaces();
					if (*iparser==',') iparser++;
				}
				parse_spaces();
				if (*iparser!='}') debugf(" config(%d,%d): '}' expected\n",nbline,iparser-currentline);
			}

			srv->handle = INVALID_SOCKET;
			//srv->checkiptime = 0;
			srv->next = cfg->server;
			cfg->server = srv;
		}
*/
#ifdef RADEGAST_CLI
		//R: host port caid providers
		else if (!strcmp(str,"R")) {
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			srv = malloc( sizeof(struct cs_server_data) );
			memset(srv,0,sizeof(struct cs_server_data) );
			srv->type = TYPE_RADEGAST;
			parse_str(str);
			srv->host = add_host(cfg, str);
			parse_int(str);
			srv->port = atoi( str );
			// Card
			struct cs_card_data *card = malloc( sizeof(struct cs_card_data) );
			memset(card, 0, sizeof(struct cs_card_data) );
			srv->card = card;
			parse_hex(str);
			card->caid = hex2int( str );
			card->nbprov = 0;
			card->uphops = 1;
			for(i=0;i<CARD_MAXPROV;i++) {
				if ( parse_hex(str)>0 ) {
					card->prov[i] = hex2int( str );
					card->nbprov++;
				} else break;
				parse_spaces();
				if (*iparser==',') iparser++;
			}
			srv->handle = INVALID_SOCKET;
			srv->next = cfg->server;
			cfg->server = srv;
		}
#endif
		else if (!strcmp(str,"NEWCAMD")) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"CLIENTID")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_hex(str);
				cfg->newcamdclientid = hex2int(str);
			}
		}

#ifdef HTTP_SRV
		else if (!strcmp(str,"HTTP")) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"PORT")) {
				parse_spaces();
				if (*iparser!=':') {
					printf("':' expected\n");
					continue;
				} else iparser++;
				parse_int(str);
				//debugf("http port:%s\n", str);
				parse_spaces();
				if (!cardserver) cfg->http.port = atoi(str);
				else debugf(" config: skip http port, defined within profile\n");
			}
			else if (!strcmp(str,"USER")) {
				parse_spaces();
				if (*iparser!=':') {
					printf("':' expected\n");
					continue;
				} else iparser++;
				parse_str(cfg->http.user);
			}
			else if (!strcmp(str,"PASS")) {
				parse_spaces();
				if (*iparser!=':') {
					printf("':' expected\n");
					continue;
				} else iparser++;
				parse_str(cfg->http.pass);
			}
		}
#endif

		else if (!strcmp(str,"BAD-DCW")) {
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			struct dcw_data *dcw = malloc( sizeof(struct dcw_data) );
			memset(dcw,0,sizeof(struct dcw_data) );
			int error = 0;
			for(i=0; i<16; i++)
				if ( parse_hex(str)!=2 ) {
					debugf(" config(%d,%d): Error reading BAD-DCW\n",nbline,iparser-currentline);
					error++;
					break;
				} 
				else {
					dcw->dcw[i] = hex2int(str);
				}
			if (error)
				free(dcw);
			else {
				dcw->next = cfg->bad_dcw;
				cfg->bad_dcw = dcw;
			}
		}

///////////////////////////////////////////////////////////////////////////////
// DEFAULT
///////////////////////////////////////////////////////////////////////////////


		else if (!strcmp(str,"DEFAULT")) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"KEY")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				for(i=0; i<14; i++) if ( parse_hex(str)!=2 ) break; else defaultcs.key[i] = hex2int(str);
			}

			else if (!strcmp(str,"DCW")) {
				parse_name(str);
				uppercase(str);
				if (!strcmp(str,"TIMEOUT")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue; 
					} else iparser++;
					parse_int(str);
					parse_spaces();
					defaultcs.dcwtimeout = atoi(str);
					if (defaultcs.dcwtimeout<300) defaultcs.dcwtimeout=300; // default 300
					else if (defaultcs.dcwtimeout>2500) defaultcs.dcwtimeout=2500; //default 9999
				}
				else if (!strcmp(str,"MAXFAILED")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					parse_spaces();
					defaultcs.maxfailedecm = atoi(str);
					if (defaultcs.maxfailedecm<0) defaultcs.maxfailedecm=0;
					else if (defaultcs.maxfailedecm>15) defaultcs.maxfailedecm=15; // default 100
				}
				else if ( (!strcmp(str,"TIME"))||(!strcmp(str,"MINTIME")) ) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					parse_spaces();
					defaultcs.dcwtime = atoi(str);
					if (defaultcs.dcwtime<0) defaultcs.dcwtime=0;
					else if (defaultcs.dcwtime>200) defaultcs.dcwtime=200; // default 500
				}
#ifdef CHECK_NEXTDCW
				else if (!strcmp(str,"CHECK")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					if (parse_bin(str)) defaultcs.checkdcw = str[0]=='1';
				}
#endif
			}
			else if ( !strcmp(str,"SERVER") ) {
				parse_name(str);
				uppercase(str);
				if (!strcmp(str,"TIMEOUT")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					defaultcs.cstimeout = atoi(str);
					if ( (defaultcs.cstimeout<300)||(defaultcs.cstimeout>10000) ) defaultcs.cstimeout=300;
				}
				else if (!strcmp(str,"MAX")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					defaultcs.csmax = atoi(str);
					if (defaultcs.csmax<0) defaultcs.csmax=0;
					else if (defaultcs.csmax>15) defaultcs.csmax=15; // default 10 - 10
				}
				else if (!strcmp(str,"INTERVAL")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					defaultcs.csinterval = atoi(str);
					if (defaultcs.csinterval<100) defaultcs.csinterval=100;
					else if (defaultcs.csinterval>2000) defaultcs.csinterval=2000; // default 3000
				}
				else if (!strcmp(str,"VALIDECMTIME")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					defaultcs.csvalidecmtime = atoi(str);
					if (defaultcs.csvalidecmtime>2000) defaultcs.csvalidecmtime=2000; // default 5000
				}
				else if (!strcmp(str,"FIRST")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					defaultcs.csfirst = atoi(str);
					if (defaultcs.csfirst<0) defaultcs.csfirst=0;
					else if (defaultcs.csfirst>3) defaultcs.csfirst=3; // default 5
				}
				else debugf(" config(%d,%d): profile variable expected\n",nbline,iparser-currentline);
			}
			else if ( !strcmp(str,"RETRY") ) {
				parse_name(str);
				uppercase(str);
				if (!strcmp(str,"NEWCAMD")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					defaultcs.csretry = atoi(str);
					if (defaultcs.csretry<0) defaultcs.csretry=0;
					else if (defaultcs.csretry>3) defaultcs.csretry=3;
				}
				else if (!strcmp(str,"CCCAM")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					defaultcs.ccretry = atoi(str);
					if (defaultcs.ccretry<0) defaultcs.ccretry=0;
					else if (defaultcs.ccretry>5) defaultcs.ccretry=5; // default 10
				}
				else debugf(" config(%d,%d): profile variable expected\n",nbline,iparser-currentline);
			}
			else if ( !strcmp(str,"CACHE") ) {
				parse_name(str);
				uppercase(str);
				if (!strcmp(str,"TIMEOUT")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					parse_int(str);
					defaultcs.cachetimeout = atoi(str);
					if (defaultcs.cachetimeout<0) defaultcs.cachetimeout=0;
					else if (defaultcs.cachetimeout>2000) defaultcs.cachetimeout=2000; // default 5000
				}
			}
		}

///////////////////////////////////////////////////////////////////////////////


		else if (!strcmp(str,"WAITTIME")) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"MAIN")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cfg->msgwaittime = atoi(str);
				if (cfg->msgwaittime<0) cfg->msgwaittime=0;
				else if (cfg->msgwaittime>100) cfg->msgwaittime=100;
			}
			else if (!strcmp(str,"CACHE")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cfg->cachewaittime = atoi(str);
				if (cfg->cachewaittime<0) cfg->cachewaittime=0;
				else if (cfg->cachewaittime>100) cfg->cachewaittime=100;
			}
		}

///////////////////////////////////////////////////////////////////////////////


#ifdef CCCAM_CLI
		else if (!strcmp(str,"C")) {
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			srv = malloc( sizeof(struct cs_server_data) );
			memset(srv,0,sizeof(struct cs_server_data) );
			srv->type = TYPE_CCCAM;
			parse_str(str);
			srv->host = add_host(cfg, str);
			parse_int(str);
			srv->port = atoi( str );
			parse_str(&srv->user[0]);
			parse_str(&srv->pass[0]);
			if (parse_int(str)) srv->uphops = atoi( str ); else srv->uphops=1;
			parse_spaces();
			if (*iparser=='{') { // Get Ports List
				iparser++;
				i = 0;
				while (i<MAX_CSPORTS) {
					if ( parse_int(str)>0 ) {
						srv->csport[i] = atoi(str);
						int j;
						for (j=0; j<i; j++) if ( srv->csport[j] && (srv->csport[j]==srv->csport[i]) ) break;
						if (j>=i) i++; else srv->csport[i] = 0;
					}
					else break;
					parse_spaces();
					if (*iparser==',') iparser++;
				}
				parse_spaces();
				if (*iparser!='}') debugf(" config(%d,%d): '}' expected\n",nbline,iparser-currentline);
			}

			srv->handle = INVALID_SOCKET;
			srv->next = cfg->server;
			cfg->server = srv;
		}
#endif

#ifdef CCCAM_SRV
		else if (!strcmp(str,"F")) {
			// F : user pass <downhops> <dcwtime> <uphops>
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			struct cc_client_data *cli = malloc( sizeof(struct cc_client_data) );
			memset(cli, 0, sizeof(struct cc_client_data) );
			// init Default
			cli->dnhops = 0;
			cli->dcwtime = 0;
			cli->uphops = 0;
			// <user> <pass> <downhops> <dcwtime> <uphops> { <CardserverPort>,... }
			parse_str(cli->user);
			parse_str(cli->pass);
			// TODO: Check for same user 
			if (parse_int(str)) {
				cli->dnhops = atoi(str);
				if (parse_int(str)) {
					cli->dcwtime = atoi(str);
					if (cli->dcwtime<0) cli->dcwtime=0;
					else if (cli->dcwtime>300) cli->dcwtime=300; // default 500
					if (parse_int(str)) {
						cli->uphops = atoi(str);
					}
				}
			}

			parse_spaces();
			if (*iparser=='{') { // Get Ports List & User Info
				iparser++;
				parse_spaces();
				if ( (*iparser>='0')&&(*iparser<='9') ) {
					i = 0;
					while (i<MAX_CSPORTS) {
						if ( parse_int(str)>0 ) {
							// check for uphops
							if (i==0) {
								parse_spaces();
								if (*iparser==':') {
									iparser++;
									cli->uphops = atoi(str);
									continue;
								}
							}
							// check for port
							cli->csport[i] = atoi(str);
							int j;
							for (j=0; j<i; j++) if ( cli->csport[j] && (cli->csport[j]==cli->csport[i]) ) break;
							if (j>=i) i++; else cli->csport[i] = 0;
						}
						else break;
						parse_spaces();
						if (*iparser==',') iparser++;
					}
				}
				else {
					while (1) {
						parse_spaces();
						if (*iparser=='}') break;
						// NAME1=value1; Name2=Value2 ... }
						parse_value(str,"\r\n\t =");
						//printf(" NAME: '%s'\n", str);
						// '='
						parse_spaces();
						if (*iparser!='=') break;
						iparser++;
						// Value
						// Check for PREDEFINED names
						if (!strcmp(str,"profiles")) {
							i = 0;
							while (i<MAX_CSPORTS) {
								if ( parse_int(str)>0 ) {
									// check for port
									int n = cli->csport[i] = atoi(str);
									int j;
									for (j=0; j<i; j++) if ( cli->csport[j] && (cli->csport[j]==n) ) break;
									if (j>=i) { 
										cli->csport[i] = n;
										i++;
									}
								}
								else break;
								parse_spaces();
								if (*iparser==',') iparser++;
							}
						}
						else {
							struct client_info_data *info = malloc( sizeof(struct client_info_data) );
							strcpy(info->name, str);
							parse_spaces();
							parse_value(str,"\r\n;}");
							for(i=strlen(str)-1; ( (str[i]==' ')||(str[i]=='\t') ) ; i--) str[i] = 0; // Remove spaces
							//printf(" VALUE: '%s'\n", str);
							strcpy(info->value, str);
							info->next = cli->info;
							cli->info = info;
						}
						parse_spaces();
						if (*iparser==';') iparser++; else break;
					}
					// Set Info Data
					struct client_info_data *info = cli->info;
					while (info) {
						strcpy(str,info->name);
						uppercase(str);
						if (!strcmp(str,"NAME")) cli->realname = info->value;
						else if (!strcmp(str,"ENDDATE")) {
						    strptime( info->value, "%Y-%m-%d", &cli->enddate);
 						}
						else if (!strcmp(str,"HOST")) {
							cli->host = add_host(cfg,info->value);
 						}
						info = info->next;
					}
				}	
				if (*iparser!='}') debugf(" config(%d,%d): '}' expected\n",nbline,iparser-currentline);
			}

			cli->handle = -1;
			cli->next = cfg->cccam.client;
			cfg->cccam.client = cli;
		}
#endif

#ifdef CCCAM
		else if (!strcmp(str,"CCCAM")) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"VERSION")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_str(str);
				for(i=0; cc_version[i]; i++)
					if ( !strcmp(cc_version[i],str) ) {
						strcpy(cfg->cccam.version,cc_version[i]);
						sprintf(cfg->cccam.build, "%d", cc_build[i]);
						break;
					}
			}

			else if (!strcmp(str,"NODEID")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				if ( parse_hex(str)==16 ) hex2array( str, cfg->cccam.nodeid );
			}

#ifdef CCCAM_SRV
			else if (!strcmp(str,"PORT")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cfg->cccam.port = atoi(str);
			}
			else if (!strcmp(str,"PROFILES")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				for(i=0;i<MAX_CSPORTS;i++) {
					if ( parse_int(str)>0 ) {
						cfg->cccam.csport[i] = atoi(str);
					}
					else break;
					parse_spaces();
					if (*iparser==',') iparser++;
				}
			}
#endif
		}
#endif

#ifdef FREECCCAM_SRV
		else if (!strcmp(str,"FREECCCAM")) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"PORT")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cfg->freecccam.port = atoi(str);
			}
			else if (!strcmp(str,"MAXUSERS")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cfg->freecccam.maxusers = atoi(str);
			}
			else if (!strcmp(str,"DCWMINTIME")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cfg->freecccam.dcwtime = atoi(str);
				if (cfg->freecccam.dcwtime<0) cfg->freecccam.dcwtime=0;
				else if (cfg->freecccam.dcwtime>300) cfg->freecccam.dcwtime=300; // default 500

			}
			else if (!strcmp(str,"USERNAME")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_str(cfg->freecccam.user);
			}
			else if (!strcmp(str,"PASSWORD")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_str(cfg->freecccam.pass);
			}
			else if (!strcmp(str,"PROFILES")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				for(i=0;i<MAX_CSPORTS;i++) {
					if ( parse_int(str)>0 ) {
						cfg->freecccam.csport[i] = atoi(str);
					}
					else break;
					parse_spaces();
					if (*iparser==',') iparser++;
				}
			}
		}
#endif

#ifdef MGCAMD_SRV
		else if ( !strcmp(str,"MG")||!strcmp(str,"MGUSER") ) {
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			struct mg_client_data *cli = malloc( sizeof(struct mg_client_data) );
			memset(cli, 0, sizeof(struct mg_client_data) );
			// init Default
			cli->dcwtime = 0;

			// <user> <pass> <dcwtime> { <CardserverPort>,... }
			parse_str(cli->user);
			parse_str(cli->pass);
			// TODO: Check for same user 
			if (parse_int(str)) {
				cli->dcwtime = atoi(str);
				if (cli->dcwtime<0) cli->dcwtime=0;
				else if (cli->dcwtime>300) cli->dcwtime=300; // default 500
			}

			parse_spaces();
			if (*iparser=='{') { // Get Ports List & User Info
				iparser++;
				parse_spaces();
				if ( (*iparser>='0')&&(*iparser<='9') ) {
					i = 0;
					while (i<MAX_CSPORTS) {
						if ( parse_int(str)>0 ) {
							// check for port
							cli->csport[i] = atoi(str);
							int j;
							for (j=0; j<i; j++) if ( cli->csport[j] && (cli->csport[j]==cli->csport[i]) ) break;
							if (j>=i) i++; else cli->csport[i] = 0;
						}
						else break;
						parse_spaces();
						if (*iparser==',') iparser++;
					}
				}
				else {
					while (1) {
						parse_spaces();
						if (*iparser=='}') break;
						// NAME1=value1; Name2=Value2 ... }
						parse_value(str,"\r\n\t =");
						//printf(" NAME: '%s'\n", str);
						// '='
						parse_spaces();
						if (*iparser!='=') break;
						iparser++;
						// Value
						// Check for PREDEFINED names
						if (!strcmp(str,"profiles")) {
							i = 0;
							while (i<MAX_CSPORTS) {
								if ( parse_int(str)>0 ) {
									// check for port
									int n = cli->csport[i] = atoi(str);
									int j;
									for (j=0; j<i; j++) if ( cli->csport[j] && (cli->csport[j]==n) ) break;
									if (j>=i) { 
										cli->csport[i] = n;
										i++;
									}
								}
								else break;
								parse_spaces();
								if (*iparser==',') iparser++;
							}
						}
						else {
							struct client_info_data *info = malloc( sizeof(struct client_info_data) );
							strcpy(info->name, str);
							parse_spaces();
							parse_value(str,"\r\n;}");
							for(i=strlen(str)-1; ( (str[i]==' ')||(str[i]=='\t') ) ; i--) str[i] = 0; // Remove spaces
							//printf(" VALUE: '%s'\n", str);
							strcpy(info->value, str);
							info->next = cli->info;
							cli->info = info;
						}
						parse_spaces();
						if (*iparser==';') iparser++; else break;
					}
					// Set Info Data
					struct client_info_data *info = cli->info;
					while (info) {
						strcpy(str,info->name);
						uppercase(str);
						if (!strcmp(str,"NAME")) cli->realname = info->value;
						else if (!strcmp(str,"ENDDATE")) {
						    strptime(  info->value, "%Y-%m-%d", &cli->enddate);
 						}
						else if (!strcmp(str,"HOST")) {
							cli->host = add_host(cfg,info->value);
 						}
						info = info->next;
					}
				}	
				if (*iparser!='}') debugf(" config(%d,%d): '}' expected\n",nbline,iparser-currentline);
			}

			cli->handle = -1;
			cli->next = cfg->mgcamd.client;
			cfg->mgcamd.client = cli;
		}
		else if (!strcmp(str,"MGCAMD")) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"PORT")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cfg->mgcamd.port = atoi(str);
			}
			else if (!strcmp(str,"KEY")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				for(i=0; i<14; i++)
					if ( parse_hex(str)!=2 ) {
						memset( cfg->mgcamd.key, 0, 16 );
						debugf(" config(%d,%d): Error reading DES-KEY\n",nbline,iparser-currentline);
						break;
					} 
					else {
						cfg->mgcamd.key[i] = hex2int(str);
					}
			}
			else if (!strcmp(str,"PROFILES")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				for(i=0;i<MAX_CSPORTS;i++) {
					if ( parse_int(str)>0 ) {
						cfg->mgcamd.csport[i] = atoi(str);
					}
					else break;
					parse_spaces();
					if (*iparser==',') iparser++;
				}
			}
		}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

		else if (!strcmp(str,"CACHE")) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"PORT")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				parse_spaces();
				cfg->cacheport = atoi(str);
			}
			else if (!strcmp(str,"PEER")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				// must check for reuse of same user
				struct cs_cachepeer_data *peer = malloc( sizeof(struct cs_cachepeer_data) );
				memset(peer,0,sizeof(struct cs_cachepeer_data) );
				peer->fsendrep = 1;
				parse_str(str);
				peer->host = add_host(cfg, str);
				parse_int(str);
				peer->port = atoi(str);
				if (parse_bin(str)) {
					peer->fblock0onid = str[0]=='1';
					if (parse_int(str)) peer->fsendrep = atoi(str);
				}
				parse_spaces();
				if (*iparser=='{') { // Get Ports List
					iparser++;
					for(i=0;i<MAX_CSPORTS;i++) {
						if ( parse_int(str)>0 ) {
							peer->csport[i] = atoi(str);
						}
						else break;
						parse_spaces();
						if (*iparser==',') iparser++;
					}
					peer->csport[i] = 0;
					parse_spaces();
					if (*iparser!='}') debugf(" config(%d,%d): '}' expected\n",nbline,iparser-currentline);
				}
				peer->next = cfg->cachepeer;
				cfg->cachepeer = peer;
				peer->reqnb=0;
				peer->reqnb=0;
				peer->hitnb=0;
			}
			else if (!strcmp(str,"TIMEOUT")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				if (cardserver) {
					cardserver->cachetimeout = atoi(str);
					if (cardserver->cachetimeout<0) cardserver->cachetimeout=0;
					else if (cardserver->cachetimeout>3000) cardserver->cachetimeout=3000; // Default 
				}
				else {
					defaultcs.cachetimeout = atoi(str);
					if (defaultcs.cachetimeout<0) defaultcs.cachetimeout=0;
					else if (defaultcs.cachetimeout>3000) defaultcs.cachetimeout=3000; // Default 
				}
			}
			else if (!strcmp(str,"TRACKER")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue; 
				} else iparser++;
				if (parse_bin(str)) cfg->cache.trackermode = str[0]=='1';
			}
		}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

		else if (!strcmp(str,"CAID")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip CAID, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			parse_hex(str);
			cardserver->card.caid = hex2int( str );
			//debugf(" *caid %04X\n",cardserver->card.caid);
		}
		else if (!strcmp(str,"PROVIDERS")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip PROVIDERS, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			cardserver->card.nbprov = 0;
			for(i=0;i<16;i++) {
				if ( parse_hex(str)>0 ) {
					cardserver->card.prov[i] = hex2int( str );
					cardserver->card.nbprov++;
				}
				else break;
				parse_spaces();
				if (*iparser==',') iparser++;
			}
			//for(i=0;i<cardserver->card.nbprov;i++) debugf(" *provider %d = %06X\n",i,cardserver->card.prov[i]); 
		}

		else if (!strcmp(str,"USER")) {
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;

			if (cardserver) {
				// must check for reuse of same user
				usr = malloc( sizeof(struct cs_client_data) );
				memset(usr,0,sizeof(struct cs_client_data) );
				usr->next = cardserver->client;
				cardserver->client = usr;
				usr->handle = INVALID_SOCKET;
				usr->flags |= FLAGCLI_CONFIG;
				parse_str(&usr->user[0]);
				parse_str(&usr->pass[0]);
				// options
				usr->dcwtime = cardserver->dcwtime;
				usr->dcwtimeout = cardserver->dcwtimeout;
				// USER: name pass dcwtime dcwtimeout { CAID:PROVIDER1,PROVIDER2,... } 

				//dcwtime
				if (parse_int(str)) {
					usr->dcwtime = atoi( str ); 
					if (usr->dcwtime<0) usr->dcwtime=0;
					else if (usr->dcwtime>500) usr->dcwtime=500;
					//dcwtimeout
					if (parse_int(str)) {
						usr->dcwtimeout = atoi( str ); 
						if (usr->dcwtimeout>10000) usr->dcwtimeout=10000;
					}
				}
				if ( parse_expect('{') ) { // Read Caid Providers
					if (parse_hex(str)) {
						usr->card.caid = hex2int(str);
						if (!parse_expect(':')) {
							usr->card.caid = 0;
							debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
							continue;
						}
						usr->card.nbprov = 0;
						for(i=0;i<16;i++) {
							if ( parse_hex(str)>0 ) {
								usr->card.prov[i] = hex2int( str );
								usr->card.nbprov++;
							}
							else break;
							parse_spaces(); if (*iparser==',') iparser++;
						}
					}
					if (!parse_expect('}')) {
						usr->card.caid = 0;
						debugf(" config(%d,%d): '}' expected\n",nbline,iparser-currentline);
						continue;
					}
				}
			}
			else { // global user
				struct global_user_data *gl = malloc( sizeof(struct global_user_data) );
				memset(gl,0,sizeof(struct global_user_data) );
				gl->next = guser;
				guser = gl;
				parse_str(&gl->user[0]);
				parse_str(&gl->pass[0]);
				// user: username pass { csports }
				parse_spaces();
				if (*iparser=='{') { // Get Ports List
					iparser++;
					for(i=0;i<MAX_CSPORTS;i++) {
						if ( parse_int(str)>0 ) {
							gl->csport[i] = atoi(str);
						}
						else break;
						parse_spaces();
						if (*iparser==',') iparser++;
					}
					gl->csport[i] = 0;
					parse_spaces();
					if (*iparser!='}') debugf(" config(%d,%d): '}' expected\n",nbline,iparser-currentline);
				}
			}
		}

		else if (!strcmp(str,"DCW")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip DCW, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"TIMEOUT")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue; 
				} else iparser++;
				parse_int(str);
				parse_spaces();
				cardserver->dcwtimeout = atoi(str);
				if (cardserver->dcwtimeout<300) cardserver->dcwtimeout=300;
				else if (cardserver->dcwtimeout>2500) cardserver->dcwtimeout=2500; // default 9999
			}
			else if (!strcmp(str,"MAXFAILED")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				parse_spaces();
				cardserver->maxfailedecm = atoi(str);
				if (cardserver->maxfailedecm<0) cardserver->maxfailedecm=0;
				else if (cardserver->maxfailedecm>100) cardserver->maxfailedecm=100;
			}
			else if ( (!strcmp(str,"TIME"))||(!strcmp(str,"MINTIME")) ) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				parse_spaces();
				cardserver->dcwtime = atoi(str);
				if (cardserver->dcwtime<0) cardserver->dcwtime=0;
				else if (cardserver->dcwtime>200) cardserver->dcwtime=200; // default 500
			}

#ifdef CHECK_NEXTDCW
			else if (!strcmp(str,"CHECK")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				if (parse_bin(str)) cardserver->checkdcw = str[0]=='1';
			}
#endif
		}

		else if (!strcmp(str,"SERVER")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip SERVER, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"TIMEOUT")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cardserver->cstimeout = atoi(str);
				if ( (cardserver->cstimeout<300)||(cardserver->cstimeout>10000) ) cardserver->cstimeout=300;
			}
			else if (!strcmp(str,"MAX")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cardserver->csmax = atoi(str);
				if (cardserver->csmax<0) cardserver->csmax=0;
				else if (cardserver->csmax>15) cardserver->csmax=15; // default 10
			}
			else if (!strcmp(str,"INTERVAL")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cardserver->csinterval = atoi(str);
				if (cardserver->csinterval<100) cardserver->csinterval=100;
				else if (cardserver->csinterval>2000) cardserver->csinterval=2000; // default 3000
			}
			else if (!strcmp(str,"VALIDECMTIME")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cardserver->csvalidecmtime = atoi(str);
				if (cardserver->csvalidecmtime>3000) cardserver->csvalidecmtime=3000; // default 5000
			}
			else if (!strcmp(str,"FIRST")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cardserver->csfirst = atoi(str);
				if (cardserver->csfirst<0) cardserver->csfirst=0;
				else if (cardserver->csfirst>5) cardserver->csfirst=5;
			}
			else debugf(" config(%d,%d): cardserver variable expected\n",nbline,iparser-currentline);
		}

		else if ( !strcmp(str,"RETRY") ) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip RETRY, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"NEWCAMD")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cardserver->csretry = atoi(str);
				if (cardserver->csretry<0) cardserver->csretry=0;
				else if (cardserver->csretry>3) cardserver->csretry=3;
			}
			else if (!strcmp(str,"CCCAM")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cardserver->ccretry = atoi(str);
				if (cardserver->ccretry<0) cardserver->ccretry=0;
				else if (cardserver->ccretry>10) cardserver->ccretry=10;
			}
			else debugf(" config(%d,%d): cardserver variable expected\n",nbline,iparser-currentline);
		}

		else if (!strcmp(str,"PORT")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip PORT, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			parse_spaces();
			if (*iparser=='+') {
				iparser++;
				//cardserver->port = defaultcs.port;
				//defaultcs.port++;
			}
			else {
				parse_int(str);
				//debugf("port:%s\n", str);
				parse_spaces();
				cardserver->port = atoi(str);
				defaultcs.port = cardserver->port;
				if (defaultcs.port) defaultcs.port++;
			}			
		}
#ifdef RADEGAST_SRV
		else if (!strcmp(str,"RADEGAST")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip PORT, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"PORT")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_int(str);
				cardserver->rdgd.port = atoi(str);
			}
		}
#endif
		else if (!strcmp(str,"ONID")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip ONID, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			parse_hex(str);
			cardserver->onid = hex2int(str);
		}

		else if (!strcmp(str,"USECACHE")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip USECACHE, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			parse_hex(str);
			cardserver->usecache = hex2int(str);
		}

		else if ( !strcmp(str,"ACCEPT") ) {
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"NULL")) {
				parse_name(str);
				uppercase(str);
				if (!strcmp(str,"SID")) {
					if (!cardserver) {
						debugf(" config(%d,%d): Skip ACCEPT, undefined profile\n",nbline,iparser-currentline);
						continue;
					}
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					if (parse_bin(str)) cardserver->faccept0sid = str[0]=='1';
				}
				else if (!strcmp(str,"CAID")) {
					if (!cardserver) {
						debugf(" config(%d,%d): Skip ACCEPT, undefined profile\n",nbline,iparser-currentline);
						continue;
					}
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					if (parse_bin(str)) cardserver->faccept0caid = str[0]=='1';
				}
				else if (!strcmp(str,"PROVIDER")) {
					if (!cardserver) {
						debugf(" config(%d,%d): Skip ACCEPT, undefined profile\n",nbline,iparser-currentline);
						continue;
					}
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					if (parse_bin(str)) cardserver->faccept0provider = str[0]=='1';
				}

				else if (!strcmp(str,"ONID")) {
					parse_spaces();
					if (*iparser!=':') {
						debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
						continue;
					} else iparser++;
					if (parse_bin(str)) cfg->faccept0onid = str[0]=='1';
				}
			}
		}

		else if (!strcmp(str,"DISABLE")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip DISABLE, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"CCCAM")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				if (parse_bin(str)) cardserver->fallowcccam = str[0]=='0';
			}
			else if (!strcmp(str,"NEWCAMD")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				if (parse_bin(str)) cardserver->fallownewcamd = str[0]=='0';
			}
			else if (!strcmp(str,"RADEGAST")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				if (parse_bin(str)) cardserver->fallowradegast = str[0]=='0';
			}
			else if (!strcmp(str,"CACHE")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				parse_bin(str);
				if (parse_bin(str)) cardserver->usecache = str[0]=='0';
			}
		}

		else if (!strcmp(str,"SID")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip SID, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_name(str);
			uppercase(str);
			if (!strcmp(str,"LIST")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				if (cardserver->sids) {
					debugf(" config(%d,%d): sids already defined!!!\n",nbline,iparser-currentline);
					continue;
				}
				int count = 0;
				unsigned short *sids;
				sids = malloc ( sizeof(unsigned short) * MAX_SIDS );
				memset( sids, 0, sizeof(unsigned short) * MAX_SIDS );
				cardserver->sids = sids;
				while ( parse_hex(str)>0 ) {
					sids[count] = hex2int(str);
					//debugf("sid%d %s\n", count, str);
					count++;
					if (count>=MAX_SIDS) {
						debugf(" config(%d,%d): too many sids...\n",nbline,iparser-currentline);
						break;
					}
					parse_spaces();
					if (*iparser==',') iparser++;
				}
				sids[count] = 0;
			}
	
			else if (!strcmp(str,"DENYLIST")) {
				parse_spaces();
				if (*iparser!=':') {
					debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
					continue;
				} else iparser++;
				if (parse_bin(str)) cardserver->isdeniedsids = str[0]=='1';
			}
		}

		else if (!strcmp(str,"KEY")) {
			if (!cardserver) {
				debugf(" config(%d,%d): Skip KEY, undefined profile\n",nbline,iparser-currentline);
				continue;
			}
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			for(i=0; i<14; i++)
				if ( parse_hex(str)!=2 ) {
					memset( cardserver->key, 0, 16 );
					debugf(" config(%d,%d): Error reading DES-KEY\n",nbline,iparser-currentline);
					break;
				} 
				else {
					cardserver->key[i] = hex2int(str);
				}
		}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

		else if (!strcmp(str,"TRACE")) {
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): ':' expected\n",nbline,iparser-currentline);
				continue;
			} else iparser++;
			
			if ( parse_bin(str)!=1 ) {
				debugf(" config(%d,%d): Error reading Trace\n",nbline,iparser-currentline);
				break;
			} 
			if (str[0]=='0') flag_debugtrace = 0;
			else if (str[0]=='1') {
				parse_str(trace.host);
				parse_int(str);
				trace.port = atoi(str);
				trace.ip = hostname2ip(trace.host);
				memset( &trace.addr, 0, sizeof(trace.addr) );
				trace.addr.sin_family = AF_INET;
				trace.addr.sin_addr.s_addr = trace.ip;
				trace.addr.sin_port = htons(trace.port);
				flag_debugtrace = 0;
				if (trace.port && trace.ip) {
					if (trace.sock<=0) trace.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
					if (trace.sock>0) flag_debugtrace = 1;
				}
			}
			else {
				debugf(" config(%d,%d): expected 0/1\n",nbline,iparser-currentline);
				break;
			}
		}
	}
	fclose(fhandle);

#ifdef FREECCCAM_SRV
	//Create clients
	for(i=0; i< cfg->freecccam.maxusers; i++) {
		struct cc_client_data *cli = malloc( sizeof(struct cc_client_data) );
		memset(cli, 0, sizeof(struct cc_client_data) );
		// init Default
		cli->handle = INVALID_SOCKET;
		cli->dcwtime = cfg->freecccam.dcwtime;
		cli->dnhops = 0;
		cli->uphops = 0;
		cli->next = cfg->freecccam.client;
		cfg->freecccam.client = cli;
	}
#endif

// ADD GLOBAL USERS TO PROFILES
	struct global_user_data *gl = guser;
	while(gl) {
		struct cardserver_data *cs = cfg->cardserver;
		while(cs) {
			int i;
			for(i=0; i<MAX_CSPORTS; i++ ) {
				if (!gl->csport[i]) break;
				if (gl->csport[i]==cs->port) {
					i=0;
					break;
				}
			}
			if (i==0) { // ADD TO PROFILE
				// must check for reuse of same user
				struct cs_client_data *usr = malloc( sizeof(struct cs_client_data) );
				memset(usr,0,sizeof(struct cs_client_data) );
				usr->next = cs->client;
				cs->client = usr;
				usr->handle = INVALID_SOCKET;
				usr->flags |= FLAGCLI_CONFIG;
				strcpy(usr->user, gl->user);
				strcpy(usr->pass, gl->pass);
				// options
				usr->dcwtime = cs->dcwtime;
				usr->dcwtimeout = cs->dcwtimeout;
			}
			cs = cs->next;
		}
		struct global_user_data *oldgl = gl;
		gl = gl->next;
		free( oldgl );
	}

	return 0;
}


int read_chinfo( struct program_data *prg )
{
	FILE *fhandle;
	char str[128];
	int nbline = 0;
	char fname[512];

	uint16 caid,sid;
	uint32 prov;
	int chncount = 0;

	sprintf(fname, "/var/etc/newbox.channelinfo");
	// Open Config file
	fhandle = fopen(fname,"rt");
	if (fhandle==0) {
		//debugf(" file not found '%s'\n",fname);
		return -1;
	} else debugf(" config: parsing file '%s'\n",fname);

	while (!feof(fhandle))
	{
		if ( !fgets(currentline, 10239, fhandle) ) break;
		iparser = &currentline[0];
		nbline++;
		parse_spaces();

		if ( ((*iparser>='0')&&(*iparser<='9')) || ((*iparser>='a')&&(*iparser<='f')) || ((*iparser>='A')&&(*iparser<='f')) ) {
			parse_hex(str);
			caid = hex2int(str);
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): caid ':' expected\n",nbline,iparser-currentline);
				break;
			} else iparser++;
			parse_hex(str);
			prov = hex2int(str);
			parse_spaces();
			if (*iparser!=':') {
				debugf(" config(%d,%d): provid ':' expected\n",nbline,iparser-currentline);
				break;
			} else iparser++;
			parse_hex(str);
			sid = hex2int(str);
			parse_spaces();
			if (*iparser=='"') {
				iparser++;
				char *end = iparser;
				while ( (*end!='"')&&(*end!='\n')&&(*end!='\r')&&(*end!=0) ) end++;
				if (end-iparser) {
					*end = 0;
					//strcpy(str,iparser);
					//debugf("%04x:%06x:%04x '%s'\n",caid,prov,sid,iparser);
					struct chninfo_data *chn = malloc( sizeof(struct chninfo_data) + strlen(iparser) + 1 );
					chn->sid = sid;
					chn->caid = caid;
					chn->prov = prov;
					strcpy( chn->name, iparser );
					chn->next = prg->chninfo;
					prg->chninfo = chn;
					chncount++;
				}
			}
		}
	}
	fclose(fhandle);
	debugf(" config: reading %d channels.\n", chncount);
	return 0;
}

int read_badcw( struct config_data *cfg )
{
	FILE *fhandle;
	char fname[512];
	int chncount = 0;
	
	sprintf(fname, "/var/etc/badcw.cfg");
	// Open Config file
	fhandle = fopen(fname,"rt");
  if (fhandle==0) {
    debugf(" config: parsing file '%s'\n",fname);
    return -1;
  }
	fclose(fhandle);
	debugf(" config: reading %d badcw.cfg.\n", chncount);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int cardcmp( struct cs_card_data *card1, struct cs_card_data *card2)
{
	if (card1->caid!=card2->caid) return 1;
	if (card1->nbprov!=card2->nbprov) return 1;
	int i;
	for(i=0; i<card1->nbprov; i++) if (card1->prov[i]!=card2->prov[i]) return 1;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
void reread_config( struct config_data *cfg )
{
	struct config_data newcfg;
	int reinitcccam = 0; // Reconnect all cccam client if card are modified

	init_config(&newcfg);
	read_config( &newcfg );

	if (!cfg) return;

	//Copy ID's from old cfg, to get new id's for new data
	newcfg.clientid = cfg->clientid;
	newcfg.serverid = cfg->serverid;
	newcfg.cardserverid = cfg->cardserverid;
	newcfg.cachepeerid = cfg->cachepeerid;
	newcfg.cccamclientid = cfg->cccamclientid;
	newcfg.mgcamdclientid = cfg->mgcamdclientid;

	//## Cache
	while(cfg->cachepeer) {
		struct cs_cachepeer_data *peer = cfg->cachepeer;
		cfg->cachepeer = cfg->cachepeer->next;
		struct cs_cachepeer_data *newpeer = newcfg.cachepeer;
		while(newpeer) {
			if ( !strcmp(newpeer->host->name,peer->host->name) && (newpeer->port==peer->port) ) break;
			newpeer = newpeer->next;
		}
		if (newpeer) { // Old Peer is found -> copy data
			memcpy( &newpeer->id , &peer->id , sizeof(struct cs_cachepeer_data)-offsetof(struct cs_cachepeer_data,id) );
		}
		else if (peer->outsock) close(peer->outsock);
		free(peer);
	}
	newcfg.cachehits = cfg->cachehits;
	newcfg.cacheihits = cfg->cacheihits;
	newcfg.cachereq = cfg->cachereq;
	newcfg.cacherep = cfg->cacherep;

	if (newcfg.cacheport!=cfg->cacheport) { // Close Old and Open New
		if (cfg->cachesock>0) close(cfg->cachesock);
		newcfg.cachesock = INVALID_SOCKET;
	}
	else newcfg.cachesock = cfg->cachesock;

#ifdef HTTP_SRV
	// Check HTTP Port/Socket
	if (newcfg.http.port!=cfg->http.port) {
		if (cfg->http.handle>0) close(cfg->http.handle);
		newcfg.http.handle = INVALID_SOCKET;
	} 
	else newcfg.http.handle = cfg->http.handle;
#endif


#ifdef CCCAM_SRV
	// Check Port/Socket
	if (newcfg.cccam.port!=cfg->cccam.port)
		if (cfg->cccam.handle!=INVALID_SOCKET) {
			close(cfg->cccam.handle);
			cfg->cccam.handle = INVALID_SOCKET;
		}
	// Check for CCcam Clients
	newcfg.cccam.handle = cfg->cccam.handle;
	struct cc_client_data *cccli = cfg->cccam.client;
	cfg->cccam.client = NULL;
	while(cccli) {
		struct cc_client_data *tmpcccli = cccli->next;
		struct cc_client_data *newcccli = newcfg.cccam.client;
		while(newcccli) {
			if ( !strcmp(newcccli->user,cccli->user) ) break;
			newcccli = newcccli->next;
		}
		if (newcccli) { // Old Peer is found -> copy data
			newcccli->id = cccli->id;
			if ( memcmp(cccli->csport,newcccli->csport,sizeof(cccli->csport))
				|| (cccli->dnhops!=newcccli->dnhops)
				|| (newcfg.cccam.port!=cfg->cccam.port) ) cc_disconnect_cli(cccli);
			// Copy runtime data to new client data
			memcpy( &newcccli->ip , &cccli->ip , sizeof(struct cc_client_data)-offsetof(struct cc_client_data,ip) );
		}
		else {
			if (cccli->handle!=INVALID_SOCKET)  cc_disconnect_cli(cccli);
		}
		// Free Client
		while (cccli->info) {
			struct client_info_data *info = cccli->info;
			cccli->info = info->next;
			free(info);
		}
		free(cccli);
		cccli = tmpcccli;
	}
#endif

#ifdef FREECCCAM_SRV
	// Check Port/Socket
	if (newcfg.freecccam.port!=cfg->freecccam.port)
		if (cfg->freecccam.handle!=INVALID_SOCKET) {
			close(cfg->freecccam.handle);
			cfg->freecccam.handle = INVALID_SOCKET;
		}
	newcfg.freecccam.handle = cfg->freecccam.handle;
	// Clear old clients
	struct cc_client_data *freecccli = cfg->freecccam.client;
	while(freecccli) {
		struct cc_client_data *tmpfreecccli = freecccli->next;
		if (freecccli->handle!=INVALID_SOCKET)  cc_disconnect_cli(freecccli);
		free(freecccli);
		freecccli = tmpfreecccli;
	}
#endif



#ifdef MGCAMD_SRV
	// Check Port/Socket
	if (newcfg.mgcamd.port!=cfg->mgcamd.port)
		if (cfg->mgcamd.handle!=INVALID_SOCKET) {
			close(cfg->mgcamd.handle);
			cfg->mgcamd.handle = INVALID_SOCKET;
		}
	newcfg.mgcamd.handle = cfg->mgcamd.handle;

	struct mg_client_data *mgcli = cfg->mgcamd.client;
	while(mgcli) {
		struct mg_client_data *tmpmgcli = mgcli->next;
		struct mg_client_data *newmgcli = newcfg.mgcamd.client;
		while(newmgcli) {
			if ( !strcmp(newmgcli->user,mgcli->user) ) break;
			newmgcli = newmgcli->next;
		}
		if (newmgcli) { // Old Peer is found -> copy data
			newmgcli->id = mgcli->id;
			if ( memcmp(mgcli->csport,newmgcli->csport,sizeof(mgcli->csport))
				|| (newcfg.mgcamd.port!=cfg->mgcamd.port) ) mg_disconnect_cli(mgcli);
			// Copy runtime data to new client data
			memcpy( &newmgcli->ip , &mgcli->ip , sizeof(struct mg_client_data)-offsetof(struct mg_client_data,ip) );
		}
		else {
			if (mgcli->handle!=INVALID_SOCKET)  mg_disconnect_cli(mgcli);
		}	
		// Free Client Data
		while (mgcli->info) {
			struct client_info_data *info = mgcli->info;
			mgcli->info = info->next;
			free(info);
		}
		free(mgcli);
		mgcli = tmpmgcli;
	}
#endif


	// Newcamd Server
	while(cfg->cardserver) {
		struct cardserver_data *cs = cfg->cardserver;
		cfg->cardserver = cfg->cardserver->next;
		struct cardserver_data *newcs = newcfg.cardserver;
		while(newcs) {
			if ( !strcmp(newcs->name,cs->name) ) break;
			newcs = newcs->next;
		}
		if (newcs) {
			// Get All Runtime data from old struct
			newcs->id = cs->id;
			newcs->ecmaccepted = cs->ecmaccepted;
			newcs->ecmok = cs->ecmok;
			newcs->ecmoktime = cs->ecmoktime;
			newcs->ecmdenied = cs->ecmdenied;
			newcs->cachehits = cs->cachehits;
			newcs->cacheihits = cs->cacheihits;

			memcpy( newcs->ttime, cs->ttime, sizeof(cs->ttime) );
			memcpy( newcs->ttimecache, cs->ttimecache, sizeof(cs->ttimecache) );
			memcpy( newcs->ttimecards, cs->ttimecards, sizeof(cs->ttimecards) );
			memcpy( newcs->ttimeclients, cs->ttimeclients, sizeof(cs->ttimeclients) );

			// check for newcamd port modification -> close old and create new socket(in check config)
			if (newcs->port!=cs->port) {
				if (cs->handle!=INVALID_SOCKET) close(cs->handle);
			}
			else {
				newcs->handle = cs->handle;
			}
			// Disconnect Clients if (different port|different key|different card)
			if ( (newcs->port!=cs->port)||memcmp(newcs->key,cs->key,14)||cardcmp( &newcs->card, &cs->card) ) {
				//Disconnect all clients & remove them
				while(cs->client) {
					struct cs_client_data *cli = cs->client;
					cs->client = cs->client->next;
					if (cli->handle!=INVALID_SOCKET) close(cli->handle);
					free(cli);
				}
			}
			else {
				// Check Clients
				while(cs->client) {
					struct cs_client_data *cli = cs->client;
					cs->client = cs->client->next;
					// search for old client into new client list
					struct cs_client_data *newcli = newcs->client;
					while(newcli) {
						if ( !strcmp(newcli->user,cli->user) ) break;
						newcli = newcli->next;
					}
					// Found ?
					if (newcli) {
						newcli->id = cli->id; // Same id
						if ( strcmp(newcli->pass,cli->pass) ) { // Disconnect Clients if (Different Pass)
							if (cli->handle!=INVALID_SOCKET) close(cli->handle);
						}
						else {
							// Copy runtime data to new client data
							memcpy( &newcli->ip , &cli->ip , sizeof(struct cs_client_data)-offsetof(struct cs_client_data,ip) );
						}
					}
					else { // disconnect
						if (cli->handle!=INVALID_SOCKET) close(cli->handle);
					}
					// Free Client Data
					while (cli->info) {
						struct client_info_data *info = cli->info;
						cli->info = info->next;
						free(info);
					}
					free(cli);
				}
			}
#ifdef RADEGAST_SRV
			// check for radegast port modification -> close old and create new socket(in check config)
			if (newcs->rdgd.port!=cs->rdgd.port) {
				if (cs->rdgd.handle!=INVALID_SOCKET) close(cs->rdgd.handle);
			}
			else {
				newcs->rdgd.handle = cs->rdgd.handle;
			}
			// Disconnect Clients if different port
			if (newcs->rdgd.port!=cs->rdgd.port) {
				//Disconnect all clients & remove them
				while(cs->rdgd.client) {
					struct rdgd_client_data *rdgdcli = cs->rdgd.client;
					cs->rdgd.client = cs->rdgd.client->next;
					if (rdgdcli->handle!=INVALID_SOCKET) close(rdgdcli->handle);
					free(rdgdcli);
				}
			}
			else newcs->rdgd.client = cs->rdgd.client;
#endif
		}
		else {
			// Not found -> Delete Cardersver Data
			reinitcccam = 1;
			if (cs->handle!=INVALID_SOCKET) close(cs->handle);

			while(cs->client) {
				struct cs_client_data *cli = cs->client;
				cs->client = cs->client->next;
				if (cli->handle!=INVALID_SOCKET) close(cli->handle);
				free(cli);
			}
		}
		if (cs->sids) free(cs->sids);
		free(cs);
	}


	// Servers
	while(cfg->server) {
		struct cs_server_data *srv = cfg->server;
		cfg->server = cfg->server->next;
		struct cs_server_data *newsrv = newcfg.server;
		while(newsrv) {
			if ( !strcmp(newsrv->host->name,srv->host->name) && (newsrv->port==srv->port) && !strcmp(newsrv->user,srv->user) ) break;
			newsrv = newsrv->next;
		}
		if (newsrv) { // Found
			newsrv->id = srv->id; // Same id
			if ( strcmp(newsrv->pass,srv->pass)
				||(newsrv->type!=srv->type)
#ifdef CCCAM_CLI
				||( (newsrv->type==TYPE_CCCAM)&&(srv->type==TYPE_CCCAM)&&(newsrv->uphops!=srv->uphops) )
#endif
			) { // Different pass,type -> Disconnect
				//Free Cards
				while (srv->card) {
					struct cs_card_data *card = srv->card;
					srv->card = srv->card->next;
					//Free SIDs
					while (card->sids) {
						struct sid_data *sid = card->sids;
						card->sids = card->sids->next;
						free(sid);
					}
					free(card);
				}
				// Close handle
				if (srv->handle!=INVALID_SOCKET) close(srv->handle);
			}
			else { // Copy runtime data from old pointer
				srv->next = newsrv->next;
				srv->host = newsrv->host;
				memcpy( srv->csport, newsrv->csport, sizeof(newsrv->csport));
				memcpy( newsrv, srv, sizeof(struct cs_server_data) );
			}
		}
		else { // Delete
			//Free Cards
			while (srv->card) {
				struct cs_card_data *card = srv->card;
				srv->card = srv->card->next;
				//Free SIDs
				while (card->sids) {
					struct sid_data *sid = card->sids;
					card->sids = card->sids->next;
					free(sid);
				}
				free(card);
			}
			// Close handle
			if (srv->handle!=INVALID_SOCKET) close(srv->handle);
		}
		// Remove
		free(srv);
	}

	//## Host Data
	struct host_data *newhost = newcfg.host;
	while (newhost) {
		struct host_data *oldhost = cfg->host;
		while (oldhost) {
			if (!strcmp(newhost->name,oldhost->name)) {
				newhost->ip = oldhost->ip;
				newhost->checkiptime = oldhost->checkiptime;
				newhost->clip = oldhost->clip;
				break;
			}
			oldhost = oldhost->next;
		}
		newhost = newhost->next;
	}
	free_allhosts( cfg );

	// DAB-DCW
	while(cfg->bad_dcw) {
		struct dcw_data *dcw = cfg->bad_dcw;
		cfg->bad_dcw = cfg->bad_dcw->next;
		free(dcw);
	}

	/////////////////
	// COPY CONFIG
	memcpy( cfg, &newcfg, sizeof(struct config_data) );
}

// Open ports & set id's
int check_config(struct config_data *cfg)
{

	// Open ports for new profiles
	struct cardserver_data *cs = cfg->cardserver;
	while (cs) {
		if (cs->handle<=0) {
			if ( (cs->port<1024)||(cs->port>0xffff) ) {
				debugf(" [%s] Newcamd Server: invalid port value (%d)\n", cs->name, cs->port);
				cs->handle = INVALID_SOCKET;
			}
			else if ( (cs->handle=CreateServerSockTcp_nonb(cs->port)) == -1) {
				debugf(" [%s] Newcamd Server: bind port failed (%d)\n", cs->name, cs->port);
				cs->handle = INVALID_SOCKET;
			}
			else debugf(" [%s] Newcamd Server started on port %d\n",cs->name,cs->port);
		}
#ifdef RADEGAST_SRV
		if (cs->rdgd.handle<=0) {
			if ( (cs->rdgd.port<1024)||(cs->rdgd.port>0xffff) ) {
				//debugf(" CardServer '%s': invalid port value (%d)\n", cs->name, cs->port);
				cs->rdgd.handle = INVALID_SOCKET;
			}
			else if ( (cs->rdgd.handle=CreateServerSockTcp_nonb(cs->rdgd.port)) == -1) {
				debugf(" [%s] Radegast Server: bind port failed (%d)\n", cs->name, cs->rdgd.port);
				cs->rdgd.handle = INVALID_SOCKET;
			}
			else debugf(" [%s] Radegast Server started on port %d\n",cs->name,cs->rdgd.port);
		}
#endif
		cs = cs->next;
	}

	// HTTP Port
#ifdef HTTP_SRV
	if (cfg->http.handle<=0) {
		if ( (cfg->http.port<1024)||(cfg->http.port>0xffff) ) {
			debugf(" HTTP Server: invalid port value (%d)\n", cfg->http.port);
			cfg->http.handle = INVALID_SOCKET;
		}
		else if ( (cfg->http.handle=CreateServerSockTcp(cfg->http.port)) == -1) {
			debugf(" HTTP Server: bind port failed (%d)\n", cfg->http.port);
			cfg->http.handle = INVALID_SOCKET;
		}
		else debugf(" HTTP server started on port %d\n",cfg->http.port);
	}
#endif

#ifdef CCCAM_SRV
	// Open port for CCcam server
	if (cfg->cccam.handle<=0) {
		if ( (cfg->cccam.port<1024)||(cfg->cccam.port>0xffff) ) {
			debugf(" CCcam Server: invalid port value (%d)\n", cfg->cccam.port);
			cfg->cccam.handle = INVALID_SOCKET;
		}
		else if ( (cfg->cccam.handle=CreateServerSockTcp_nonb(cfg->cccam.port)) == -1) {
			debugf(" CCcam Server: bind port failed (%d)\n", cfg->cccam.port);
			cfg->cccam.handle = INVALID_SOCKET;
		}
		else debugf(" CCcam server started on port %d (version: %s)\n",cfg->cccam.port,cfg->cccam.version);
	}
#endif

#ifdef FREECCCAM_SRV
	// Open port
	if (cfg->freecccam.handle<=0) {
		if ( (cfg->freecccam.port<1024)||(cfg->freecccam.port>0xffff) ) {
			debugf(" FreeCCcam Server: invalid port value (%d)\n", cfg->freecccam.port);
			cfg->freecccam.handle = INVALID_SOCKET;
		}
		else if ( (cfg->freecccam.handle=CreateServerSockTcp_nonb(cfg->freecccam.port)) == -1) {
			debugf(" FreeCCcam Server: bind port failed (%d)\n", cfg->freecccam.port);
			cfg->freecccam.handle = INVALID_SOCKET;
		}
		else debugf(" FreeCCcam server started on port %d\n",cfg->freecccam.port);
	}
#endif

#ifdef MGCAMD_SRV
	// Open port for MGcamd server
	if (cfg->mgcamd.handle<=0) {
		if ( (cfg->mgcamd.port<1024)||(cfg->mgcamd.port>0xffff) ) {
			debugf(" MGcamd Server: invalid port value (%d)\n", cfg->mgcamd.port);
			cfg->mgcamd.handle = INVALID_SOCKET;
		}
		else if ( (cfg->mgcamd.handle=CreateServerSockTcp_nonb(cfg->mgcamd.port)) == -1) {
			debugf(" MGcamd Server: bind port failed (%d)\n", cfg->mgcamd.port);
			cfg->mgcamd.handle = INVALID_SOCKET;
		}
		else debugf(" MGcamd server started on port %d\n",cfg->mgcamd.port);
	}
#endif

	// Open port for Cache
	if (cfg->cachesock<=0) {
		if ( (cfg->cacheport<1024)||(cfg->cacheport>0xffff) ) {
			debugf(" Cache Server: invalid port value (%d)\n", cfg->cacheport);
			cfg->cachesock = INVALID_SOCKET;
		}
		else if ( (cfg->cachesock=CreateServerSockUdp(cfg->cacheport)) == -1) {
			debugf(" Cache Server: bind port failed (%d)\n", cfg->cacheport);
			cfg->cachesock = INVALID_SOCKET;
		}
		else {
			debugf(" Cache server started on port %d\n",cfg->cacheport);
		}
	}
	// create cahcepeer socket
	struct cs_cachepeer_data *peer = cfg->cachepeer;
	while (peer) {
		if (peer->outsock<=0) peer->outsock = CreateClientSockUdp();
		peer = peer->next;
	}

////// SETUP ID'S

	//add servers id's
	struct cs_server_data *srv = cfg->server;
	while (srv) {
		if (!srv->id) {
			srv->id = cfg->serverid;
			cfg->serverid++;
		}
		srv = srv->next;
	}

	// add cardserver id's
	cs = cfg->cardserver;
	while(cs) {
		if (!cs->id) {
			cs->id = cfg->cardserverid;
			cfg->cardserverid++;

			struct cs_client_data *cli = cs->client;
			while (cli) {
				if (!cli->id) {
					cli->id = cfg->clientid;
					cfg->clientid++;
				}
				cli = cli->next;
			}
		}
		cs = cs->next;
	}

	// add CCcam Client id's
#ifdef CCCAM_SRV
	struct cc_client_data *cccli = cfg->cccam.client;
	while (cccli) {
		if (!cccli->id) {
			cccli->id = cfg->cccamclientid;
			cfg->cccamclientid++;
		}
		cccli = cccli->next;
	}
#endif

	// add MGCAMD Client id's
#ifdef MGCAMD_SRV
	struct mg_client_data *mgcli = cfg->mgcamd.client;
	while (mgcli) {
		if (!mgcli->id) {
			mgcli->id = cfg->mgcamdclientid;
			cfg->mgcamdclientid++;
		}
		mgcli = mgcli->next;
	}
#endif

	// add cahcepeer id's
//	struct cs_cachepeer_data *
	peer = cfg->cachepeer;
	while(peer) {
		if (!peer->id) {
			peer->id = cfg->cachepeerid;
			cfg->cachepeerid++;
		}
		peer = peer->next;
	}

	return 0;
}


// Close ports
int done_config(struct config_data *cfg)
{
	// Close Newcamd/Radegast Clients Connections & profiles ports
	struct cardserver_data *cs = cfg->cardserver;
	while (cs) {
		if (cs->handle>0) {
			close(cs->handle);
			struct cs_client_data *cscli = cs->client;
			while (cscli) {
				if (cscli->handle>0) close(cscli->handle);
				cscli = cscli->next;
			}
		}
#ifdef RADEGAST_SRV
		if (cs->rdgd.handle>0) {
			close(cs->rdgd.handle);
			struct rdgd_client_data *rdgdcli = cs->rdgd.client;
			while (rdgdcli) {
				if (rdgdcli->handle>0) close(rdgdcli->handle);
				rdgdcli = rdgdcli->next;
			}
		}
#endif
		cs = cs->next;
	}

#ifdef HTTP_SRV
	// HTTP Port
	if (cfg->http.handle>0) close(cfg->http.handle);
#endif

#ifdef CCCAM_SRV
	if (cfg->cccam.handle>0) {
		close(cfg->cccam.handle);
		struct cc_client_data *cccli = cfg->cccam.client;
		while (cccli) {
			if (cccli->handle>0) close(cccli->handle);
			cccli = cccli->next;
		}
	}
#endif

#ifdef FREECCCAM_SRV
	if (cfg->freecccam.handle>0) {
		close(cfg->freecccam.handle);
		struct cc_client_data *fcccli = cfg->freecccam.client;
		while (fcccli) {
			if (fcccli->handle>0) close(fcccli->handle);
			fcccli = fcccli->next;
		}
	}
#endif

#ifdef MGCAMD_SRV
	if (cfg->mgcamd.handle>0) {
		close(cfg->mgcamd.handle);
		struct mg_client_data *mgcli = cfg->mgcamd.client;
		while (mgcli) {
			if (mgcli->handle>0) close(mgcli->handle);
			mgcli = mgcli->next;
		}
	}
#endif

	if (cfg->cachesock>0) close(cfg->cachesock);

	struct cs_cachepeer_data *peer = cfg->cachepeer;
	while (peer) {
		if (peer->outsock>0) close(peer->outsock);
		peer = peer->next;
	}

	// Close Servers Connections
	struct cs_server_data *srv = cfg->server;
	while (srv) {
		if (srv->handle>0) close(srv->handle);
		srv = srv->next;
	}

	return 0;
}

