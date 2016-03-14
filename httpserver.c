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

#define HTTP_GET  0
#define HTTP_POST 1

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>

#include "common.h"
#include "debug.h"
#include "convert.h"
#include "tools.h"
#include "threads.h"
#include "ecmdata.h"

#ifdef CCCAM
#include "msg-cccam.h"
#endif

#include "config.h"
#include "sockets.h"

#include "httpserver.h"
#include "httpbuffer.c"
#include "dyn_buffer.c"

#include "main.h"

#include "images.c"

struct cs_cachepeer_data *getpeerbyid(int32_t id);
struct cs_server_data *getsrvbyid(uint32 id);
void cc_disconnect_srv(struct cs_server_data *srv);
void cc_disconnect_cli(struct cc_client_data *cli);
char *src2string(int32_t srctype, int32_t srcid, char *prestr, char *ret);

typedef struct
{
	char name[256];
	char value[512];
} http_get;

typedef struct 
{
	struct dyn_buffer dbf;
	int32_t type;//= (HTTP_GET/HTTP_POST)
	char path[512];
	char file[512];
	int32_t http_version;//(0:1.0,1:1.1)
	char Host[100];//(localhost:9999)
	int32_t Connection;//(1:keep-alive, 0:close);
	http_get getlist[20];
	int32_t getcount;
	http_get postlist[20];
	int32_t postcount;
	http_get headers[20];
	int32_t hdrcount;

} http_request;

void buf2str( char *dest, char *start, char *end)
{
  while (*start==' ') start++;
  while (start<=end)
  {
	*dest=*start;
	start++;
	dest++;
  }
  *dest='\0';
}

///////////////////////////////////////////////////////////////////////////////
char *isset_get(http_request *req, char *name)
{
  int32_t i;
  char *n,*v;
  for(i=0; i<req->getcount; i++) {
    n = req->getlist[i].name;
    v = req->getlist[i].value;
    if (!strcmp(name, n)) {
		//printf("[$_GET] Name: '%s'    Value :'%s'\n", n,v);
		return v;
	}
  }
  return NULL;
}


///////////////////////////////////////////////////////////////////////////////
char *isset_header(http_request *req, char *name)
{
  int32_t i;
  char *n,*v;
  //printf("Searching '%s'\n", name);
  for(i=0; i<req->hdrcount; i++) {
	//printf("[HEADER] Name: '%s'    Value :'%s'\n", req->headers[i].name, req->headers[i].value);
    n = req->headers[i].name;
    v = req->headers[i].value;
    if (!strcmp(name, n)) return v;
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
void explode_get(http_request *req, char *get) // Get Variables
{
  char *end,*a;
  int32_t i;
  i=0;
  //debugf("explode_get()\n");
  while ( (end=strchr(get, '&')) ) 
  {
	*end = '\0';
        if (i>8) break; //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	if ( (a=strchr(get, '=')) ) {
	  *a='\0';
	  strncpy(req->getlist[i].name,get,255); 
	  strncpy(req->getlist[i].value,a+1,255);
	  //printf("$_GET['%s'] = '%s'\n",req->getlist[i].name,req->getlist[i].value);
	  i++;
	}
	get=end+1;
  }
  if ( (a=strchr(get, '=')) ) {
    *a='\0';
    strncpy(req->getlist[i].name,get,255);
    strncpy(req->getlist[i].value,a+1,255);
    //printf("$_GET['%s'] = '%s'\n",req->getlist[i].name,req->getlist[i].value);
    i++;
  }
  req->getcount=i;
}

///////////////////////////////////////////////////////////////////////////////
void explode_post(http_request *req, char *post)
{
  char *end,*a;
  int32_t i;
  i=0;
  while ( (end=strchr(post, '&')) ) 
  {
	*end = '\0';
	if ( (a=strchr(post, '=')) ) {
	  *a='\0';
	  strncpy(req->postlist[i].name,post,255); 
	  strncpy(req->postlist[i].value,a+1,255);
	  //printf("$_POST['%s'] = '%s'\n",req->postlist[i].name,req->postlist[i].value);
	  i++;
	}
	post=end+1;
  }
  if ( (a=strchr(post, '=')) ) {
    *a='\0';
    strncpy(req->postlist[i].name,post,255);
    strncpy(req->postlist[i].value,a+1,255);
    i++;
  }
  req->postcount=i;
}

///////////////////////////////////////////////////////////////////////////////


int32_t extractreq(http_request *req, char *buffer, int32_t len )
{
	char *path_start, *path_end;
	char *rnrn, *slash;

	//printf("buffer size %d\n",len);
	//#Check Header
	if (!(rnrn=strstr( buffer, "\r\n\r\n"))) return -1;
	int32_t reqsize = (rnrn-buffer)+4;
	//#Get Path
	path_start = buffer+4;
	while (*path_start==' ') path_start++;
	path_end=path_start;
	while (*path_end!=' ') path_end++;
	buf2str( req->path, path_start, path_end-1);
	//printf("Path = '%s'\n", req->path);
	//#extract filename, and path
	slash = path_start = (char*)&req->path;
	while (*path_start) {
		if (*path_start=='/') slash=path_start;
		else if (*path_start=='?') {
			explode_get(req,path_start+1);
			*path_start='\0';
			break;
		}
		path_start++;
	}
	slash++;
	strncpy( req->file, slash, 100);
	//#Extract headers
	path_start = buffer+4;
	while ( (*path_start!='\r')&&(*path_start!='\n') ) path_start++;
	if (*path_start=='\r') path_start++;
	if (*path_start=='\n') path_start++;
	while ( path_start<rnrn ) {
		// start = path_start
		//get end of line
		path_end = path_start;
		slash = NULL;
		while ( (*path_end!='\r')&&(*path_end!='\n')&&(*path_end!=0) ) {
			if (*path_end==':') if (!slash) slash = path_end;
			path_end++;
		}
		if (path_end==path_start) break; // end
		char tmp = *path_end;
		*path_end = 0;
		if (slash) {
			// Extract header name: value
			buf2str( req->headers[req->hdrcount].name , path_start, slash-1);
			buf2str( req->headers[req->hdrcount].value, slash+1, path_end-1);
			//printf(">> %s\n", path_start);
			//printf("[HEADER] Name: '%s'    Value :'%s'\n", req->headers[req->hdrcount].name, req->headers[req->hdrcount].value);
			//if ( !strcmp(req->headers[req->hdrcount].name,"Authorization") ) 
			req->hdrcount++;
		}
		*path_end = tmp;
		path_start = path_end;
		while ( (*path_start=='\r')||(*path_start=='\n') ) path_start++;
	}

	if ( !memcmp(buffer,"GET ",4) ) {
		//printf("requesttype = GET\n");
		req->type = HTTP_GET;
	}
	else if ( !memcmp(buffer,"POST",4) ) {
		//printf("requesttype = POST\n");
		req->type = HTTP_POST;
		int32_t i;
		for(i=0; i<req->hdrcount; i++) {
			if ( !strcmp(req->headers[i].name,"Content-Length") ) {
				reqsize += atoi(req->headers[i].value);
				//printf("req size %d\n", reqsize);
				return reqsize;
			}
		}
	}
	return 0;
}




int32_t parse_http_request(int32_t sock, http_request *req )
{
	unsigned char buffer[2048]; // HTTP Header cant be greater than 1k
	int32_t size;
	int32_t totalsize = 0;
	memset(buffer,0,sizeof(buffer));
	memset(req,0, sizeof(http_request));
	size = recv( sock, buffer, sizeof(buffer), MSG_NOSIGNAL);
	if (size<10) return 0;
	totalsize += size;
	//printf("** Receiving %d bytes\n%s\n",size,buffer );
	if ( !memcmp(buffer,"GET ",4) || !memcmp(buffer,"POST",4) ) {
		buffer[size] = '\0';

		// Get Header
		while ( !strstr((char*)buffer, "\r\n\r\n") ) {
			struct pollfd pfd;
			pfd.fd = sock;
			pfd.events = POLLIN | POLLPRI;
			int32_t retval = poll(&pfd, 1, 5000);
			if ( retval>0 )	{
				if ( pfd.revents & (POLLHUP|POLLNVAL) ) return 0; // Disconnect
				else if ( pfd.revents & (POLLIN|POLLPRI) ) {
					int32_t len = recv(sock, (buffer+size), sizeof(buffer)-size, MSG_NOSIGNAL);
					//printf("** Receiving %d bytes\n",len );
					if (len<=0) return 0;
					size+=len;
					buffer[size]=0;
					totalsize += len;
				}
			}
			else if (retval==0) break;
			else return 0;
		}
		// Received Header
		//debugf(" Received Header >>>\n%s\n<<<\n", buffer);
		int32_t ret = extractreq(req,(char*)buffer,size);
		if (ret==-1) return 0;
		//Get Data
		if (req->type==HTTP_POST) {
			while (ret>totalsize) {
				//printf("Waiting....\n");
				struct pollfd pfd;
				pfd.fd = sock;
				pfd.events = POLLIN | POLLPRI;
				int32_t retval = poll(&pfd, 1, 5000);
				if ( retval>0 )	{
					if ( pfd.revents & (POLLHUP|POLLNVAL) ) return 0; // Disconnect
					else if ( pfd.revents & (POLLIN|POLLPRI) ) {
						if (size>=sizeof(buffer)) {
							dynbuf_write( &req->dbf, buffer, size);
							size = 0;
						}
						int32_t len = recv(sock, (buffer+size), sizeof(buffer)-size, MSG_NOSIGNAL);
						//printf("** Receiving %d bytes\n",len );
						if (len<=0) return 0;
						size+=len;
						totalsize += len;
					}
				}
				else return 0;
			}
			if (size) dynbuf_write( &req->dbf, buffer, size);
		}
	}
	else return 0;
	return 1;
}



/// XXX: not thread safe
char channelname[256];
char *getchname(uint16 caid, uint32 prov, uint16 sid )
{
	struct chninfo_data *chn= prg.chninfo;
	while (chn) {
		if ( (chn->caid==caid)&&(chn->prov==prov)&&(chn->sid==sid) ) return chn->name;
		chn = chn->next;
	}
	sprintf(channelname, "%04X:%06X:%04X", caid, prov, sid );
	return channelname;
}



int32_t total_profiles()
{
	int32_t count=0;
	struct cardserver_data *cs = cfg.cardserver;
	while(cs) {
		count++;
		cs = cs->next;
	}
	return count;
}



int32_t total_servers()
{
	int32_t nb=0;
	struct cs_server_data *srv=cfg.server;
	while (srv) {
		nb++;
		srv=srv->next;
	}
	return nb;
}

int32_t connected_servers()
{
	int32_t nb=0;
	struct cs_server_data *srv=cfg.server;
	while (srv) {
		if (srv->handle>0) nb++;
		srv=srv->next;
	}
	return nb;
}


int32_t totalcachepeers()
{
	struct cs_cachepeer_data *peer;
	int32_t count=0;

	peer = cfg.cachepeer;
	while (peer) {
		count++;
		peer = peer->next;
	}
	return count;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//<LINK href=\"./style.css\" rel=\"stylesheet\" type=\"text/css\">


//color: #000000; background-color: #FFFFFF;
char http_replyok[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";

char http_html[] = "<HTML>\n";
char http_html_[] = "</HTML>\n";

char http_head[] = "<HEAD>\n";
char http_head_[] = "</HEAD>\n";

char http_body[] = "<BODY>\n";
char http_body_[] = "</BODY>\n";

char http_title[] = "<title>NewBox - %s</title>\n";

char http_link[] = "<link rel=\"icon\" type=\"image/png\" href=\"/favicon.png\">\n";

char http_style[] = "<STYLE TYPE=\"text/css\" MEDIA=screen><!--\n\
body { background-color: #7c9eda; font-family: Tahoma; font-size: 14; color: #000; }\
a { font-family: Tahoma; font-size: 12; }\
a:link { text-decoration: none; color: #3300FF; }\
a:visited { text-decoration: none; color: #3300FF; }\
a:hover { text-decoration: none; color: #FF0000; }\
a:active { text-decoration: none; color: #3300FF; }\
td { border-width: 0px; font-family: Tahoma; font-size: 11; padding: 2px 7px 2px 7px; }\
.header { font-family: Arial, sans-serif; font-size: 12; font-weight: bold; background-color: #441144; color: #ffffff; text-align: center; }\
.busy { background-color: #99bb00; text-align: center; }\
.online { background-color: #66F822; text-align: center; }\
.offline { background-color: #EE3333; text-align: center; }\
.left { text-align: left; }\
.center { text-align: center; }\
.right { text-align: right; }\
.alt1 { background-color: #FFFFFF; }\
.alt2 { background-color: #FAF8FA; }\
.alt3 { background-color: #E4EAE4; }\
.small { font-size: 10; }\
textarea { border:5px solid #999999; width:100%; height:81%; margin:5px 0; padding:3px; font-size: 13 }\
div.outer { position: relative; width: 100%; } div.right { width: 100%; margin-left: 50%; } div.top,div.bottom { position: absolute; width: 50%; }\ div.top { top: 0; } div.bottom { bottom: 0; }\
fieldset { border-radius: 11px; background-color: #ffffff; font-weight: bolder; color : #000000; font: 16px;  }\
.menu { margin-top: 70px; padding: 3px; background-color: #ffffff; border-bottom: 5px solid #000000; width:100%; overflow: hidden; }\
.menu ul { margin:3px; padding:3px; }\
.menu li { list-style-type: none; float:left; border-right: 2px solid #000000; }\
.menu li a { float: left; color: #000000; padding: 3px 16px; margin: 0 2px 0 1px; text-decoration: none; font: 900 16px Arial,Helvetica,sans-serif; }\
.menu a.selected { background: #000000; color:#ffffff; }\
.menu a.disabled { color:#ffffff; }\
.menu li a:hover { color: #ffffff; text-decoration: none; background-color: #000000; }\
input, textarea, select { padding:3px; border:2px solid #000000; border-radius:5px; box-shadow:1px 1px 2px #A4A4A4 inset; }\
input, select { margin: 5px 1px; background: #E6E6E6; }\
input:hover, select:hover { background: #ffffff; }\
.sbutton { background: #ffffff; font-weight:bolder; }\
.option th, .option td { font-family : Arial,sans-serif; font-size: 10; background : #ffffff; color : #000000; }\
.infomenu { border-width: 0px; background-color: whitesmoke; font-family: Tahoma; font-size: 14; }\
.infomenu td { border-width: 0px; font-family: Tahoma; font-size: 14; padding: 0px 0px; }\
.button { float:left; height:auto; font: 11px Lucida Grande, Geneva, Verdana, Arial, Helvetica, sans-serif; width:100px; text-align:center; white-space:nowrap; }\
.button a:link, .button a:visited {	background-color: #e8e8e8;color: #666; font-size: 11px; font-weight:bolder; text-decoration: none; border:1px solid #000; margin: 0.2em; padding:0.2em; display:block; }\
.button a:hover { background-color: #f8f8f8; color:#555; border-top:0.1em solid #777; border-left:0.1em solid #777; border-bottom:0.1em solid #aaa; border-right:0.1em solid #aaa; padding:0.2em; margin: 0.2em; }\
div.outer { position: relative; width: 100%; } div.right { width: 100%; margin-left: 50%; } div.top,div.bottom { position: absolute; width: 50%; } div.top { top: 0; } div.bottom { bottom: 0; }\
table.option th, table.option td { font-family : Arial,sans-serif; font-size: 10; background : #efe none; color : #630; }\
\n\
.yellow { font: 11px Verdana, Arial, Helvetica, sans-serif; border-collapse: collapse; }\
.yellow th { padding: 3px; text-align: left; border-top: 1px solid #FB7A31; border-bottom: 1px solid #FB7A31; background: #FFC; }\
.yellow tr:hover { background-color: #ebf2fb; }\
.yellow td { border-bottom: 1px solid #CCC; padding: 3px; }\
.yellow td+td, .yellow th+th { border-left: 1px solid #CCC; }\
.yellow .alt1 { background-color: #FFFFFF; }\
.yellow .alt2 { background-color: #FAF8FA; }\
.yellow .alt3 { background-color: #E4EAE4; }\
.yellow .success { color: #347C17; }\
.yellow .failed { color: #C11B17; }\
\n\
\n--></STYLE>\n";

char http_menu[] = "<div class='menu'><ul><li><a class='selected' href=\"/\">Home</a></li>\
<li><a class='selected' href=\"/servers\">Servers</a></li>"
"<li><a class='selected' href=\"/cache\">Cache</a></li>"
"<li><a class='selected' href=\"/profiles\">Profiles</a></li>"
//"<li><a class='selected' href=\"/newcamd\">Newcamd</a></li>"
#ifdef CCCAM_SRV
"<li><a class='selected' href=\"/cccam\">CCcam</a></li>"
#endif
#ifdef FREECCCAM_SRV
"<li><a class='selected' href=\"/freecccam\">FreeCCcam</a></li>"
#endif
#ifdef MGCAMD_SRV
"<li><a class='selected' href=\"/mgcamd\">MGcamd</a></li>"
#endif
"<li><a class='selected' href=\"/editor\">Editor</a></li>"
"<li><a class='selected' href=\"/restart\">Restart</a></li></ul></div>"
"<br><br>";


void tcp_writeecmdata(struct tcp_buffer_data *tcpbuf, int32_t sock, int32_t ecmok, int32_t ecmnb)
{
	char http_buf[1024];
	if (ecmnb) {
		int32_t n;
		if (ecmnb>9999999) n = (ecmok*10)/(ecmnb/10); else n = (ecmok*100)/ecmnb;
		sprintf( http_buf, "<td>%d<span style=\"float: right;\">%d%%</span></td>", ecmok, n );
	}
	else
		sprintf( http_buf, "<td><span style=\"float: right;\">0%%</span></td>" );
	tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
}

void tcp_writeecmdata2(struct tcp_buffer_data *tcpbuf, int32_t sock, int32_t ecmok, int32_t ecmnb)
{
	char http_buf[1024];
	if (ecmnb) {
		int32_t n;
		if (ecmnb>9999999) n = (ecmok*10)/(ecmnb/10); else n = (ecmok*100)/ecmnb;
		sprintf( http_buf, "<td>%d / %d<span style=\"float: right;\">%d%%</span></td>", ecmok, ecmnb, n );
	}
	else
		sprintf( http_buf, "<td><span style=\"float: right;\">0%%</span></td>" );
	tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
}

void getstatcell(int32_t ecmok, int32_t ecmnb, char *dest)
{
	if (ecmnb) {
		int32_t n;
		if (ecmnb>9999999) n = (ecmok*10)/(ecmnb/10); else n = (ecmok*100)/ecmnb;
		sprintf( dest, "%d<span style=\"float: right;\">%d%%</span>", ecmok, n );
	}
	else sprintf( dest, "<span style=\"float: right;\">0%%</span>" );
}

void getstatcell2(int32_t ecmok, int32_t ecmnb, char *dest)
{
	if (ecmnb) {
		int32_t n;
		if (ecmnb>9999999) n = (ecmok*10)/(ecmnb/10); else n = (ecmok*100)/ecmnb;
		sprintf( dest, "%d / %d<span style=\"float: right;\">%d%%</span>", ecmok, ecmnb, n );
	}
	else sprintf( dest, "<span style=\"float: right;\">0%%</span>" );
}


void http_send_image(int32_t sock, http_request *req, unsigned char *buf, int32_t size, char *type)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	sprintf( http_buf, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nCache-Control: private, max-age=86400\r\nContent-Length: %d\r\nConnection: close\r\nContent-Type: image/%s\r\n\r\n", size, type);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, (char*)buf, size );
	tcp_flush(&tcpbuf, sock);
}


void http_send_xml(int32_t sock, http_request *req, char *buf, int32_t size)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
//	sprintf( http_buf, "HTTP/1.1 200 OK\r\nDate: Tue, 03 Apr 2012 08:10:57 GMT\r\nServer: Apache/2.2.14 (Ubuntu)\r\nLast-Modified: Tue, 03 Apr 2012 08:08:36 GMT\r\nETag: \"1c08f-b7-4bcc1d0490900\"\r\nAccept-Ranges: bytes\r\nContent-Length: %d\r\nKeep-Alive: timeout=15, max=100\r\nConnection: Keep-Alive\r\nContent-Type: application/xml\r\n\r\n" , size );
	sprintf( http_buf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Type: application/xml\r\n\r\n", size);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, (char*)buf, size );
	tcp_flush(&tcpbuf, sock);
}

/*

char file[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html>\n\n  <head>\n    <script type=\"text/javascript\" language=\"javascript\">\n\n      //=====================================================\n\n      function makeHttpRequest(url, callFunction, xml)\n      {\n\n        //===============================\n        // Define http_request\n        \n        var httpRequest;\n        try\n        {\n          httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n        }\n        catch(trymicrosoft)\n        {\n          try\n          {\n            httpRequest = new ActiveXObject(\"Msxml2.XMLHTTP\");\n          }\n          catch(oldermicrosoft)\n          {\n            try\n            {\n              httpRequest = new ActiveXObject(\"Microsoft.XMLHTTP\");\n            }\n            catch(failed)\n            {\n              httpRequest = false;\n            }\n          }\n        }\n        if(!httpRequest)\n        {\n          alert('Your browser does not support Ajax.');\n          return false;\n        }\n\n        //===============================\n        // Action http_request\n\n        httpRequest.onreadystatechange = function()\n        {\n          if(httpRequest.readyState == 4)\n            if(httpRequest.status == 200)\n            {\n              if(xml)\n                eval(callFunction+'(httpRequest.responseXML)');\n              else\n                eval(callFunction+'(httpRequest.responseText)');\n            }\n            else\n              alert('Request Error: '+httpRequest.status);\n        }\n        httpRequest.open('GET',url,true);\n        httpRequest.send(null);\n      \n      }\n\n      //=====================================================\n\n      function buildTable2D(tabName,xmlDoc)\n      {\n        var htmDiv = document.getElementById('div'+tabName);\n        htmDiv.innerHTML = '<table border=1 id=\"'+tabName+'\"></table>';\n        var htmTab = document.getElementById(tabName);\n        var xmlTag = xmlDoc.getElementsByTagName(xmlDoc.documentElement.tagName).item(0);\n        for (var row=0; row < xmlTag.childNodes.length; row++)\n        {\n          var xmlRow = xmlTag.childNodes.item(row);\n          if(xmlRow.childNodes.length > 0)\n          {\n            var htmRow = htmTab.insertRow(parseInt(row/2));\n            for (var col=0; col < xmlRow.childNodes.length; col++)\n            {\n              var xmlCell = xmlRow.childNodes.item(col);\n              if(xmlCell.childNodes.length > 0)\n              {\n                var htmCell = htmRow.insertCell(parseInt(col/2));\n                htmCell.innerHTML = xmlCell.childNodes.item(0).data;\n              }\n            }\n          }\n        }\n      }\n\n      //=====================================================\n\n      function buildTable(xmlDoc)\n      {\n        buildTable2D('xmltable',xmlDoc);\n      }\n\n      //=====================================================\n\n    </script>\n  </head>\n\n  <body>\n    <input type=\"button\" value=\"My First Ajax XML\" onclick=\"makeHttpRequest('MyFirstAjaxXML.xml','buildTable',true)\"/>\n    <div id=\"divxmltable\">\n    </div>\n  </body>\n  \n</html>\n";

void http_send_file(int32_t sock, http_request *req)
{
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, file, strlen(file) );
	tcp_flush(&tcpbuf, sock);
}

*/

void http_send_index(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Home"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_writestr(&tcpbuf, sock, "\n<script type=\"text/JavaScript\"><!--\nfunction imgrequest( url, el )\n\
{\n\
	var httpRequest;\n\
	try { httpRequest = new XMLHttpRequest(); }\n\
	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n\
	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n\
	if ( typeof(el)!='undefined' ) {\n\
		el.onclick = null;\n\
		el.style.opacity = '0.7';\n\
		httpRequest.onreadystatechange = function()\n\
		{\n\
			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n\
		}\n\
	}\n\
	httpRequest.open('GET', url, true);\n\
	httpRequest.send(null);\n\
}\n\
\
var autorefresh=3000;\n\
var tautorefresh;\n\
function setautorefresh(t)\n\
{\n\
	clearTimeout(tautorefresh);\n\
	autorefresh = t;\n\
	if (t>0) tautorefresh = setTimeout('updateDiv()',autorefresh);\n\
}\n\
function updateDiv()\n\
{\n\
	var httpRequest;\n\
	try {\n\
		httpRequest = new XMLHttpRequest();\n\
	}\n\
	catch(trymicrosoft) {\n\
		try {\n\
			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n\
		}\n\
		catch(oldermicrosoft) {\n\
			try {\n\
				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n\
			}\n\
			catch(failed) {\n\
				httpRequest = false;\n\
			}\n\
		}\n\
	}\n\
	if (!httpRequest) {\n\
		alert('Your browser does not support Ajax.');\n\
		return false;\n\
	}\n\
	httpRequest.onreadystatechange = function()\n\
	{\n\
		if (httpRequest.readyState == 4) {\n\
			if(httpRequest.status == 200) {\n\
				requestError=0;\n\
				document.getElementById('mainDiv').innerHTML = httpRequest.responseText;\n\
			}\n\
			tautorefresh = setTimeout('updateDiv()',autorefresh);\n\
		}\n\
	}\n\
	httpRequest.open('GET', '/?action=div', true);\n\
	httpRequest.send(null);\n\
}\n\
\
function start()\n\
{\n\
	 setautorefresh(autorefresh);\n\
}\n\
\n--></script>\n" );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<BODY onload=\"start();\"><div id='mainDiv'>");
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );


	unsigned int d= GetTickCount()/1000;
	sprintf( http_buf,"<b>NewBox v%s\n</b><br>", VERSION); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf,"<div align=right>Autorefresh <select onchange='setautorefresh(this.value);'><option value=0>OFF</option><option value=1000>1s</option><option value=2000>2s</option><option value=3000 selected>3s</option><option value=4000>4s</option><option value=5000>5s</option><option value=6000>6s</option><option value=7000>7s</option><option value=8000>8s</option><option value=9000>9s</option><option value=10000>10s</option></select></div>\n<b>Uptime CardServer:</b> %02dd\n %02d:%02d:%02d\n<br>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	struct utsname info;
	gettimeofday( uname(&info), NULL );
	sprintf( http_buf,"<br><b>System:</b> %s\n<br><b>Nodename:</b> %s\n<br><b>Kernel:</b> %s\n<br><b>Version:</b> %s\n<br><b>CPU Architecture:</b> %s\n<br>", info.sysname, info.nodename, info.release, info.version, info.machine); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<br><b>Total Profiles:</b> %d\n", total_profiles() ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<br><b>Connected Servers:</b> %d\n / %d\n", connected_servers(), total_servers() ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );


	//int32_t total, connected, active;

	//cccam_clients( &total, &connected, &active );

	//sprintf( http_buf, "<br><b>Connected CCcam Clients:</b> %d / %d", connected, total); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	//mgcamd_clients( &total, &connected, &active );

	//sprintf( http_buf, "<br><b>Connected MGcamd Clients:</b> %d / %d", connected, total); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<br><b>Total Cache Peers:</b> %d", totalcachepeers() ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<br><br><fieldset><legend> <b>Debug:</b> </legend><pre style=\"background-color: #fff; font-size:10; color:#004455;\">"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	int32_t i=idbgline;
	do {
		sprintf( http_buf, "%s", dbgline[i] ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		i++;
		if (i>=MAX_DBGLINES) i=0;
	} while (i!=idbgline);
	sprintf( http_buf, "</pre></fieldset><br><center>Copyright (C) 2014 - 2016 developed by <a href='https://www.lonasdigital.com'><b>Javilonas</b></a>.\n <br>Followme on <a href='https://twitter.com/Javilonas'><b>Twitter</b></a> \n and\n <a href='https://github.com/javilonas'><b>Github</b></a></center></div><br>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	tcp_write(&tcpbuf, sock, http_body_, strlen(http_body_) );
	tcp_write(&tcpbuf, sock, http_html_, strlen(http_html_) );
	tcp_flush(&tcpbuf, sock);
}

void http_send_restart(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Restarting"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	tcp_writestr(&tcpbuf, sock, "\n<script type=\"text/JavaScript\"><!--\nsetTimeout(\"location.href = '/';\",5000);\n--></script>\n<h3>Restarting NewBox<br>Plesase Wait...</h3>");

	tcp_flush(&tcpbuf, sock);
	prg.restart = 1;
}


///////////////////////////////////////////////////////////////////////////////
// SERVERS
///////////////////////////////////////////////////////////////////////////////

char *srvtypename(struct cs_server_data *srv)
{
	static char newcamd[] = "Newcamd";
	//static char mgcamd[] = "MGcamd";
	static char cccam[] = "CCcam";
	static char radegast[] = "Radegast";
	if (srv->type==TYPE_NEWCAMD) return newcamd;
	//if (srv->type==TYPE_MGCAMD) return mgcamd;
	if (srv->type==TYPE_CCCAM) return cccam;
	if (srv->type==TYPE_RADEGAST) return radegast;
	return NULL;
}


int32_t srv_cardcount(struct cs_server_data *srv, int32_t uphops)
{
	int32_t count=0;
	struct cs_card_data *card = srv->card;
	while (card) {
		if ( (uphops==-1) 
#ifdef CCCAM_CLI
			|| (card->uphops==uphops)
#endif
		) count++;
		card = card->next;
	}
	return count;
}


char *encryptxml( char *src)
{
	char exml[1024];
	char *dest = exml;
	while (*src) {
		switch (*src) {
			case '<':
				memcpy(dest,"&lt;", 4);
				dest +=4;
				break;
			default:
				*dest = *src;
				dest++;
		}
		src++;
	}
	*dest = 0;
	strcpy( src, exml);
	return src;
}


void getservercells(struct cs_server_data *srv, char cell[8][1024] )
{
	char temp[1024];
	unsigned int ticks = GetTickCount();
	uint d;
	int32_t i;
	memset(cell, 0, 8*1024);
	// CELL0
	if (srv->handle>0)
		d = (((ticks-srv->connected) + srv->uptime)/10) / (ticks/1000);
	else
		d = (srv->uptime*100) / ticks;
	sprintf( cell[0],"%d%%",d);
	// CELL1
	sprintf( cell[1],"<a href=\"/server?id=%d\">%s:%d</a><br>", srv->id,srv->host->name,srv->port);
	if (!srv->host->ip && srv->host->clip)
		sprintf( temp,"0.0.0.0 (%s)",(char*)ip2string(srv->host->ip) );
	else
		sprintf( temp,"%s",(char*)ip2string(srv->host->ip) );
	strcat( cell[1], temp );
	// CELL2
	if (srv->type==TYPE_NEWCAMD) {
		if (srv->progname) strcpy( cell[2], srv->progname); else sprintf( cell[2],"Newcamd");
	}
#ifdef CCCAM_CLI
	else if (srv->type==TYPE_CCCAM) {
		if (srv->handle>0)
			sprintf( cell[2],"CCcam %s<br>%02x%02x%02x%02x%02x%02x%02x%02x", srv->version, srv->nodeid[0],srv->nodeid[1],srv->nodeid[2],srv->nodeid[3],srv->nodeid[4],srv->nodeid[5],srv->nodeid[6],srv->nodeid[7]);
		else sprintf( cell[2],"CCcam");
		//if (srv->progname) sprintf( cell[2],"<td>CCcam(%s) %s", srv->progname, srv->version); else sprintf( cell[2],"<td>CCcam %s", srv->version);

	}
#endif
	/*else if (srv->type==TYPE_MGCAMD) {
		sprintf( cell[2],"MGcamd");
	}*/
#ifdef RADEGAST_CLI
	else if (srv->type==TYPE_RADEGAST) {
		sprintf( cell[2],"Radegast");
	}
#endif
	else { // Unknown
		sprintf( cell[2],"Unknown");
	}

	// CELL3
	if (srv->handle>0) {
		d = (ticks-srv->connected)/1000;
		sprintf( cell[3],"%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		if (srv->busy) sprintf( cell[7],"busy"); else sprintf( cell[7],"online");
	}
	else {
		sprintf( cell[3],"offline");
		sprintf( cell[7],"offline");
	}

	// CELL4
	if (srv->ecmnb)
		sprintf( cell[4],"%d / %d<span style=\"float: right;\">%d%%</span>",srv->ecmok ,srv->ecmnb, (srv->ecmok*100)/srv->ecmnb);
	else
		sprintf( cell[4],"<span style=\"float: right;\">0%%</span>");

	// CELL5
	if (srv->ecmok)
		sprintf( cell[5],"%d ms",(srv->ecmoktime/srv->ecmok) );
	else
		sprintf( cell[5],"-- ms");
	// CELL6
	strcat( cell[6], "<span style='float:right;'>");
	//sprintf( temp,"<img title='Refresh' src='refresh.png' OnClick=\"serveraction(%d,'refresh');\">",srv->id); strcat( cell[6], temp );

	if (srv->disabled) {
		sprintf( temp," <img title='Enable' src='enable.png' OnClick=\"serveraction(%d,'enable');\">",srv->id);
		strcat( cell[6], temp );
	}
	else {
		sprintf( temp," <img title='disable' src='disable.png' OnClick=\"serveraction(%d,'disable');\">",srv->id);
		strcat( cell[6], temp );
	}
	strcat( cell[6], "</span>");
	if (srv->handle>0) {
		if (srv->type==TYPE_CCCAM)
			sprintf( temp,"<b>Total Cards = %d\n</b> ( <font color='#2b7d04'><b>Hop1</b></font> = %d,\n <font color='blue'><b>Hop2</b></font> = %d,\n <font color='red'><b>Hop3</b></font> = %d\n )<font style=\"font-size: 9;\">", srv_cardcount(srv,-1), srv_cardcount(srv,1), srv_cardcount(srv,2), srv_cardcount(srv,3));
		else
			sprintf( temp,"<b>Total Cards = %d</b><font style=\"font-size: 9;\">", srv_cardcount(srv,-1) );
		strcat( cell[6], temp );
		int32_t icard = 0;
		struct cs_card_data *card = srv->card;
		while (card) {
			if (card->uphops<=1) {
				if (icard>3) {
					strcat( cell[6], "<br> ..." );
					break;
				}
				sprintf( temp,"<br><b>%04x:</b> %x",card->caid,card->prov[0]);
				strcat( cell[6], temp );
				for(i=1; i<card->nbprov; i++) {
					sprintf( temp,", %x", card->prov[i]);
					strcat( cell[6], temp );
				}
				icard++;
			}
			card = card->next;
		}
		strcat( cell[6],"</font>");
	}
	else {
		if (srv->statmsg) {
			//if (srv->error) sprintf( temp,"%s (errno=%d)",srv->statmsg, srv->error); else 
			sprintf( temp,"%s",srv->statmsg);
			strcat( cell[6], temp );
		}
	}
}

/*

total servers   connected
total cccam
total newcamd
total radegats
*/

void alltotal_servers( int32_t *all, int32_t *cccam, int32_t *newcamd, /*int32_t *mgcamd,*/ int32_t *radegast )
{
	*all = 0;
	*cccam = 0;
	*newcamd = 0;
	//*mgcamd = 0;
	*radegast = 0;

	struct cs_server_data *srv=cfg.server;
	while (srv) {
		(*all)++;
		if (srv->type==TYPE_CCCAM) (*cccam)++;
		else if (srv->type==TYPE_NEWCAMD) (*newcamd)++;
		//else if (srv->type==TYPE_MGCAMD) (*mgcamd)++;
		else if (srv->type==TYPE_RADEGAST) (*radegast)++;
		srv=srv->next;
	}
}

void allconnected_servers( int32_t *all, int32_t *cccam, int32_t *newcamd, /*int32_t *mgcamd,*/ int32_t *radegast )
{
	*all = 0;
	*cccam = 0;
	*newcamd = 0;
	//*mgcamd = 0;
	*radegast = 0;

	struct cs_server_data *srv=cfg.server;
	while (srv) {
		if (srv->handle>0) {
			(*all)++;
			if (srv->type==TYPE_CCCAM) (*cccam)++;
			else if (srv->type==TYPE_NEWCAMD) (*newcamd)++;
			//else if (srv->type==TYPE_MGCAMD) (*mgcamd)++;
			else if (srv->type==TYPE_RADEGAST) (*radegast)++;
		}
		srv=srv->next;
	}
}

void http_send_servers(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;

	char cell[8][1024];
	struct cs_server_data *srv;
	int32_t i;

	char *id = isset_get( req, "id");
	// Get Server ID
	if (id)	{
		i = atoi(id);
		//look for server
		srv = cfg.server;
		while (srv) {
			if (srv->id==(uint32)i) break;
			srv = srv->next;
		}
		if (!srv) return;
		char *action = isset_get( req, "action");
		if (action) {
			if (!strcmp(action,"disable")) {
				srv->disabled = 1;
				if (srv->type==TYPE_NEWCAMD) cs_disconnect_srv(srv);
				//else if (srv->type==TYPE_MGCAMD) cc_disconnect_srv(srv);
				else if (srv->type==TYPE_CCCAM) cc_disconnect_srv(srv);
#ifdef RADEGAST_CLI
				else if (srv->type==TYPE_RADEGAST) rdgd_disconnect_srv(srv);
#endif
			}
			else if (!strcmp(action,"enable")) {
				srv->disabled = 0;
				srv->host->checkiptime = 0;
			}
		}
		// Send XML CELLS
		getservercells(srv,cell);
		char buf[5000] = "";
		sprintf( buf, "<server>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n</server>\n",encryptxml(cell[0]),encryptxml(cell[1]),encryptxml(cell[2]),encryptxml(cell[7]),encryptxml(cell[3]),encryptxml(cell[4]),encryptxml(cell[5]),encryptxml(cell[6]) );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Servers"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>\n\nvar requestError = 0;\n\nfunction makeHttpRequest( url, xml, callFunction, param )\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				if(xml) {\n					eval( callFunction+'(httpRequest.responseXML,param)');\n				}\n				else {\n					eval( callFunction+'(httpRequest.responseText,param)');\n				}\n			}\n			else {\n				requestError = 1;\n			}\n		}\n	}\n	httpRequest.open('GET',url,true);\n	httpRequest.send(null);\n}\n\nvar currentid=0;\n\nfunction xmlupdateserver( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n}\n\nfunction serveraction(id, action)\n{\n	return makeHttpRequest( '/servers?id='+id+'&action='+action , true, 'xmlupdateserver', 'srv'+id);\n}\n\nfunction dotimer()\n{\n	if (!requestError) {\n		if (currentid>0) serveraction(currentid, 'refresh');\n		t = setTimeout('dotimer()',1000);\n	}\n}\n</script>\n" );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<BODY onload=\"dotimer();\">");
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );


	int32_t iall, icccam, inewcamd, /*imgcamd,*/ iradegast; // Total
	alltotal_servers( &iall, &icccam, &inewcamd, /*&imgcamd,*/ &iradegast );
	int32_t jall, jcccam, jnewcamd, /*jmgcamd,*/ jradegast; // Connected
	allconnected_servers( &jall, &jcccam, &jnewcamd, /*&jmgcamd,*/ &jradegast );

	//sprintf( http_buf, "<span class='button'><a href='/servers'>All Servers</a></span>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if ( (icccam+inewcamd)/*&&(icccam+imgcamd)*/&&(icccam+iradegast)&&(iradegast+inewcamd)/*&&(iradegast+imgcamd)&&(inewcamd+imgcamd)*/ ) {
		if (icccam) {
			sprintf( http_buf, "<span class='button'><a href='/servers?list=cccam'>CCcam</a></span>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		if (inewcamd) {
			sprintf( http_buf, "<span class='button'><a href='/servers?list=newcamd'>Newcamd</a></span>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		/*if (imgcamd) {
			sprintf( http_buf, "<span class='button'><a href='/servers?list=mgcamd'>MGcamd</a></span>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}*/
		if (iradegast) {
			sprintf( http_buf, "<span class='button'><a href='/servers?list=radegast'>Radegast</a></span>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		sprintf( http_buf, "<br><br>");tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	//
	char *action = isset_get( req, "list");
	int32_t listid = 0;
	if (action) {
		if (!strcmp(action,"all")) listid = 0;
		else if (!strcmp(action,"cccam")) listid = 1;
		else if (!strcmp(action,"newcamd")) listid = 2;
		//else if (!strcmp(action,"mgcamd")) listid = 3;
		else if (!strcmp(action,"radegast")) listid = 4;
	} else 	action = "all";

	sprintf( http_buf, "<span class='button'><a href='/servers?list=%s&show=connected'>Connected</a></span><span class='button'><a href='/servers?list=%s&show=disconnected'>Disonnected</a></span><br>", action, action);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	action = isset_get( req, "show");
	int32_t showid = 3;
	if (action) {
		if (!strcmp(action,"connected")) showid = 1;
		else if (!strcmp(action,"disconnected")) showid = 2;
	}


	if (listid==0) {
		if (showid==2) {
			sprintf( http_buf, "<br>Disconnected Servers: <b>%d</b> / %d", iall-jall, iall);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", (iall-jall)*100 / iall);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		else {
			sprintf( http_buf, "<br>Connected Servers: <b>%d</b> / %d", jall, iall);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", jall*100 / iall);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
	}
	else if (listid==1) {
		if (showid==2) {
			sprintf( http_buf, "<br>Disconnected Servers: <b>%d</b> / %d", icccam-jcccam, icccam);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", (icccam-jcccam)*100 / icccam);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		else {
			sprintf( http_buf, "<br>Connected Servers: <b>%d</b> / %d", jcccam, icccam);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", jcccam*100 / icccam);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
	}
	else if (listid==2) {
		if (showid==2) {
			sprintf( http_buf, "<br>Disconnected Servers: <b>%d</b> / %d", inewcamd-jnewcamd, inewcamd);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", (inewcamd-jnewcamd)*100 / inewcamd);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		else {
			sprintf( http_buf, "<br>Connected Servers: <b>%d</b> / %d", jnewcamd, inewcamd);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", jnewcamd*100 / inewcamd);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
	}
	/*else if (listid==3) {
		if (showid==2) {
			sprintf( http_buf, "<br>Disconnected Servers: <b>%d</b> / %d", imgcamd-jmgcamd, imgcamd);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", (imgcamd-jmgcamd)*100 / imgcamd);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		else {
			sprintf( http_buf, "<br>Connected Servers: <b>%d</b> / %d", jmgcamd, imgcamd);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", jmgcamd*100 / imgcamd);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
	}*/
	else if (listid==4) {
		if (showid==2) {
			sprintf( http_buf, "<br>Disconnected Servers: <b>%d</b> / %d", iradegast-jradegast, iradegast);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", (iradegast-jradegast)*100 / iradegast);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		else {
			sprintf( http_buf, "<br>Connected Servers: <b>%d</b> / %d", jradegast, iradegast);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (total_servers()) {
				sprintf( http_buf, " (%d%%)", jradegast*100 / iradegast);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
	}

	// Table
	sprintf( http_buf, "<br><center> <table class=yellow width=100%%><th width=20px>Uptime</th><th width=200px>Host</th><th width=100px>Server</th><th width=90px>Connected</td><th width=150px>Ecm OK</th><th width=50px>EcmTime</th><th>Cards</th></tr>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	srv = cfg.server;
	int32_t alt = 0;

	if (listid==0) {
		while (srv) {
			if ( ((showid&1)&&(srv->handle>0))||((showid&2)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				sprintf( http_buf,"<tr id=\"srv%d\" class=alt%d onMouseOver='currentid=%d'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}
	else if (listid==1) {
		while (srv) {
			if (srv->type==TYPE_CCCAM)
			if ( ((showid&1)&&(srv->handle>0))||((showid&2)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				sprintf( http_buf,"<tr id=\"srv%d\" class=alt%d onMouseOver='currentid=%d'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}
	else if (listid==2) {
		while (srv) {
			if (srv->type==TYPE_NEWCAMD)
			if ( ((showid&1)&&(srv->handle>0))||((showid&2)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				sprintf( http_buf,"<tr id=\"srv%d\" class=alt%d onMouseOver='currentid=%d'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}
	/*else if (listid==3) {
		while (srv) {
			if (srv->type==TYPE_MGCAMD)
			if ( ((showid&1)&&(srv->handle>0))||((showid&2)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				sprintf( http_buf,"<tr id=\"srv%d\" class=alt%d onMouseOver='currentid=%d'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}*/
	else if (listid==4) {
		while (srv) {
			if (srv->type==TYPE_RADEGAST)
			if ( ((showid&1)&&(srv->handle>0))||((showid&2)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				sprintf( http_buf,"<tr id=\"srv%d\" class=alt%d onMouseOver='currentid=%d'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}

	sprintf( http_buf, "</table></center><br>\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_flush(&tcpbuf, sock);
}




void http_send_server(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	unsigned int d;
	struct cs_server_data *srv;
	int32_t i;

	char *id = isset_get( req, "id");

	// Get Server ID
	if (id)	i = atoi(id); else i=0;
	//look for server
	srv = cfg.server;
	while (srv) {
		if (srv->id==(uint32)i) break;
		srv = srv->next;
	}
	if (!srv) {
		tcp_init(&tcpbuf);
		tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, http_title, "Server"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
		tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );
		sprintf( http_buf, "<br>Server not found (id=%d)<br>", i);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_body_, strlen(http_body_) );
		tcp_write(&tcpbuf, sock, http_html_, strlen(http_html_) );
		tcp_flush(&tcpbuf, sock);
		return;
	}


	char *action = isset_get(req,"action");
	if (action) {
		if (!strcmp(action,"disable")) {
			tcp_init(&tcpbuf);
			sprintf( http_buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf, "Offline <img onclick=\"serveraction(1,'enable');\" src='enable.png'/>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			srv->disabled = 1;
			cs_disconnect_srv(srv);
			tcp_flush(&tcpbuf, sock);
			return;
		}
		else if (!strcmp(action,"enable")) {
			tcp_init(&tcpbuf);
			sprintf( http_buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf, "Offline <img onclick=\"serveraction(1,'disable');\" src='disable.png'/>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			srv->disabled = 0;
			tcp_flush(&tcpbuf, sock);
			return;
		}
		else if (!strcmp(action,"xml")) {
			char buf[1024];
			printf(" Send XML \n");
			sprintf( buf, "<server><id>%d</id>\n<host>%s</host>\n<ip>%s</ip>\n<port>%d</port>\n",srv->id,srv->host->name,ip2string(srv->host->ip),srv->port);
			if (srv->type==TYPE_CCCAM) {
				if (srv->handle>0)
					sprintf( http_buf, "<type>CCcam</type>\n<version>%s</version>\n<nodeid>%02x%02x%02x%02x%02x%02x%02x%02x</nodeid>\n",srv->version,srv->nodeid[0],srv->nodeid[1],srv->nodeid[2],srv->nodeid[3],srv->nodeid[4],srv->nodeid[5],srv->nodeid[6],srv->nodeid[7]);
				else
					sprintf( http_buf, "<type>CCcam</type>\n");
				strcat( buf, http_buf);
			}
			else if (srv->type==TYPE_RADEGAST) strcat( buf, "<type>Radegast</type>\n");
			else if (srv->type==TYPE_NEWCAMD) strcat( buf, "<type>Newcamd</type>\n");
			//else if (srv->type==TYPE_MGCAMD) strcat( buf, "<type>MGcamd</type>\n");
			// ECM
			int32_t temp;
			if (srv->ecmok) temp =  srv->ecmoktime/srv->ecmok; else temp=0;
			sprintf( http_buf, "<ecmnb>%d</ecmnb>\n<ecmok>%d</ecmok>\n<ecmtime>%d</ecmtime>\n",srv->ecmnb,srv->ecmok,temp);
			strcat( buf, http_buf);
			// Uptime
			uint d = srv->uptime;
			if (srv->handle>0) d += (GetTickCount()-srv->connected);
			sprintf( http_buf,"<uptime>%02dd %02d:%02d:%02d</uptime>", d/(3600000*24), (d/3600000)%24, (d/60000)%60, (d/1000)%60 );
			strcat( buf, http_buf);
			// Connected
			if (srv->handle>0) {
				d = (GetTickCount()-srv->connected)/1000;
				sprintf( http_buf,"<connected>%02dd %02d:%02d:%02d</connected>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
				strcat( buf, http_buf);
			}
			strcat( buf, "</server>\n");
			http_send_xml( sock, req, buf, strlen(buf) );
			return;
		}
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Server"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	sprintf( http_buf, "<br><b>Server: %s:%d</b><ul>",srv->host->name,srv->port);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<li>IP Address: %s</li>",(char*)ip2string(srv->host->ip));
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (srv->type==TYPE_CCCAM)
	sprintf( http_buf, "<li>Type: CCcam</li>");
	else if (srv->type==TYPE_NEWCAMD)
	sprintf( http_buf, "<li>Type: Newcamd</li>");
	else if (srv->type==TYPE_MGCAMD)
	sprintf( http_buf, "<li>Type: MGcamd</li>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (srv->handle>0) {
			if (srv->type==TYPE_CCCAM) {
				sprintf( http_buf,"<li>Version: %s</li><li>NodeID: %02x%02x%02x%02x%02x%02x%02x%02x</li>", srv->version,srv->nodeid[0],srv->nodeid[1],srv->nodeid[2],srv->nodeid[3],srv->nodeid[4],srv->nodeid[5],srv->nodeid[6],srv->nodeid[7]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			d = (GetTickCount()-srv->connected)/1000;
			sprintf( http_buf,"<li>Connected: %02dd %02d:%02d:%02d</li>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			d = (GetTickCount()-srv->connected) + srv->uptime;
			sprintf( http_buf,"<li>Uptime: %02dd %02d:%02d:%02d (%d%%)</li>", d/(3600000*24), (d/3600000)%24, (d/60000)%60, (d/1000)%60, (d*100/GetTickCount()) );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			int32_t temp;
			if (srv->ecmnb)	temp = srv->ecmok*100/srv->ecmnb; else temp = 0;
			sprintf( http_buf,"<li>Total ECM = %d</li><li>EcmOK = %d (%d%%)</li>",srv->ecmnb,srv->ecmok,temp);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

			if (srv->ecmok) temp =  srv->ecmoktime/srv->ecmok; else temp=0;
			sprintf( http_buf,"<li>EcmTime = %d ms</li></ul>",temp );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

			//Print used profiles
			sprintf( http_buf, "<br>Used Profiles:</br><table class=option><tr><th width=200px>Profile name</th><th width=90px>Total ECM</th><th width=90px>Ecm OK</th><th width=90px>Ecm Time</th></tr>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			int32_t alt=0;
			for(i=0; i<MAX_CSPORTS; i++) {
				if (!srv->cstat[i].csid) break;
				struct cardserver_data *cs = getcsbyid(srv->cstat[i].csid);
				if (!cs) continue;
				if (alt==1) alt=2; else alt=1;
				//Profile name
				sprintf( http_buf,"<tr><td class=alt%d><a href=\"/profile?id=%d\">%s</a></td>",alt, cs->id, cs->name); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				//TotalECM
				sprintf( http_buf, "<td class=alt%d align=center>%d</td>",alt, srv->cstat[i].ecmnb ); //,cs->ecmdenied);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				//ECM OK
				tcp_writeecmdata(&tcpbuf, sock, srv->cstat[i].ecmok, srv->cstat[i].ecmnb );
				//ECM TIME
				int32_t temp;
				if (srv->cstat[i].ecmok) temp = srv->cstat[i].ecmoktime/srv->cstat[i].ecmok; else temp=0;
				if (temp)
					sprintf( http_buf, "<td class=alt%d align=center>%dms</td>",alt, temp);
				else
					sprintf( http_buf, "<td class=alt%d align=center>-- ms</td>",alt);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				//Close Row
				sprintf( http_buf,"</tr>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			sprintf( http_buf,"</table>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

			// Print CardList
			if (srv->type==TYPE_CCCAM) {
				sprintf( http_buf, "<br><b>Total Cards = %d\n</b> ( <font color='#2b7d04'><b>Hop1</b></font> = %d,\n <font color='blue'><b>Hop2</b></font> = %d,\n <font color='red'><b>Hop3</b></font> = %d\n )<br><table class=yellow width=100%%><tr><th width=120px>NodeID_CardID</th><th width=150px>EcmOK</th><th width=70px>EcmTime</th><th>Caid/Providers</th></tr>",srv_cardcount(srv,-1), srv_cardcount(srv,1), srv_cardcount(srv,2), srv_cardcount(srv,3));
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct cs_card_data *card = srv->card;
				int32_t alt=0;
				while(card) {
					if (alt==1) alt=2; else alt=1;
#ifdef CCCAM_CLI
					sprintf( http_buf,"<tr><td class=alt%d>%02x%02x%02x%02x%02x%02x%02x%02x_%x</td>",alt, card->nodeid[0], card->nodeid[1], card->nodeid[2], card->nodeid[3], card->nodeid[4], card->nodeid[5], card->nodeid[6], card->nodeid[7], card->shareid);
#else
					sprintf( http_buf,"<tr><td class=alt%d>%x</td>",alt, card->id);
#endif
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

#ifdef CCCAM_CLI
					sprintf( http_buf,"<td class=alt%d>%d / %d<span style=\"float:right\">",alt,card->ecmok,card->ecmnb);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					if (card->ecmnb)
						sprintf( http_buf,"%d%%</span></td>", card->ecmok*100/card->ecmnb);
					else
						sprintf( http_buf,"0%%</span></td>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					if (card->ecmok)
						sprintf( http_buf,"<td class=alt%d align=center>%d ms</td>",alt, card->ecmoktime/card->ecmok );
					else
						sprintf( http_buf,"<td class=alt%d align=center>-- ms</td>",alt);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					sprintf( http_buf,"<td class=alt%d>[%d] <b>%04x:</b> %x",alt,card->uphops,card->caid,card->prov[0]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#else
					sprintf( http_buf,"<td class=alt%d><b>%04x:</b> %x",alt,card->caid,card->prov[0]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif

					for(i=1; i<card->nbprov; i++) {
						sprintf( http_buf,", %x", card->prov[i]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					sprintf( http_buf,"</td></tr>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					card = card->next;
				}
				sprintf( http_buf,"</table>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			else {
				sprintf( http_buf, "<br>Cards:<br><table class=yellow width=100%%><tr><th>Caid/Providers</th></tr>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

				struct cs_card_data *card = srv->card;
				int32_t alt=0;
				while(card) {
					if (alt==1) alt=2; else alt=1;
					sprintf( http_buf,"<tr><td class=alt%d><b>%04x:</b> %x",alt,card->caid,card->prov[0]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					for(i=1; i<card->nbprov; i++) {
						sprintf( http_buf,", %x", card->prov[i]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					sprintf( http_buf,"</td></tr>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					card = card->next;
				}
				sprintf( http_buf,"</table>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		else {
			sprintf( http_buf,"<li>Connected: --offline--</li>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if (srv->uptime) {
				sprintf( http_buf,"<li>Uptime: %02dd %02d:%02d:%02d (%d%%)</li>", srv->uptime/(3600000*24), (srv->uptime/3600000)%24, (srv->uptime/60000)%60, (srv->uptime/1000)%60, (srv->uptime*100/GetTickCount()) );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int32_t temp;
				if (srv->ecmnb)	temp = srv->ecmok*100/srv->ecmnb; else temp = 0;
				sprintf( http_buf,"<li>Total ECM = %d</li><li>EcmOK = %d (%d%%)</li>",srv->ecmnb,srv->ecmok,temp);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (srv->ecmok) temp =  srv->ecmoktime/srv->ecmok; else temp=0;
				sprintf( http_buf,"<li>EcmTime = %d ms</li></ul>",temp );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}

	tcp_flush(&tcpbuf, sock);
}


void getcachecells(struct cs_cachepeer_data *peer, char cell[11][512] )
{
	char temp[512];
	memset(cell, 0, 11*512);
	// CELL0#Host/port
	//sprintf( cell[0],"<a href='/cacheclient?id=%d'>%s<br>%s:%d</a>",peer->id,peer->host->name,peer->port);
	sprintf( cell[0],"%s:%d",peer->host->name,peer->port);
	// CELL1#IP
	sprintf( cell[1],"%s",(char*)ip2string(peer->host->ip) );
	// CELL2#Program
	sprintf( cell[2],"%s %s",peer->program, peer->version);
	// CELL3 # Ping
	if (peer->disabled) {
		sprintf( cell[10],"offline");
		sprintf( cell[3],"Off");
	}
	else {
		if ( !peer->lastactivity || ( (peer->lastactivity+60000)<GetTickCount() ) ) sprintf( cell[10],"offline"); else sprintf( cell[10],"online");
		if ( peer->ping>0 ) sprintf( cell[3],"%d", peer->ping); else sprintf( cell[3],"?");
	}
	// CELL4 # Request
	sprintf( cell[4],"%d",peer->reqnb);
	// CELL5 #
	sprintf( cell[5],"%d",peer->repok);
	// CELL7 #Forwarded Hits
	sprintf( cell[6],"%d",peer->hitfwd);
	// CELL8 # Cache Hits/Total
	getstatcell( peer->hitnb, cfg.cachehits, cell[7] );
	// CELL9 # Instant Cache
	getstatcell( peer->ihitnb, peer->hitnb, cell[8] );
	// CELL10 # Last Used Cache
	strcat( cell[9], "<span style='float:right;'>");
	if (peer->disabled) {
		sprintf( temp," <img title='Enable' src='enable.png' OnClick=\"peeraction(%d,'enable');\">",peer->id);
		strcat( cell[9], temp );
	}
	else {
		sprintf( temp," <img title='disable' src='disable.png' OnClick=\"peeraction(%d,'disable');\">",peer->id);
		strcat( cell[9], temp );
	}
	strcat( cell[9], "</span>");
	if (peer->lastcaid) {
		sprintf( temp,"ch %s (%dms)", getchname(peer->lastcaid, peer->lastprov, peer->lastsid) , peer->lastdecodetime );
		strcat( cell[9], temp );
	}
}


void http_send_cache(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	struct cs_cachepeer_data *peer;
	char cell[11][512];

	int32_t i;
	char *id = isset_get( req, "id");
	// Get Peer ID
	if (id)	{
		i = atoi(id);
		//look for server
		peer = cfg.cachepeer;
		while (peer) {
			if (peer->id==(uint32)i) break;
			peer = peer->next;
		}
		if (!peer) return;
		//
		char *action = isset_get( req, "action");
		if (action) {
			if (!strcmp(action,"disable")) {
				peer->disabled = 1;
			}
			else if (!strcmp(action,"enable")) {
				peer->disabled = 0;
			}
		}
		// Send XML CELLS
		getcachecells(peer,cell);
		char buf[5000] = "";
		sprintf( buf, "<peer>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6><c7>%s</c7><c8>%s</c8><c9>%s</c9>\n</peer>\n",encryptxml(cell[0]),encryptxml(cell[1]),encryptxml(cell[2]),encryptxml(cell[10]),encryptxml(cell[3]),encryptxml(cell[4]),encryptxml(cell[5]),encryptxml(cell[6]),encryptxml(cell[7]),encryptxml(cell[8]),encryptxml(cell[9]) );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Cache"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>\n\nvar requestError = 0;\n\nfunction makeHttpRequest( url, xml, callFunction, param )\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				if(xml) {\n					eval( callFunction+'(httpRequest.responseXML,param)');\n				}\n				else {\n					eval( callFunction+'(httpRequest.responseText,param)');\n				}\n			}\n			else {\n				requestError = 1;\n			}\n		}\n	}\n	httpRequest.open('GET',url,true);\n	httpRequest.send(null);\n}\n\nvar currentid=0;\n\nfunction xmlupdatepeer( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n	row.cells.item(9).innerHTML = xmlDoc.getElementsByTagName('c9')[0].childNodes[0].nodeValue;\n}\n\nfunction peeraction(id, action)\n{\n	return makeHttpRequest( '/cache?id='+id+'&action='+action , true, 'xmlupdatepeer', 'peer'+id);\n}\n\nfunction dotimer()\n{\n	if (!requestError) {\n		if (currentid>0) peeraction(currentid, 'refresh');\n		t = setTimeout('dotimer()',1000);\n	}\n}\n</script>\n");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<BODY onload=\"dotimer();\">");
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	if (cfg.cachesock>0) { sprintf( http_buf, "<br>Cache Server [<font color=#00ff00>ENABLED</font>]");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) ); }
	else {
		sprintf( http_buf, "<br>Cache Server [<font color=#ff0000>DISABLED</font>]");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	sprintf( http_buf,"<ul><li>Port = %d</li><li>Total Peers = %d</li>",cfg.cacheport,totalcachepeers() );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<li>Total Requests = %d</li>",cfg.cachereq);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<li>Total Replies = %d (%d%%)</li></ul>", cfg.cacherep, (cfg.cacherep*100)/(cfg.cachereq+1) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<center><table class=yellow width=100%%><tr><th width=200px>Host</th><th width=100px>IP Address</th><th width=80px>Program</th><th width=30px>Ping</th><th width=90px>Requests</th><th width=90px>Replies</th><th width=90px>Forwarded Hits</th><th width=90px>Cache Hits/Total</th><th width=80px>Instant Cache</th><th>Last Used Cache</th></tr>\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	peer = cfg.cachepeer;
	int32_t alt=0;
	while (peer) {
		if (alt==1) alt=2; else alt=1;
		getcachecells(peer, cell);
		sprintf( http_buf,"<tr id=\"peer%d\" class=alt%d onMouseOver='currentid=%d'><td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",peer->id,alt,peer->id,cell[0],cell[1],cell[2],cell[10],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8],cell[9]);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		peer = peer->next;
	}

	// Total
	sprintf( http_buf,"<tr class=alt3><td align=right>Total</td><td colspan=3>%d</td>",totalcachepeers());
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	int32_t totreq = 0;
	int32_t totreqok = 0;
	int32_t totrepok = 0;
	int32_t tothits = 0;
	int32_t totfwd = 0;
	int32_t totihits = 0;
	peer = cfg.cachepeer;
	while (peer) {
		totreq += peer->reqnb;
		totreqok += peer->reqok;
		totrepok += peer->repok;
		tothits += peer->hitnb;
		totihits += peer->ihitnb;
		totfwd += peer->hitfwd;
		peer = peer->next;
	}
	sprintf( http_buf,"<td>%d</td>",totreq); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
//	tcp_writeecmdata(&tcpbuf, sock, totreqok, totreq);
	sprintf( http_buf,"<td>%d</td>",totrepok); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>",totfwd); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writeecmdata(&tcpbuf, sock, tothits, cfg.cachehits);
	tcp_writeecmdata(&tcpbuf, sock, totihits, tothits);
	sprintf( http_buf,"<td> </td></tr>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "</table></center><br>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_flush(&tcpbuf, sock);
}


///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
void getprofilecells(struct cardserver_data *cs, char cell[11][512])
{
	char temp[512];
	// CELL0 # Profile name
	if (cs->handle>0)
	sprintf( cell[0],"<a href=\"/profile?id=%d\">%s</a>", cs->id, cs->name);
	else
	sprintf( cell[0],"<a href=\"/newcamd?id=%d\">%s</a>", cs->id, cs->client);
	// CELL1 # Port
	sprintf( cell[1],"<a href=\"/newcamd?pid=%d\">%d</a>", cs->id, cs->port);
	if (cs->handle>0)
	sprintf( cell[10],"online"); 
	else
	sprintf( cell[10],"offline");
	// CELL2 # Ecm Time
	if (cs->ecmok)
	sprintf( cell[2],"%d ms",(cs->ecmoktime/cs->ecmok) );
	else
	sprintf( cell[2],"-- ms");
	// CELL3 # TotalECM
	int32_t ecmnb = cs->ecmaccepted+cs->ecmdenied;
	sprintf( cell[3], "%d", ecmnb );
	// CELL4 # AcceptedECM
	getstatcell( cs->ecmaccepted, ecmnb, cell[4] );
	// CELL5 # ECM OK
	getstatcell( cs->ecmok, cs->ecmaccepted, cell[5] );
	// CELL6 # CacheHits
	getstatcell( cs->cachehits, cs->ecmok, cell[6] );
	// CELL7 # Cache iHits
	getstatcell( cs->cacheihits, cs->cachehits, cell[7] );
	// CELL8 # Clients
	int32_t i=0;
	int32_t j=0;
	struct cs_client_data *usr = cs->client;
	while (usr) {
		i++;
		if (usr->handle>0) j++;
		usr = usr->next;
	}
	getstatcell2(j,i,cell[8]);
	// CELL9 # Caid:Providers
	sprintf( cell[9],"<b>%04X:</b> %x ",cs->card.caid,cs->card.prov[0]);
	for(i=1; i<cs->card.nbprov; i++) {
		sprintf( temp,",%x ",cs->card.prov[i]);
		strcat( cell[9], temp );
	}
}


void http_send_profiles(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;

	char cell[11][512];

	int32_t i;
	char *id = isset_get( req, "id");
	// Get Peer ID
	if (id)	{
		i = atoi(id);
		//look for server
		struct cardserver_data *cs = cfg.cardserver;
		while (cs) {
			if (cs->id==(uint32)i) break;
			cs = cs->next;
		}
		if (!cs) return;
		// Send XML CELLS
		getprofilecells(cs,cell);
		char buf[5000] = "";
		sprintf( buf, "<profile>\n<c0>%s</c0>\n<c1_c>%s</c1_c>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n<c9>%s</c9>\n</profile>\n",encryptxml(cell[0]),encryptxml(cell[10]),encryptxml(cell[1]),encryptxml(cell[2]),encryptxml(cell[3]),encryptxml(cell[4]),encryptxml(cell[5]),encryptxml(cell[6]),encryptxml(cell[7]),encryptxml(cell[8]),encryptxml(cell[9]) );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Profiles"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>\n\nvar requestError = 0;\n\nfunction makeHttpRequest( url, xml, callFunction, param )\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				if(xml) {\n					eval( callFunction+'(httpRequest.responseXML,param)');\n				}\n				else {\n					eval( callFunction+'(httpRequest.responseText,param)');\n				}\n			}\n			else {\n				requestError = 1;\n			}\n		}\n	}\n	httpRequest.open('GET',url,true);\n	httpRequest.send(null);\n}\n\nvar currentid=0;\n\nfunction xmlupdateprofile( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).className = xmlDoc.getElementsByTagName('c1_c')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n	row.cells.item(9).innerHTML = xmlDoc.getElementsByTagName('c9')[0].childNodes[0].nodeValue;\n}\n\nfunction profileaction(id, action)\n{\n	return makeHttpRequest( '/profiles?id='+id+'&action='+action , true, 'xmlupdateprofile', 'profile'+id);\n}\n\nfunction dotimer()\n{\n	if (!requestError) {\n		if (currentid>0) profileaction(currentid, 'refresh');\n		t = setTimeout('dotimer()',1000);\n	}\n}\n</script>\n");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<BODY onload=\"dotimer();\">");
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	sprintf( http_buf, "<br>Total Profiles: %d", total_profiles() ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><center><table class=yellow width=100%%><tr><th width=150px>Profile name</th><th width=50px>Port</th><th width=60px>EcmTime</th><th width=60px>TotalECM</th><th width=90px>AcceptedECM</th><th width=80px>Ecm OK</th><th width=80px>Cache Hits</th><th width=80px>Instant Hits</th><th width=80px>Clients</th><th>Caid:Providers</th></tr>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	struct cardserver_data *cs = cfg.cardserver;
	int32_t alt=0;
	while(cs) {
		if (alt==1) alt=2; else alt=1;
		getprofilecells( cs, cell );
		sprintf( http_buf,"<tr id=\"profile%d\" class=alt%d onMouseOver='currentid=%d'><td>%s</td><td class=%s>%s</td><td align=center>%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cs->id,alt,cs->id,cell[0],cell[10],cell[1],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8],cell[9]);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		cs = cs->next;
	}

	// Total
	if (alt==1) alt=2; else alt=1;
	sprintf( http_buf,"<tr class=alt3><td align=right>Total</td><td align=center>%d</td><td align=center>--</td>",total_profiles()); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	int32_t totecm = 0;
	int32_t totecmaccepted = 0;
	int32_t totecmok = 0;
	int32_t totcachehits = 0;
	int32_t totcacheihits = 0;
	cs = cfg.cardserver;
	while(cs) {
		totecm += cs->ecmaccepted+cs->ecmdenied;
		totecmaccepted += cs->ecmaccepted;
		totecmok += cs->ecmok;
		totcachehits += cs->cachehits;
		totcacheihits += cs->cacheihits;
		cs = cs->next;
	}
	sprintf( http_buf,"<td align=center>%d</td>",totecm); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writeecmdata(&tcpbuf, sock, totecmaccepted, totecm);
	tcp_writeecmdata(&tcpbuf, sock, totecmok, totecmaccepted);
	tcp_writeecmdata(&tcpbuf, sock, totcachehits, totecmok);
	tcp_writeecmdata(&tcpbuf, sock, totcacheihits, totcachehits);
	sprintf( http_buf, "<td colspan=2> </td></tr>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Speed
	uint ticks = GetTickCount()/1000;
	sprintf( http_buf,"<tr class=alt2><td align=right>Average speed</td><td colspan=2 align=center>(per minute)</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td align=center>%d</td>", totecm*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>", totecmaccepted*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>", totecmok*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>", totcachehits*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>", totcacheihits*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<td colspan=2> </td></tr>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "</table></center>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_flush(&tcpbuf, sock);
}

///////////////////////////////////////////////////////////////////////////////
// PROFILE
///////////////////////////////////////////////////////////////////////////////

void cs_clients( struct cardserver_data *cs, int32_t *total, int32_t *connected, int32_t *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct cs_client_data *cli=cs->client;
	while (cli) {
		(*total)++;
		if (cli->handle>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}


#ifdef RADEGAST_SRV

int32_t connected_radegast_clients(struct cardserver_data *cs)
{
	int32_t nb=0;
	struct rdgd_client_data *rdgdcli=cs->rdgd.client;
	if (cs->rdgd.handle)
	while (rdgdcli) {
		if (rdgdcli->handle>0) nb++;
		rdgdcli=rdgdcli->next;
	}
	return nb;
}

#endif

/*
 * resolve client type for newcamd protocol
 */
char *programid(unsigned int id)
{
	typedef struct {
		char name[13];
		unsigned int id;
	} tnewcamdprog; 

	static tnewcamdprog camdp[] =
	{
		{ "generic", 0x0000 },
		{ "Opticum", 0x02C2 },
		{ "rq-sssp-client-CS", 0x0665 },
		{ "rqcamd", 0x0666 },
		{ "rq-echo-client", 0x0667 },
		{ "rq-sssp-client-CW", 0x0669 },
		{ "JlsRq", 0x0769 },
		{ "AlexCS", 0x414C },
		{ "camd3", 0x4333 },
		{ "CCcam", 0x4343 },
		{ "Cardlink", 0x434C },
		{ "DiabloCam-UW", 0x4453 },
		{ "eyetvCamd", 0x4543 },
		{ "Octagon", 0x4765 },
		{ "LCE", 0x4C43 },
		{ "NextYE2k", 0x4E58 },
		{ "SBCL", 0x5342 },
		{ "Tecview", 0x5456 },
		{ "vdr-sc", 0x5644 },
		{ "WiCard", 0x5743 },
		{ "cx", 0x6378 },
		{ "Tvheadend", 0x6502 },
		{ "evocamd", 0x6576 },
		{ "gbox2CS", 0x6762 },
		{ "Kaffeine", 0x6B61 },
		{ "kpcs", 0x6B63 },
		{ "mpcs", 0x6D63 },
		{ "mgcamd", 0x6D67 },
		{ "NextYE2k", 0x6E65 },
		{ "NewCS", 0x6E73 },
		{ "radegast", 0x7264 },
		{ "Scam", 0x7363 },
		{ "WinCSC", 0x7763 },
		{ "tsdecrypt", 0x7878 },
		{ "OScam", 0x8888 },
		{ "NCam", 0x6e63 },
		{ "Acamd", 0x9911 },
		{ "DVBplug", 0x9922 },
		{ "NewBox", 0x4E78 },
		{ NULL, 0xFFFF }
	};
	static char unknown[] = "Unknown";
	unsigned int i;
	id = id & 0xffff;
	for( i=0; i<sizeof(camdp)/sizeof(tnewcamdprog); i++ )
		if (camdp[i].id==id) return camdp[i].name;
	return unknown;
}

char* str_laststatus[] = { "NOK", "OK" };


///////////////////////////////////////////////////////////////////////////////

void getnewcamdclientcells(struct cardserver_data *cs, struct cs_client_data *cli, char cell[10][512])
{
	char temp[512];
	// CELL0 # User name
	sprintf( cell[0],"<a href='/newcamdclient?pid=%d&amp;id=%d'>%s</a>", cs->id, cli->id, cli->user);
	// CELL1 # PROGRAM ID
	if (cli->handle>0)
		sprintf( cell[1],"%s",programid(cli->progid));
	else
		cell[1][0]=0;
	// CELL2 # IP
	if (cli->handle>0)
		sprintf( cell[2], "%s", (char*)ip2string(cli->ip) );
	else
		cell[2][0] = 0;
	// CELL3 # CONNECTION TIME
	if (cli->handle>0) {
		uint d = (GetTickCount()-cli->connected)/1000;
		if (cli->ecm.busy) sprintf( cell[9], "busy"); else sprintf( cell[9], "online");
		sprintf( cell[3],"%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	}
	else {
		sprintf( cell[9], "offline");
		sprintf( cell[3], "offline");
	}
	// ECM STAT
	if (cli->cachedcw) sprintf( cell[4], "%d [%d]", cli->ecmnb, cli->cachedcw); else sprintf( cell[4], "%d", cli->ecmnb );
	//
	int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[5]);
	getstatcell( cli->ecmok, ecmaccepted, cell[6]);
	// Ecm Time
	if (cli->ecmok)
		sprintf( cell[7],"%d ms",(cli->ecmoktime/cli->ecmok) );
	else
		sprintf( cell[7],"-- ms");

	//Last Used Share
	if ( cli->ecm.lastcaid ) {
		if (cli->ecm.laststatus) sprintf( cell[8],"<span class=success>"); else sprintf( cell[8],"<span class=failed>");
		sprintf( temp,"ch %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		strcat( cell[8], temp);
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string(cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, " /", temp);
				strcat( cell[8], temp);
			}
		}
		strcat( cell[8], "</span>");
	}
	else {
		cell[8][0] = 0;
	}
}

struct cs_client_data *getnewcamdclientbyid(struct cardserver_data *cs, uint32 id);

void http_send_newcamd(int32_t sock, http_request *req)
{
	char http_buf[1024];
	char cell[10][512];
	struct tcp_buffer_data tcpbuf;
	struct cs_client_data *cli;

	// Profile ID
	char *pid = isset_get( req, "pid");
	int32_t i;
	if (pid)
	i = atoi(pid); else i=0;
	struct cardserver_data *cs = getcsbyid(i);
	if (!cs) {
		tcp_init(&tcpbuf);
		tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, http_title, "Newcamd"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
		tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );
		sprintf( http_buf, "<br>Profile not found (id=%d)<br>", i);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	char *id = isset_get( req, "id");
	if (id)	{ // XML
		int32_t i = atoi(id);
		struct cs_client_data *cli = getnewcamdclientbyid(cs, i);
		if (!cli) return;
		// Action
		char *action = isset_get( req, "action");
		if (action) {
			if (!strcmp(action,"disable")) {
				cli->disabled = 1;
				cs_disconnect_cli(cli);
			}
			else if (!strcmp(action,"enable")) {
				cli->disabled = 0;
			}
		}

		// Send XML CELLS
		getnewcamdclientcells(cs,cli,cell);
		char buf[5000] = "";
		sprintf( buf, "<newcamd>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n</newcamd>\n",encryptxml(cell[0]),encryptxml(cell[1]),encryptxml(cell[2]),encryptxml(cell[9]),encryptxml(cell[3]),encryptxml(cell[4]),encryptxml(cell[5]),encryptxml(cell[6]),encryptxml(cell[7]),encryptxml(cell[8]) );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Newcamd"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );

	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>\n\n");
	sprintf( http_buf, "var profileid = %d;\n", cs->id); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
//tcp_writestr(&tcpbuf, sock, "var requestError = 0;\n\nfunction makeHttpRequest( url, xml, callFunction, param )\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				if(xml) {\n					eval( callFunction+'(httpRequest.responseXML,param)');\n				}\n				else {\n					eval( callFunction+'(httpRequest.responseText,param)');\n				}\n			}\n			else {\n				requestError = 1;\n			}\n		}\n	}\n	httpRequest.open('GET',url,true);\n	httpRequest.send(null);\n}\n\nvar currentid=0;\n\nfunction xmlupdatenewcamd( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n	t = setTimeout('dotimer()',1000);\n}\n\nfunction newcamdaction(id, action)\n{\n	return makeHttpRequest( '/newcamd?pid='+profileid+'&id='+id+'&action='+action , true, 'xmlupdatenewcamd', 'cli'+id);\n}\n\nfunction dotimer()\n{\n	if (!requestError) {\n		if (currentid>0) newcamdaction(currentid, 'refresh'); else t = setTimeout('dotimer()',1000);\n	}\n}\n</script>\n");
	tcp_writestr(&tcpbuf, sock, "var requestError = 0;\n\nfunction makeHttpRequest( url, xml, callFunction, param )\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				if(xml) {\n					eval( callFunction+'(httpRequest.responseXML,param)');\n				}\n				else {\n					eval( callFunction+'(httpRequest.responseText,param)');\n				}\n			}\n			else {\n				requestError = 1;\n			}\n		}\n	}\n	httpRequest.open('GET',url,true);\n	httpRequest.send(null);\n}\n\nvar currentid=0;\n\nfunction xmlupdatenewcamd( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	t = setTimeout('dotimer()',1000);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n}\n\nfunction newcamdaction(id, action)\n{\n	return makeHttpRequest( '/newcamd?pid='+profileid+'&id='+id+'&action='+action , true, 'xmlupdatenewcamd', 'cli'+id);\n}\n\nfunction dotimer()\n{\n	if (!requestError) {\n		if (currentid>0) newcamdaction(currentid, 'refresh'); else t = setTimeout('dotimer()',1000);\n	}\n}\n</script>\n");

	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<BODY onload=\"dotimer();\">");
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );


	sprintf( http_buf, "<span class='button'><a href='/newcamd?pid=%d'>Connected</a></span><span class='button'><a href='/newcamd?pid=%d&amp;list=active'>Active</a></span><span class='button'><a href='/newcamd?pid=%d&amp;list=all'>All Clients</a></span><br>", cs->id, cs->id, cs->id);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<br><b>Profile: <a href='/profile?id=%d'>%s</a></b><br>Newcamd Port = %d<br>", cs->id, cs->name, cs->port);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	//NEWCAMD CLIENTS
	if (cs->handle) {

		char *action = isset_get( req, "list");
		int32_t actionid = 0;
		if (action) {
			if (!strcmp(action,"active")) actionid = 1;
			else if (!strcmp(action,"all")) actionid = 2;
		}
		//
		int32_t total, connected, active;
		cs_clients( cs, &total, &connected, &active );
		if (actionid==1)
			sprintf( http_buf, "<br>Active Clients: <b>%d</b> / %d", active, connected);
		else if (actionid==2)
			sprintf( http_buf, "<br>Total Clients: %d", total);
		else
			sprintf( http_buf, "<br>Connected Clients: <b>%d</b> / %d", connected, total);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		sprintf( http_buf, "<br><table class=yellow width=100%%><tr><th width=170px>Client name</th><th width=70px>Program</th><th width=100px>IP Address</th><th width=100px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		cli = cs->client;
		int32_t alt=0;

		if (actionid==1) {
			while (cli) {
				if ( (cli->handle>0)&&((GetTickCount()-cli->ecm.recvtime) < 20000) ) {
					if (alt==1) alt=2; else alt=1;
					getnewcamdclientcells(cs,cli, cell);
					sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else if (actionid==2) {
			while (cli) {
				if (alt==1) alt=2; else alt=1;
				getnewcamdclientcells(cs,cli, cell);
				sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cli->next;
			}
		}
		else {
			while (cli) {
				if (cli->handle>0) {
					if (alt==1) alt=2; else alt=1;
					getnewcamdclientcells(cs,cli, cell);
					sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
	
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	tcp_flush(&tcpbuf, sock);
}


///////////////////////////////////////////////////////////////////////////////
void http_send_newcamd_client(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	char *pid = isset_get( req, "pid");
	char *id = isset_get( req, "id");
	if (!id || !pid) return; //error

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Newcamd Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	struct cardserver_data *cs = getcsbyid( atoi(pid) );
	struct cs_client_data *cli = getnewcamdclientbyid( cs, atoi(id) );
	if (!cs) {
		sprintf( http_buf, "Profile not found"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}
	if (!cli) {
		sprintf( http_buf, "Client not found"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	// INFO
	struct client_info_data *info = cli->info;
	while (info) {
		sprintf( http_buf,"<br><strong>%s: </strong>%s",info->name,info->value);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		info = info->next;
	}
	// NAME
	sprintf( http_buf,"<br><strong>User: </strong>%s",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

/*	// CCcam Version
	sprintf( http_buf,"<br><strong>CCcam: </strong>%s",cli->version );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
*/
	// IP/HOST
	sprintf( http_buf,"<br><strong>IP address: </strong>%s",(char*)ip2string(cli->ip) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	// Connection Time
	uint d = (GetTickCount()-cli->connected)/1000;
	sprintf( http_buf,"<br><strong>Connected: </strong>%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	// Ecm Stat
	int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<br><strong>Total ECM: </strong>%d", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Accepted ECM: </strong>%d", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Ecm OK: </strong>%d", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Cached DCW: </strong>%d", cli->cachedcw);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		sprintf( http_buf,"<br><strong>Ecm Time: </strong>%d ms",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	//Last Used Share
	if ( cli->ecm.lastcaid ) {

		sprintf( http_buf,"<br><br><fieldset><legend>Last Used share</legend>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		sprintf( http_buf, "<ul>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Decode Status
		if (cli->ecm.laststatus)
			sprintf( http_buf,"<li>Decode success");
		else
			sprintf( http_buf,"<li>Decode failed");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		sprintf( http_buf,"<li>Channel %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string(cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, "<li>", http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			// Last ECM
			ECM_DATA *ecm = getecmbyid(cli->ecm.lastid);
			// ECM
			sprintf( http_buf,"<li>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->ecm.laststatus) {
				sprintf( http_buf,"<li>DCW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( (ecm->lastdecode.status>0)&&(ecm->lastdecode.counter>0) ) {
				sprintf( http_buf,"<li>Last DCW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"<li>Total wrong DCW = %d", ecm->lastdecode.error); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (ecm->lastdecode.counter>2) {
					sprintf( http_buf,"<li>Total Consecutif DCW = %d<li>ECM Interval = %ds", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
			}
#endif
			//
			sprintf( http_buf, "</ul><br>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<table width='100%%' class='yellow'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='70px'>Elapsed time</th><th>DCW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int32_t i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct cs_server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						sprintf( http_buf,"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				sprintf( http_buf,"</table>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		sprintf( http_buf,"</fieldset>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = getecmbyid(cli->ecm.id);
		sprintf( http_buf,"<br><br><fieldset><legend>Current Ecm Request</legend>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"<li>Channel  %s", getchname(ecm->caid, ecm->provid, ecm->sid) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// ECM
		sprintf( http_buf,"<li>ECM(%d): ", ecm->ecmlen);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		array2hex( ecm->ecm, http_buf, ecm->ecmlen );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"</fieldset>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	tcp_flush(&tcpbuf, sock);
}



void http_send_profile(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	char *id = isset_get( req, "id");

	// Get Server
	int32_t i;
	if (id)	i = atoi(id); else i=0;

	struct cardserver_data *cs = cfg.cardserver;
	while (cs) {
		if (cs->id==(uint32)i) break;
		cs = cs->next;
	}

	if (!cs) {
		tcp_init(&tcpbuf);
		tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, http_title, "Profile"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
		tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

		sprintf( http_buf, "<br>Profile not found (id=%d)<br>", i);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Profile"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	sprintf( http_buf, "<span class='button'><a href='/newcamd?pid=%d'>Newcamd</a></span>", cs->id);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<br><br><div class=\"outer\"> <div class=\"top\"><b>Profile: %s</b><ul><li>Newcamd Port = %d</li>",cs->name, cs->port);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef RADEGAST_SRV
	sprintf( http_buf, "<li>Radegast Port = %d</li>", cs->rdgd.port);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	sprintf( http_buf, "<li>Network ID = %04X</li>", cs->onid);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<li>Caid = %04X</li><li>Providers =", cs->card.caid);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	for (i=0;i<cs->card.nbprov;i++) {
		sprintf( http_buf, " %06x",cs->card.prov[i]);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	sprintf( http_buf, "</li><li>Total ECM = %d</li> <li>Accepted ECM = %d</li><li>Ecm OK = %d</li><li>Ecm Time = %dms</li><li>Total Cache Hits = %d</li><li>Instant Cache Hits = %d</li></ul>", cs->ecmaccepted+cs->ecmdenied, cs->ecmaccepted, cs->ecmok, cs->ecmoktime/(cs->ecmok+1), cs->cachehits, cs->cacheihits);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "</div><div class=\"right\"><table class=option border=1px cellspacing=0><tr><th width=150px>Option</th><th width=50px>Value</th></tr>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf,"<tr><td>DCW TIME</td><td>%d</td></tr>", cs->dcwtime); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>DCW TIMEOUT</td><td class=small>%d</td></tr>", cs->dcwtimeout); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>DCW MAXFAILED</td><td class=small>%d</td></tr>", cs->maxfailedecm); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>SERVER MAX</td><td>%d</td></tr>", cs->csmax); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>SERVER INTERVAL</td><td>%d</td></tr>", cs->csinterval); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>SERVER TIMEOUT</td><td>%d</td></tr>", cs->cstimeout); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
//	sprintf( http_buf,"<tr><td>SERVER TIMEPERECM:</td><td>%d</td></tr>", cs->cstimeperecm); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>SERVER VALIDECMTIME</td><td>%d</td></tr>", cs->csvalidecmtime); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>SERVER FIRST</td><td>%d</td></tr>", cs->csfirst); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>RETRY NEWCAMD</td><td>%d</td></tr>", cs->csretry); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>RETRY CCCAM</td><td>%d</td></tr>", cs->ccretry); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<tr><td>CACHE TIMEOUT</td><td>%d</td></tr>", cs->cachetimeout); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "</table></div></div>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

/*
#ifdef RADEGAST_SRV
	struct rdgd_client_data *rdgdcli;
	if (cs->rdgd.handle && cs->rdgd.client) {
		//READEGAST CLIENTS
		sprintf( http_buf, "<br>Connected Radegast Clients: %d<br><table class=yellow width=100%%><tr><th width=100px>IP Address</th><th width=100px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>", connected_radegast_clients(cs));
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		rdgdcli = cs->rdgd.client;
		int32_t alt=0;
		while (rdgdcli) {
			if (rdgdcli->handle>0) {
				if (alt==1) alt=2; else alt=1;
				d = (GetTickCount()-rdgdcli->connected)/1000;
				if (rdgdcli->ecm.busy)
					sprintf( http_buf,"<tr class=alt%d><td>%s</td><td class=\"busy\">%02dd %02d:%02d:%02d</td>",alt,(char*)ip2string(rdgdcli->ip), d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
				else
					sprintf( http_buf,"<tr class=alt%d><td>%s</td><td class=\"online\">%02dd %02d:%02d:%02d</td>",alt,(char*)ip2string(rdgdcli->ip), d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

				sprintf( http_buf, "<td align=center>%d</td>", rdgdcli->ecmnb );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int32_t ecmaccepted = rdgdcli->ecmnb-rdgdcli->ecmdenied;
				tcp_writeecmdata(&tcpbuf, sock, ecmaccepted, rdgdcli->ecmnb);
				tcp_writeecmdata(&tcpbuf, sock, rdgdcli->ecmok, ecmaccepted);
				//Ecm Time
				if (rdgdcli->ecmok)
					sprintf( http_buf,"<td align=center>%d ms</td>",(rdgdcli->ecmoktime/rdgdcli->ecmok) );
				else
					sprintf( http_buf,"<td align=center>-- ms</td>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				//Last Used Share
				if ( rdgdcli->ecm.lastcaid ) {
					if (rdgdcli->ecm.laststatus) sprintf( http_buf,"<td class=success>"); else sprintf( http_buf,"<td class=failed>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					sprintf( http_buf,"ch %s (%dms) %s ", getchname(rdgdcli->ecm.lastcaid, rdgdcli->ecm.lastprov, rdgdcli->ecm.lastsid) , rdgdcli->ecm.lastdecodetime, str_laststatus[rdgdcli->ecm.laststatus] );
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					if ( (GetTickCount()-rdgdcli->ecm.recvtime) < 20000 ) {
						if (rdgdcli->ecm.lastdcwsrctype==DCW_SOURCE_SERVER) {
							struct cs_server_data *srv = getsrvbyid(rdgdcli->ecm.lastdcwsrcid);
							if (srv) {
								sprintf( http_buf," / from server (%s:%d)", srv->host->name, srv->port);
								tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							}
						}
						else if (rdgdcli->ecm.lastdcwsrctype==DCW_SOURCE_CACHE) {
							struct cs_cachepeer_data *peer = getpeerbyid(rdgdcli->ecm.lastdcwsrcid);
							if (peer) {
								sprintf( http_buf," / from cache peer (%s:%d)", peer->host->name, peer->port);
								tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							}
						}
					}
					sprintf( http_buf,"</td>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				else {
					sprintf( http_buf,"<td> </td>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				sprintf( http_buf,"</tr>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			rdgdcli = rdgdcli->next;
		}
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#endif

*/

	// Send Stat
	sprintf( http_buf, "<style type=\"text/css\">\n.mainborder\n{ background: #d2d2d2; border: 1px solid #0B198C; border-spacing: 0px; font: 10px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; padding: 1 2; }\n.redborder { background: #d25555; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n.greenborder { background: #55d255; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n.blueborder { background: #5555e2; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n</style>\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<style type=\"text/css\">\n.yellowborder { background: #FFFF00; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n</style>\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (cs->ecmok) {
		sprintf( http_buf, "\n<br><br><table class=\"mainborder\" width=100%%>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<tr><td><div class=\"redborder\" style=\"height: 10px; width: 10px;\"></div> </td><td width=100%%>Total DCW number</td></tr><tr><td><div class=greenborder style=\"height: 10px; width: 10px;\"></div> </td><td width=100%%>Number of DCW from servers</td></tr><tr><td><div class=blueborder style=\"height: 10px; width: 10px;\"></div> </td><td width=100%%>Number of DCW from cache</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//Get Max of ttime
		int32_t max=1;
		for(i=0; i<(cs->dcwtimeout/100); i++) if (max<cs->ttime[i]) max=cs->ttime[i];
		for(i=0; i<(cs->dcwtimeout/100); i++) {
			sprintf( http_buf, "<tr><td>%d.%ds</td><td>", i/10,i%10 );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// RED
			int32_t width = cs->ttime[i]*100/max;
			if (width>10)
				sprintf( http_buf, "<div class=redborder style='height:3px; width:%d%%'><span style=\"float: right;\">%d</span></div>", width, cs->ttime[i] );
			else
				sprintf( http_buf, "<div class=redborder style='height:3px; width:%d%%'></div>", width );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				// GREEN
				sprintf( http_buf, "<div class=greenborder style='height:3px; width:%d%%'></div>", (cs->ttimecards[i]*100/max) );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				// BLUE
				sprintf( http_buf, "<div class=blueborder style='height:3px; width:%d%%'></div>", (cs->ttimecache[i]*100/max) );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				// YELLOW
				sprintf( http_buf, "<div class=yellowborder style='height:3px; width:%d%%'></div>", (cs->ttimeclients[i]*100/max) );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				//
				sprintf( http_buf, "</td></tr>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		sprintf( http_buf, "</table><br>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	tcp_flush(&tcpbuf, sock);
}



#ifdef CCCAM_SRV

void cccam_clients( int32_t *total, int32_t *connected, int32_t *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct cc_client_data *cli=cfg.cccam.client;
	while (cli) {
		(*total)++;
		if (cli->handle>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}


void getcccamcells(struct cc_client_data *cli, char cell[10][512])
{
	char temp[512];

	// CELL0 # NAME
	if (cli->realname)
		sprintf( cell[0],"<a href='/cccamclient?id=%d'>%s<br>%s</a>",cli->id,cli->user,cli->realname);
	else
		sprintf( cell[0],"<a href='/cccamclient?id=%d'>%s</a>",cli->id,cli->user);

	// CELL1 # VERSION
	sprintf( cell[1],"%s",cli->version );

	// CELL2 # IP
	if (cli->host)
		sprintf( cell[2],"%s<br>%s",(char*)ip2string(cli->ip), cli->host->name );
	else
		sprintf( cell[2],"%s",(char*)ip2string(cli->ip) );

	// CELL3 # Connection Time
	if (cli->handle>0) {
		if (cli->ecm.busy) sprintf( cell[9],"busy"); else sprintf( cell[9],"online");
		uint d = (GetTickCount()-cli->connected)/1000;
		sprintf( cell[3], "%02dd %02d:%02d:%02d<br>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		if (cli->enddate.tm_year)
			sprintf( temp,"End:%d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		else
			sprintf( temp,"freeze: %d", cli->freeze);
		strcat( cell[3], temp );
	}
	else {
		sprintf( cell[9],"offline");
		sprintf( cell[3],"offline");
	}

	// CELL4+5+6 # ECM STAT: TOTAL/ACCEPTED/OK
	sprintf( cell[4], "%d", cli->ecmnb );

	int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[5]);
	getstatcell( cli->ecmok, ecmaccepted, cell[6]);

	// CELL7 # Ecm Time
	if (cli->ecmok) sprintf( cell[7],"%d ms",(cli->ecmoktime/cli->ecmok) ); else sprintf( cell[7],"-- ms");

	// CELL8 # Last Used Share
	sprintf( cell[8], "<span style='float:right;'>");
	if (cli->disabled) {
		sprintf( temp," <img title='Enable' src='enable.png' OnClick=\"cccamaction(%d,'enable');\">",cli->id);
		strcat( cell[8], temp );
	}
	else {
		sprintf( temp," <img title='disable' src='disable.png' OnClick=\"cccamaction(%d,'disable');\">",cli->id);
		strcat( cell[8], temp );
	}
	strcat( cell[8], "</span>");

	if ( cli->ecm.lastcaid ) {
		if (cli->ecm.laststatus)  strcat( cell[8],"<span class=success>"); else strcat( cell[8],"<span class=failed>");
		sprintf( temp,"ch %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		strcat( cell[8], temp );
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string( cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, " /", temp );
				strcat( cell[8], temp );
			}
		}
		strcat( cell[8], "</span>" );
	}
	//else cell[8][0] = 0;
}

struct cc_client_data *getcccamclientbyid(uint32 id);

void http_send_cccam(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	char cell[10][512];

	char *id = isset_get( req, "id");
	if (id)	{ // XML
		int32_t i;
		i = atoi(id);
		struct cc_client_data *cli = getcccamclientbyid(i);
		if (!cli) return;
		// Action
		char *action = isset_get( req, "action");
		if (action) {
			if (!strcmp(action,"disable")) {
				cli->disabled = 1;
				cc_disconnect_cli(cli);
			}
			else if (!strcmp(action,"enable")) {
				cli->disabled = 0;
			}
		}

		// Send XML CELLS
		getcccamcells(cli,cell);
		char buf[5000] = "";
		sprintf( buf, "<cccam>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n</cccam>\n",encryptxml(cell[0]),encryptxml(cell[1]),encryptxml(cell[2]),encryptxml(cell[9]),encryptxml(cell[3]),encryptxml(cell[4]),encryptxml(cell[5]),encryptxml(cell[6]),encryptxml(cell[7]),encryptxml(cell[8]) );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "CCcam"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>\n\nvar requestError = 0;\n\nfunction makeHttpRequest( url, xml, callFunction, param )\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				if(xml) {\n					eval( callFunction+'(httpRequest.responseXML,param)');\n				}\n				else {\n					eval( callFunction+'(httpRequest.responseText,param)');\n				}\n			}\n			else {\n				requestError = 1;\n			}\n		}\n	}\n	httpRequest.open('GET',url,true);\n	httpRequest.send(null);\n}\n\nvar currentid=0;\n\nfunction xmlupdatecccam( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n	t = setTimeout('dotimer()',1000);\n}\n\nfunction cccamaction(id, action)\n{\n	return makeHttpRequest( '/cccam?id='+id+'&action='+action , true, 'xmlupdatecccam', 'cli'+id);\n}\n\nfunction dotimer()\n{\n	if (!requestError) {\n		if (currentid>0) cccamaction(currentid, 'refresh'); else t = setTimeout('dotimer()',1000);\n	}\n}\n</script>\n");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<BODY onload=\"dotimer();\">");
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	sprintf( http_buf, "<span class='button'><a href='/cccam'>Connected</a></span><span class='button'><a href='/cccam?list=active'>Active</a></span><span class='button'><a href='/cccam?list=all'>All Clients</a></span><br>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );


	if (cfg.cccam.handle>0) { sprintf( http_buf, "<br>CCcam Server [<font color=#00ff00>ENABLED</font>]");tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) ); }
	else {
		sprintf( http_buf, "<br>CCcam Server [<font color=#ff0000>DISABLED</font>]");tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}
	sprintf( http_buf,"<tr><br>NodeID = %02x%02x%02x%02x%02x%02x%02x%02x", cfg.cccam.nodeid[0], cfg.cccam.nodeid[1], cfg.cccam.nodeid[2], cfg.cccam.nodeid[3], cfg.cccam.nodeid[4], cfg.cccam.nodeid[5], cfg.cccam.nodeid[6], cfg.cccam.nodeid[7]);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//
	sprintf( http_buf, "<br>Version = %s<br>Port = %d", cfg.cccam.version, cfg.cccam.port); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Clients
	char *action = isset_get( req, "list");
	int32_t actionid = 0;
	if (action) {
		if (!strcmp(action,"active")) actionid = 1;
		else if (!strcmp(action,"all")) actionid = 2;
	}
	int32_t total, connected, active;
	cccam_clients( &total, &connected, &active );
	if (actionid==1)
		sprintf( http_buf, "<br>Active Clients: <b>%d</b> / %d", active, connected);
	else if (actionid==2)
		sprintf( http_buf, "<br>Total Clients: %d", total);
	else
		sprintf( http_buf, "<br>Connected Clients: <b>%d</b> / %d", connected, total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Table
	sprintf( http_buf, "<br><center><table class=yellow width=100%%><tr><th width=180px>Client</th><th width=70px>version</th><th width=100px>ip</th><th width=100px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct cc_client_data *cli = cfg.cccam.client;
	int32_t alt=0;
	if (actionid==1) {
		while (cli) {
			if ( (cli->handle>0)&&((GetTickCount()-cli->ecm.recvtime) < 20000) ) {
				if (alt==1) alt=2; else alt=1;
				getcccamcells(cli,cell);
				sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			cli = cli->next;
		}
	}
	else if (actionid==2) {
		while (cli) {
			if (alt==1) alt=2; else alt=1;
			getcccamcells(cli,cell);
			sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			cli = cli->next;
		}
	}
	else {
		while (cli) {
			if (cli->handle>0) {
				if (alt==1) alt=2; else alt=1;
				getcccamcells(cli,cell);
				sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			cli = cli->next;
		}
	}

	sprintf( http_buf, "</table></center>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	tcp_flush(&tcpbuf, sock);
}

///////////////////////////////////////////////////////////////////////////////
void http_send_cccam_client(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	char *id = isset_get( req, "id");
	int32_t i;
	if (id)	i = atoi(id); else return; // error
	struct cc_client_data *cli = getcccamclientbyid(i);

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "CCcam Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	if (!cli) {
		sprintf( http_buf, "Client not found"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	// INFO
	struct client_info_data *info = cli->info;
	while (info) {
		sprintf( http_buf,"<br><strong>%s: </strong>%s",info->name,info->value);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		info = info->next;
	}
	// NAME
	sprintf( http_buf,"<br><strong>User: </strong>%s",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Real Name
/*	if (cli->realname) {
		sprintf( http_buf,"<br><strong>Name: </strong>%s",cli->realname);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}*/
	// CCcam Version
	sprintf( http_buf,"<br><strong>CCcam: </strong>%s",cli->version );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// IP/HOST
	sprintf( http_buf,"<br><strong>IP address: </strong>%s",(char*)ip2string(cli->ip) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
/*	// HOST
	if (cli->host) {
		sprintf( http_buf,"<br><strong>Host: </strong>%s", cli->host->name );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}*/
	// Connection Time
	uint d = (GetTickCount()-cli->connected)/1000;
	sprintf( http_buf,"<br><strong>Connected: </strong>%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
/*	//End date
	if (cli->enddate.tm_year) {
		sprintf( http_buf,"<br><strong>End date: </strong>%d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}*/
	// Ecm Stat
	int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<br><strong>Total ECM: </strong>%d", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Accepted ECM: </strong>%d", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Ecm OK: </strong>%d", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		sprintf( http_buf,"<br><strong>Ecm Time: </strong>%d ms",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Freeze
	sprintf( http_buf,"<br><strong>Total Freeze: </strong>%d", cli->freeze);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	//Last Used Share
	if ( cli->ecm.lastcaid ) {

		sprintf( http_buf,"<br><br><fieldset><legend>Last Used share</legend>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		sprintf( http_buf, "<ul>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Decode Status
		if (cli->ecm.laststatus)
			sprintf( http_buf,"<li>Decode success");
		else
			sprintf( http_buf,"<li>Decode failed");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		sprintf( http_buf,"<li>Channel %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string(cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, "<li>", http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			// Last ECM
			ECM_DATA *ecm = getecmbyid(cli->ecm.lastid);
			// ECM
			sprintf( http_buf,"<li>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->ecm.laststatus) {
				sprintf( http_buf,"<li>DCW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( (ecm->lastdecode.status>0)&&(ecm->lastdecode.counter>0) ) {
				sprintf( http_buf,"<li>Last DCW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"<li>Total wrong DCW = %d", ecm->lastdecode.error); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (ecm->lastdecode.counter>2) {
					sprintf( http_buf,"<li>Total Consecutif DCW = %d<li>ECM Interval = %ds", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
			}
#endif
			//
			sprintf( http_buf, "</ul><br>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<table width='100%%' class='yellow'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='70px'>Elapsed time</th><th>DCW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int32_t i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct cs_server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						sprintf( http_buf,"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				sprintf( http_buf,"</table>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		sprintf( http_buf,"</fieldset>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = getecmbyid(cli->ecm.id);
		sprintf( http_buf,"<br><br><fieldset><legend>Current Ecm Request</legend>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"<li>Channel  %s", getchname(ecm->caid, ecm->provid, ecm->sid) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// ECM
		sprintf( http_buf,"<li>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"</fieldset>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	tcp_flush(&tcpbuf, sock);
}


#ifdef FREECCCAM_SRV

void freecccam_clients( int32_t *total, int32_t *connected, int32_t *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct cc_client_data *cli=cfg.freecccam.client;
	while (cli) {
		(*total)++;
		if (cli->handle>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}

int32_t freecccam_connectedclients()
{
	int32_t nb=0;
	struct cc_client_data *cli=cfg.freecccam.client;
	while (cli) {
		if (cli->handle>0) nb++;
		cli=cli->next;
	}
	return nb;
}

void getfreecccamcells(struct cc_client_data *cli, char cell[10][512])
{
	char temp[512];
	// CELL0 # NAME
	if (cli->realname)
		sprintf( cell[0],"<a href='/freecccamclient?id=%d'>%s<br>%s</a>",cli->id,cli->user,cli->realname);
	else
		sprintf( cell[0],"<a href='/freecccamclient?id=%d'>%s</a>",cli->id,cli->user);
	// CELL1 # VERSION
	sprintf( cell[1],"%s",cli->version );
	// CELL2 # IP
	if (cli->host)
		sprintf( cell[2],"a href='/freecccamclient?id=%d'>%s<br>%s</a>",cli->id,(char*)ip2string(cli->ip), cli->host->name );
	else
		sprintf( cell[2],"<a href='/freecccamclient?id=%d'>%s</a>",cli->id,(char*)ip2string(cli->ip) );
	// CELL3 # Connection Time
	if (cli->handle>0) {
		if (cli->ecm.busy) sprintf( cell[9],"busy");
		else sprintf( cell[9],"online");
		uint d = (GetTickCount()-cli->connected)/1000;
		sprintf( cell[3], "%02dd %02d:%02d:%02d<br>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		if (cli->enddate.tm_year)
			sprintf( temp,"End:%d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		else
			sprintf( temp,"freeze: %d", cli->freeze);
		strcat( cell[3], temp );
	}
	else {
		sprintf( cell[9],"offline");
		sprintf( cell[3],"offline");
	}
	// CELL4+5+6 # ECM STAT: TOTAL/ACCEPTED/OK
	sprintf( cell[4], "%d", cli->ecmnb );
	int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[5]);
	getstatcell( cli->ecmok, ecmaccepted, cell[6]);
	// CELL7 # Ecm Time
	if (cli->ecmok) sprintf( cell[7],"%d ms",(cli->ecmoktime/cli->ecmok) ); else sprintf( cell[7],"-- ms");
	// CELL8 # Last Used Share
	sprintf( cell[8], "<span style='float:right;'>");
	if (cli->disabled) {
		sprintf( temp," <img title='Enable' src='enable.png' OnClick=\"freecccamaction(%d,'enable');\">",cli->id);
		strcat( cell[8], temp );
	}
	else {
		sprintf( temp," <img title='disable' src='disable.png' OnClick=\"freecccamaction(%d,'disable');\">",cli->id);
		strcat( cell[8], temp );
	}
	strcat( cell[8], "</span>");

	if ( cli->ecm.lastcaid ) {
		if (cli->ecm.laststatus)  strcat( cell[8],"<span class=success>"); else strcat( cell[8],"<span class=failed>");
		sprintf( temp,"ch %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		strcat( cell[8], temp );
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string( cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, " /", temp );
				strcat( cell[8], temp );
			}
		}
		strcat( cell[8], "</span>" );
	}
	//else cell[8][0] = 0;
}

struct cc_client_data *getfreecccamclientbyid(uint32_t id);

void http_send_freecccam(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	char cell[10][512];

	char *id = isset_get( req, "id");
	if (id)	{ // XML
		int32_t i;
		i = atoi(id);
		struct cc_client_data *cli = getfreecccamclientbyid(i);
		if (!cli) return;
		// Action
		char *action = isset_get( req, "action");
		if (action) {
			if (!strcmp(action,"disable")) {
				cli->disabled = 1;
				cc_disconnect_cli(cli);
			}
			else if (!strcmp(action,"enable")) {
				cli->disabled = 0;
			}
		}

		// Send XML CELLS
		getfreecccamcells(cli,cell);
		char buf[5000] = "";
		sprintf( buf, "<freecccam>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n</freecccam>\n",encryptxml(cell[0]),encryptxml(cell[1]),encryptxml(cell[2]),encryptxml(cell[9]),encryptxml(cell[3]),encryptxml(cell[4]),encryptxml(cell[5]),encryptxml(cell[6]),encryptxml(cell[7]),encryptxml(cell[8]) );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "FreeCCcam"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>\n\nvar requestError = 0;\n\nfunction makeHttpRequest( url, xml, callFunction, param )\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				if(xml) {\n					eval( callFunction+'(httpRequest.responseXML,param)');\n				}\n				else {\n					eval( callFunction+'(httpRequest.responseText,param)');\n				}\n			}\n			else {\n				requestError = 1;\n			}\n		}\n	}\n	httpRequest.open('GET',url,true);\n	httpRequest.send(null);\n}\n\nvar currentid=0;\n\nfunction xmlupdatefreecccam( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n	t = setTimeout('dotimer()',1000);\n}\n\nfunction freecccamaction(id, action)\n{\n	return makeHttpRequest( '/freecccam?id='+id+'&action='+action , true, 'xmlupdatefreecccam', 'cli'+id);\n}\n\nfunction dotimer()\n{\n	if (!requestError) {\n		if (currentid>0) freecccamaction(currentid, 'refresh'); else t = setTimeout('dotimer()',1000);\n	}\n}\n</script>\n");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<BODY onload=\"dotimer();\">");
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	/*sprintf( http_buf, "<span class='button'><a href='/freecccam'>Connected</a></span><span class='button'><a href='/freecccam?list=active'>Active</a></span><span class='button'><a href='/freecccam?list=all'>All Clients</a></span><br>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );*/


	if (cfg.freecccam.handle>0) { sprintf( http_buf, "<br>FreeCCcam Server [<font color=#00ff00>ENABLED</font>]");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) ); }
	else {
		sprintf( http_buf, "<br>FreeCCcam Server [<font color=#ff0000>DISABLED</font>]");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	/*sprintf( http_buf,"<tr><br>NodeID = %02x%02x%02x%02x%02x%02x%02x%02x", cfg.freecccam.nodeid[0], cfg.freecccam.nodeid[1], cfg.freecccam.nodeid[2], cfg.freecccam.nodeid[3], cfg.freecccam.nodeid[4], cfg.freecccam.nodeid[5], cfg.freecccam.nodeid[6], cfg.freecccam.nodeid[7]);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );*/
	//
	sprintf( http_buf, "<br>Port = %d", cfg.freecccam.port);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	/*sprintf( http_buf, "<br>Version = %s", cfg.cccam.version);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );*/

	// Table
	sprintf( http_buf, "<br>Connected Clients: %d<br><center><table class=yellow width=100%%><tr><th width=200px>Client ip</th><th width=70px>version</th><th width=100px>Connected</th><th width=60px>Freeze</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>", freecccam_connectedclients());
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct cc_client_data *cli = cfg.freecccam.client;
	int32_t alt=0;
	while (cli) {
		if (cli->handle>0) {
			if (alt==1) alt=2; else alt=1;
			getfreecccamcells(cli,cell);
			uint d = (GetTickCount()-cli->connected)/1000;
			if (cli->ecm.busy)
				sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d><td><a href='/freecccamclient?id=%d'>%s</a></td><td>%s</td><td class=\"busy\">%02dd %02d:%02d:%02d</td>",cli->id,alt,cli->id,(char*)ip2string(cli->ip),cli->version, d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
			else
				sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d><td><a href='/freecccamclient?id=%d'>%s</a></td><td>%s</td><td class=\"online\">%02dd %02d:%02d:%02d</td>",cli->id,alt,cli->id,(char*)ip2string(cli->ip),cli->version, d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// Freeze
			sprintf( http_buf,"<td align=center>%d</td>", cli->freeze);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// Ecm Stat
			int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
			sprintf( http_buf, "<td align=center>%d</td>", cli->ecmnb );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf, "<td align=center>%d</td>", ecmaccepted);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf, "<td align=center>%d</td>", cli->ecmok);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			//Ecm Time
			if (cli->ecmok)
				sprintf( http_buf,"<td align=center>%d ms</td>",(cli->ecmoktime/cli->ecmok) );
			else
				sprintf( http_buf,"<td align=center>-- ms</td>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

			//Last Used Share
			if ( cli->ecm.lastcaid ) {
				if (cli->ecm.laststatus) sprintf( http_buf,"<td class=success>"); else sprintf( http_buf,"<td class=failed>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"ch %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
					// From ???
					if (cli->ecm.laststatus) {
						src2string( cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, " /", http_buf );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				sprintf( http_buf,"</td>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			else {
				sprintf( http_buf,"<td> </td>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}

			sprintf( http_buf,"</tr>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		cli = cli->next;
	}

	sprintf( http_buf, "</table></center>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	tcp_flush(&tcpbuf, sock);
}

///////////////////////////////////////////////////////////////////////////////
void http_send_freecccam_client(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	char *id = isset_get( req, "id");
	int32_t i;
	if (id)	i = atoi(id); else return; // error
	struct cc_client_data *cli = getfreecccamclientbyid(i);

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "FreeCCcam Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	if (!cli) {
		sprintf( http_buf, "Client not found"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	// INFO
	struct client_info_data *info = cli->info;
	while (info) {
		sprintf( http_buf,"<br><strong>%s: </strong>%s",info->name,info->value);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		info = info->next;
	}
	// NAME
	/*sprintf( http_buf,"<br><strong>ID User: </strong>%s",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );*/
	// Real Name
/*	if (cli->realname) {
		sprintf( http_buf,"<br><strong>Name: </strong>%s",cli->realname);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}*/
	// CCcam Version
	sprintf( http_buf,"<br><strong>CCcam: </strong>%s",cli->version );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// IP/HOST
	sprintf( http_buf,"<br><strong>IP address: </strong>%s",(char*)ip2string(cli->ip) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
/*	// HOST
	if (cli->host) {
		sprintf( http_buf,"<br><strong>Host: </strong>%s", cli->host->name );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}*/
	// Connection Time
	uint d = (GetTickCount()-cli->connected)/1000;
	sprintf( http_buf,"<br><strong>Connected: </strong>%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
/*	//End date
	if (cli->enddate.tm_year) {
		sprintf( http_buf,"<br><strong>End date: </strong>%d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}*/
	// Ecm Stat
	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<br><strong>Total ECM: </strong>%d", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Accepted ECM: </strong>%d", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Ecm OK: </strong>%d", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		sprintf( http_buf,"<br><strong>Ecm Time: </strong>%d ms",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Freeze
	sprintf( http_buf,"<br><strong>Total Freeze: </strong>%d", cli->freeze);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Last Used Share
	if ( cli->ecm.lastcaid ) {

		sprintf( http_buf,"<br><br><fieldset><legend>Last Used share</legend>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		sprintf( http_buf, "<ul>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Decode Status
		if (cli->ecm.laststatus)
			sprintf( http_buf,"<li>Decode success");
		else
			sprintf( http_buf,"<li>Decode failed");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		sprintf( http_buf,"<li>Channel %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string(cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, "<li>", http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			// Last ECM
			ECM_DATA *ecm = getecmbyid(cli->ecm.lastid);
			// ECM
			sprintf( http_buf,"<li>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->ecm.laststatus) {
				sprintf( http_buf,"<li>DCW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( (ecm->lastdecode.status>0)&&(ecm->lastdecode.counter>0) ) {
				sprintf( http_buf,"<li>Last DCW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"<li>Total wrong DCW = %d", ecm->lastdecode.error); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (ecm->lastdecode.counter>2) {
					sprintf( http_buf,"<li>Total Consecutif DCW = %d<li>ECM Interval = %ds", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
			}
#endif
			//
			sprintf( http_buf, "</ul><br>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<table width='100%%' class='yellow'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='70px'>Elapsed time</th><th>DCW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct cs_server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						sprintf( http_buf,"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				sprintf( http_buf,"</table>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		sprintf( http_buf,"</fieldset>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = getecmbyid(cli->ecm.id);
		sprintf( http_buf,"<br><br><fieldset><legend>Current Ecm Request</legend>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"<li>Channel  %s", getchname(ecm->caid, ecm->provid, ecm->sid) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// ECM
		sprintf( http_buf,"<li>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"</fieldset>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	tcp_flush(&tcpbuf, sock);
}
#endif

#endif


#ifdef MGCAMD_SRV

struct mg_client_data *getmgcamdclientbyid(uint32 id);
void mg_disconnect_cli(struct mg_client_data *cli);

void getmgcamdcells(struct mg_client_data *cli, char cell[10][512])
{
	char temp[512];

	// CELL0 # NAME
	if (cli->realname)
		sprintf( cell[0],"<a href='/mgcamdclient?id=%d'>%s<br>%s</a>",cli->id,cli->user,cli->realname);
	else
		sprintf( cell[0],"<a href='/mgcamdclient?id=%d'>%s</a>",cli->id,cli->user);

	// CELL1 # PROGRAM ID
	sprintf( cell[1],"%s",programid(cli->progid) );

	// CELL2 # IP
	if (cli->host)
		sprintf( cell[2],"%s<br>%s",(char*)ip2string(cli->ip), cli->host->name );
	else
		sprintf( cell[2],"%s",(char*)ip2string(cli->ip) );

	// CELL3 # Connection Time
	if (cli->handle>0) {
		if (cli->ecm.busy) sprintf( cell[9],"busy"); else sprintf( cell[9],"online");
		uint d = (GetTickCount()-cli->connected)/1000;
		sprintf( cell[3], "%02dd %02d:%02d:%02d<br>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		if (cli->enddate.tm_year)
			sprintf( temp,"End:%d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		else
			sprintf( temp,"freeze: %d", cli->freeze);
		strcat( cell[3], temp );
	}
	else {
		sprintf( cell[9],"offline");
		sprintf( cell[3],"offline");
	}

	// CELL4+5+6 # ECM STAT: TOTAL/ACCEPTED/OK
	// ECM STAT

	if (cli->cachedcw) sprintf( cell[4], "%d [%d]", cli->ecmnb, cli->cachedcw); else sprintf( cell[4], "%d", cli->ecmnb );

	int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[5]);
	getstatcell( cli->ecmok, ecmaccepted, cell[6]);

	// CELL7 # Ecm Time
	if (cli->ecmok) sprintf( cell[7],"%d ms",(cli->ecmoktime/cli->ecmok) ); else sprintf( cell[7],"-- ms");

	// CELL8 # Last Used Share
	sprintf( cell[8], "<span style='float:right;'>");
	if (cli->disabled) {
		sprintf( temp," <img title='Enable' src='enable.png' OnClick=\"mgcamdaction(%d,'enable');\">",cli->id);
		strcat( cell[8], temp );
	}
	else {
		sprintf( temp," <img title='disable' src='disable.png' OnClick=\"mgcamdaction(%d,'disable');\">",cli->id);
		strcat( cell[8], temp );
	}
	strcat( cell[8], "</span>");

	if ( cli->ecm.lastcaid ) {
		if (cli->ecm.laststatus)  strcat( cell[8],"<span class=success>"); else strcat( cell[8],"<span class=failed>");
		sprintf( temp,"ch %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		strcat( cell[8], temp );
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string( cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, " /", temp );
				strcat( cell[8], temp );
			}
		}
		strcat( cell[8], "</span>" );
	}
	//else cell[8][0] = 0;
}


void mgcamd_clients( int32_t *total, int32_t *connected, int32_t *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct mg_client_data *cli=cfg.mgcamd.client;
	while (cli) {
		(*total)++;
		if (cli->handle>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}

void http_send_mgcamd(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	char cell[10][512];

	char *id = isset_get( req, "id");
	if (id)	{ // XML
		int32_t i;
		i = atoi(id);
		struct mg_client_data *cli = getmgcamdclientbyid(i);
		if (!cli) return;
		// Action
		char *action = isset_get( req, "action");
		if (action) {
			if (!strcmp(action,"disable")) {
				cli->disabled = 1;
				mg_disconnect_cli(cli);
			}
			else if (!strcmp(action,"enable")) {
				cli->disabled = 0;
			}
		}

		// Send XML CELLS
		getmgcamdcells(cli,cell);
		char buf[5000] = "";
		sprintf( buf, "<mgcamd>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n</mgcamd>\n",encryptxml(cell[0]),encryptxml(cell[1]),encryptxml(cell[2]),encryptxml(cell[9]),encryptxml(cell[3]),encryptxml(cell[4]),encryptxml(cell[5]),encryptxml(cell[6]),encryptxml(cell[7]),encryptxml(cell[8]) );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "MGcamd"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>\n\nvar requestError = 0;\n\nfunction makeHttpRequest( url, xml, callFunction, param )\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				if(xml) {\n					eval( callFunction+'(httpRequest.responseXML,param)');\n				}\n				else {\n					eval( callFunction+'(httpRequest.responseText,param)');\n				}\n			}\n			else {\n				requestError = 1;\n			}\n		}\n	}\n	httpRequest.open('GET',url,true);\n	httpRequest.send(null);\n}\n\nvar currentid=0;\n\nfunction xmlupdatemgcamd( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n	t = setTimeout('dotimer()',1000);\n}\n\nfunction mgcamdaction(id, action)\n{\n	return makeHttpRequest( '/mgcamd?id='+id+'&action='+action , true, 'xmlupdatemgcamd', 'cli'+id);\n}\n\nfunction dotimer()\n{\n	if (!requestError) {\n		if (currentid>0) mgcamdaction(currentid, 'refresh'); else t = setTimeout('dotimer()',1000);\n	}\n}\n</script>\n");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<BODY onload=\"dotimer();\">");
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	sprintf( http_buf, "<span class='button'><a href='/mgcamd'>Connected</a></span><span class='button'><a href='/mgcamd?list=active'>Active</a></span><span class='button'><a href='/mgcamd?list=all'>All Clients</a></span><br>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );


	if (cfg.mgcamd.handle>0) { sprintf( http_buf, "<br>MGcamd Server [<font color=#00ff00>ENABLED</font>]");tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) ); }
	else {
		sprintf( http_buf, "<br>MGcamd Server [<font color=#ff0000>DISABLED</font>]");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}
	// Port
	sprintf( http_buf, "<br>Port = %d", cfg.mgcamd.port); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Clients
	char *action = isset_get( req, "list");
	int32_t actionid = 0;
	if (action) {
		if (!strcmp(action,"active")) actionid = 1;
		else if (!strcmp(action,"all")) actionid = 2;
	}
	int32_t total, connected, active;
	mgcamd_clients( &total, &connected, &active );
	if (actionid==1)
		sprintf( http_buf, "<br>Active Clients: <b>%d</b> / %d", active, connected);
	else if (actionid==2)
		sprintf( http_buf, "<br>Total Clients: %d", total);
	else
		sprintf( http_buf, "<br>Connected Clients: <b>%d</b> / %d", connected, total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// table
	sprintf( http_buf, "<br><center><table class=yellow width=100%%><tr><th width=200px>Client</th><th width=70px>Program</th><th width=100px>ip</th><th width=100px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct mg_client_data *cli = cfg.mgcamd.client;
	int32_t alt=0;
	if (actionid==1) {
		while (cli) {
			if ( (cli->handle>0)&&((GetTickCount()-cli->ecm.recvtime) < 20000) ) {
				if (alt==1) alt=2; else alt=1;
				getmgcamdcells(cli,cell);
				sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			cli = cli->next;
		}
	}
	else if (actionid==2) {
		while (cli) {
			if (alt==1) alt=2; else alt=1;
			getmgcamdcells(cli,cell);
			sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			cli = cli->next;
		}
	}
	else {
		while (cli) {
			if (cli->handle>0) {
				if (alt==1) alt=2; else alt=1;
				getmgcamdcells(cli,cell);
				sprintf( http_buf,"<tr id=\"cli%d\" class=alt%d onMouseOver='currentid=%d'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			cli = cli->next;
		}
	}

	sprintf( http_buf, "</table></center>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	tcp_flush(&tcpbuf, sock);
}

///////////////////////////////////////////////////////////////////////////////

void http_send_mgcamd_client(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	char *id = isset_get( req, "id");
	int32_t i;
	if (id)	i = atoi(id); else return; // error
	struct mg_client_data *cli = getmgcamdclientbyid(i);

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Mgcamd Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	if (!cli) {
		sprintf( http_buf, "Client not found"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	// NAME
	sprintf( http_buf,"<br><strong>User: </strong>%s",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Real Name
	if (cli->realname) {
		sprintf( http_buf,"<br><strong>Name: </strong>%s",cli->realname);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Program ID
	sprintf( http_buf,"<br><strong>Program: </strong>%s",programid(cli->progid) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// IP/HOST
	sprintf( http_buf,"<br><strong>IP address: </strong>%s",(char*)ip2string(cli->ip) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// HOST
	if (cli->host) {
		sprintf( http_buf,"<br><strong>Host: </strong>%s", cli->host->name );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Connection Time
	uint d = (GetTickCount()-cli->connected)/1000;
	sprintf( http_buf,"<br><strong>Connected: </strong>%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//End date
	if (cli->enddate.tm_year) {
		sprintf( http_buf,"<br><strong>End date: </strong>%d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Ecm Stat
	int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<br><strong>Total ECM: </strong>%d", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Accepted ECM: </strong>%d", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<br><strong>Ecm OK: </strong>%d", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		sprintf( http_buf,"<br><strong>Ecm Time: </strong>%d ms",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Freeze
	sprintf( http_buf,"<br><strong>Total Freeze: </strong>%d", cli->freeze);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<br><strong>Cached DCW: </strong>%d", cli->cachedcw);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	//Last Used Share
	if ( cli->ecm.lastcaid ) {

		sprintf( http_buf,"<br><br><fieldset><legend>Last Used share</legend>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		sprintf( http_buf, "<ul>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Decode Status
		if (cli->ecm.laststatus)
			sprintf( http_buf,"<li>Decode success");
		else
			sprintf( http_buf,"<li>Decode failed");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		sprintf( http_buf,"<li>Channel %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string( cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, "<li>", http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			// Last ECM
			ECM_DATA *ecm = getecmbyid(cli->ecm.lastid);
			// ECM
			sprintf( http_buf,"<li>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->ecm.laststatus) {
				sprintf( http_buf,"<li>DCW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( (ecm->lastdecode.status>0)&&(ecm->lastdecode.counter>0) ) {
				sprintf( http_buf,"<li>Last DCW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"<li>Total wrong DCW = %d", ecm->lastdecode.error); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (ecm->lastdecode.counter>2) {
					sprintf( http_buf,"<li>Total Consecutif DCW = %d<li>ECM Interval = %ds", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
			}
#endif
			//
			sprintf( http_buf, "</ul><br>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<table width='100%%' class='yellow'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='70px'>Elapsed time</th><th>DCW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int32_t i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct cs_server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						sprintf( http_buf,"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				sprintf( http_buf,"</table>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		sprintf( http_buf,"</fieldset>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = getecmbyid(cli->ecm.id);
		sprintf( http_buf,"<br><br><fieldset><legend>Current Ecm Request</legend>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"<li>Channel  %s", getchname(ecm->caid, ecm->provid, ecm->sid) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// ECM
		sprintf( http_buf,"<li>ECM(%d): ", ecm->ecmlen);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		array2hex( ecm->ecm, http_buf, ecm->ecmlen );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"</fieldset>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	tcp_flush(&tcpbuf, sock);
}

#endif

//Newcamd Client
/*void cs_disconnect_cli(struct cs_client_data *cli);

void getnewcamdcells(struct cc_client_data *cli, char cell[10][512])
{
	char temp[512];
	// CELL0 # NAME
	if (cli->realname)
		sprintf( cell[0],"<a href='/newcamdclient?id=%d'>%s<br>%s</a>",cli->id,cli->user,cli->realname);
	else
		sprintf( cell[0],"<a href='/newcamdclient?id=%d'>%s</a>",cli->id,cli->user);
	// CELL1 # VERSION
	sprintf( cell[1],"%s",cli->version );
	// CELL2 # IP
	if (cli->host)
		sprintf( cell[2],"%s<br>%s",(char*)ip2string(cli->ip), cli->host->name );
	else
		sprintf( cell[2],"%s",(char*)ip2string(cli->ip) );
	// CELL3 # Connection Time
	if (cli->handle>0) {
		if (cli->ecm.busy) sprintf( cell[9],"busy"); else sprintf( cell[9],"online");
		uint d = (GetTickCount()-cli->connected)/1000;
		sprintf( cell[3], "%02dd %02d:%02d:%02d<br>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		if (cli->enddate.tm_year)
			sprintf( temp,"End:%d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		else
			sprintf( temp,"freeze: %d", cli->freeze);
		strcat( cell[3], temp );
	}
	else {
		sprintf( cell[9],"offline");
		sprintf( cell[3],"offline");
	}
	// CELL4+5+6 # ECM STAT: TOTAL/ACCEPTED/OK
	sprintf( cell[4], "%d", cli->ecmnb );
	int32_t ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[5]);
	getstatcell( cli->ecmok, ecmaccepted, cell[6]);
	// CELL7 # Ecm Time
	if (cli->ecmok) sprintf( cell[7],"%d ms",(cli->ecmoktime/cli->ecmok) ); else sprintf( cell[7],"-- ms");
	// CELL8 # Last Used Share
	sprintf( cell[8], "<span style='float:right;'>");
	if (cli->disabled) {
		sprintf( temp," <img title='Enable' src='enable.png' OnClick=\"newcamdaction(%d,'enable');\">",cli->id);
		strcat( cell[8], temp );
	}
	else {
		sprintf( temp," <img title='disable' src='disable.png' OnClick=\"newcamdaction(%d,'disable');\">",cli->id);
		strcat( cell[8], temp );
	}
	strcat( cell[8], "</span>");

	if ( cli->ecm.lastcaid ) {
		if (cli->ecm.laststatus)  strcat( cell[8],"<span class=success>"); else strcat( cell[8],"<span class=failed>");
		sprintf( temp,"ch %s (%dms) %s ", getchname(cli->ecm.lastcaid, cli->ecm.lastprov, cli->ecm.lastsid) , cli->ecm.lastdecodetime, str_laststatus[cli->ecm.laststatus] );
		strcat( cell[8], temp );
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->ecm.laststatus) {
				src2string( cli->ecm.lastdcwsrctype, cli->ecm.lastdcwsrcid, " /", temp );
				strcat( cell[8], temp );
			}
		}
		strcat( cell[8], "</span>" );
	}
	//else cell[8][0] = 0;
}

void newcamd_clients( struct cardserver_data *cs, int32_t *total, int32_t *connected, int32_t *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct cs_client_data *cli=cs->client;
	while (cli) {
		(*total)++;
		if (cli->handle>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}

/// End Newcamd client
*/
#include "bmsearch.c"

void http_send_editor(int32_t sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, http_title, "Editor"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write(&tcpbuf, sock, http_menu, strlen(http_menu) );

	if (req->type==HTTP_POST) {
		// Check Content-Type
		char *content = isset_header(req, "Content-Type");
		if (!content) {
			debugf(" Invalid form\n");
			return;
		}
		// Parse Content-type
		if ( memcmp(content,"multipart/form-data",19) ) {
			debugf(" Invalid Content-type\n");
			return;
		}
		// Get ';'
		while (*content!=';') {
			if (*content==0)  {
				debugf(" Invalid header data\n");
				return;
			}
			content++;
		}
		content++;
		// Skip Spaces
		while (*content==' ') content++;
		// Get Boundry
		if ( memcmp(content,"boundary",8) ) {
			debugf(" Invalid Content-type\n");
			return;
		}
		// Get '='
		while (*content!='=') {
			if (*content==0)  {
				debugf(" Invalid header data\n");
				return;
			}
			content++;
		}
		content++;
		// Skip Spaces
		while (*content==' ') content++;
		// Get Boundary Value
		char boundary[255];
		sprintf( boundary, "\r\n--%s", content);
		//printf(" boundary: '%s'\n", boundary);

		// search for boundary in file
		content = req->dbf.data;

		while (content) {
			content = (char*) boyermoore_horspool_memmem( (uchar*)content, req->dbf.datasize-(content-(char*)req->dbf.data), (uchar*)boundary, strlen(boundary) );
			if (content) {
				content += strlen(boundary);
				if ( *content=='\r' && *(content+1)=='\n' ) {
					content+=2;
					// Get Content-Disposition
					// Content-Disposition: form-data; name="textedit"
					char *p = content;
					while (*p!='\r') p++;
					if ( *p=='\r' && *(p+1)=='\n' && *(p+2)=='\r' && *(p+3)=='\n' ) { // Good
						*p=0;
						//printf(" Content: '%s'\n", content);
						char *pdata = p+4;
						// search for newt boundary
						content = (char*)boyermoore_horspool_memmem( (uchar*)content, req->dbf.datasize-(content-(char*)req->dbf.data), (uchar*)boundary, strlen(boundary) );
						*content = 0;
						//printf(" the file is:\n-------------\n%s\n-------------\n", pdata); 
						// save
						FILE *cfgfd = fopen( config_file, "w");
						if (!cfgfd) {
							sprintf( http_buf, "<h2>Error opening config file</h2>");
						}
						else {
							fwrite( pdata, 1, content-pdata, cfgfd);
							fclose(cfgfd);
							sprintf( http_buf, "<script type=\"text/JavaScript\"><!--\nsetTimeout(\"location.href = '/editor';\",5000);\n--></script>\n<h3><center>Config is Successfully Saved</center></h3>");
						}
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
			}
		}
		tcp_flush(&tcpbuf, sock);
	}
	else {
		sprintf( http_buf, "<form enctype=\"multipart/form-data\"action=\"/editor\" method=\"post\"><center><textarea cols=\"40\" rows=\"10\" spellcheck=\"false\" name=\"textedit\">"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		FILE *fd = fopen(config_file, "r");
		while( !feof(fd) ) {
			int32_t len = fread(http_buf, 1, sizeof(http_buf), fd);
			if (len<=0) break;
			tcp_write(&tcpbuf, sock, http_buf, len );
		}
		sprintf( http_buf, "</textarea><br><input type=\"submit\" value=\"Save '%s'\"><br></center></form>",config_file);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
	}
}


int32_t atoint(char *index)
{
	int32_t n=0;
	while (*index)
	{
		if ( (*index<'0')||(*index>'9') ) return n;
		else n = n*10 + (*index - '0');
		index++;
	}
	return n;
}

#include "base64.c"


void *gererClient(int32_t *param )
{
	int32_t sock = *param;
	http_request req;

	struct pollfd pfd;
	pfd.fd = sock;
	pfd.events = POLLIN | POLLPRI;
	int32_t retval = poll(&pfd, 1, 2000);

	//printf("\n*Connexion de %s(%d)\n", tt, sock); // print pid
	dynbuf_init(&req.dbf, 1024);

	if ( retval>0 )
	if ( pfd.revents & (POLLIN|POLLPRI) ) 
	if ( parse_http_request(sock, &req) ) {

		int32_t auth=0;
		if ( (req.type==HTTP_GET)||(req.type==HTTP_POST) ) {
			// check for auth
			if (!cfg.http.user[0] || !cfg.http.user[0]) auth = 1;
			else {
				int32_t i;
				for(i=0; i<req.hdrcount; i++) {
					if( !strcmp(req.headers[i].name,"Authorization") ) {
						//printf("Authorization: %s\n", req.headers[i].value);
						//get auth type
						if (!memcmp(req.headers[i].value, "Basic ",6)) {
							// get encrypted login
							char pass[256];
							char realpass[256];
							base64_pdecode( &req.headers[i].value[6], pass);
							//printf("PASS: %s\n",pass);
							sprintf(realpass,"%s:%s", cfg.http.user, cfg.http.pass);
							if (!strcmp(pass,realpass)) auth=1;
						}
						break;
					}
				}
			}
			if ( auth ) {
				if (strcmp(req.path,"/")==0) http_send_index(sock,&req);
				else if (strcmp(req.path,"/profiles")==0) {
					http_send_profiles(sock,&req);
				}
				else if (strcmp(req.path,"/profile")==0) {
					http_send_profile(sock,&req);
				}
				else if (strcmp(req.path,"/newcamd")==0) {
					http_send_newcamd(sock,&req);
				}
				else if (strcmp(req.path,"/newcamdclient")==0) {
					http_send_newcamd_client(sock,&req);
				}
				else if (strcmp(req.path,"/servers")==0) {
					http_send_servers(sock,&req);
				}
				else if (strcmp(req.path,"/server")==0) {
					http_send_server(sock,&req);
				}
				else if (strcmp(req.path,"/cache")==0) {
					http_send_cache(sock,&req);
				}
				/*else if (strcmp(req.path,"/cacheclient")==0) {
					http_send_cache_client(sock,&req);
				}*/
#ifdef CCCAM_SRV
				else if (strcmp(req.path,"/cccam")==0) {
					http_send_cccam(sock,&req);
				}
				else if (strcmp(req.path,"/cccamclient")==0) {
					http_send_cccam_client(sock,&req);
				}
#endif
#ifdef FREECCCAM_SRV
				else if (strcmp(req.path,"/freecccam")==0) {
					http_send_freecccam(sock,&req);
				}
				else if (strcmp(req.path,"/freecccamclient")==0) {
					http_send_freecccam_client(sock,&req);
				}
#endif
#ifdef MGCAMD_SRV
				else if (strcmp(req.path,"/mgcamd")==0) {
					http_send_mgcamd(sock,&req);
				}
				else if (strcmp(req.path,"/mgcamdclient")==0) {
					http_send_mgcamd_client(sock,&req);
				}
#endif
				else if (strcmp(req.path,"/restart")==0) {
					http_send_restart(sock,&req);
				}
				else if (strcmp(req.path,"/editor")==0) {
					http_send_editor(sock,&req);
				}
				else if (strcmp(req.path,"/connect.png")==0) {
					http_send_image(sock, &req, connect_png, sizeof(connect_png), "png");
				}
				else if (strcmp(req.path,"/disconnect.png")==0) {
					http_send_image(sock, &req, disconnect_png, sizeof(disconnect_png), "png");
				}
				else if (strcmp(req.path,"/enable.png")==0) {
					http_send_image(sock, &req, enable_png, sizeof(enable_png), "png");
				}
				else if (strcmp(req.path,"/disable.png")==0) {
					http_send_image(sock, &req, disable_png, sizeof(disable_png), "png");
				}
				else if (strcmp(req.path,"/refresh.png")==0) {
					http_send_image(sock, &req, refresh_png, sizeof(refresh_png), "png");
				}
				else if (strcmp(req.path,"/favicon.png")==0) {
					http_send_image(sock, &req, favicon_png, sizeof(favicon_png), "png");
				}
			}
			else { // send( client_sock, (char*)data, strlen(data),0);
				//printf("%s\n", http_buf);
				struct tcp_buffer_data tcpbuf;
				char auth[] = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"WebInterface\"\r\nVary: Accept-Encoding\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>WebInterface</TITLE></HEAD><BODY><H2>Access forbidden, authorization required</H2></BODY></HTML>";
				tcp_init(&tcpbuf);
				tcp_write(&tcpbuf, sock, auth, strlen(auth) );
				tcp_flush(&tcpbuf, sock);
				//return;
			}
		}
	}

	dynbuf_free(&req.dbf);

	//printf("*Deconnexion de %s(%d)\n", tt, sock);

	if ( close(sock) ) debugf(" HTTP Server: socket close failed(%d)\n",sock);

	return NULL;
}



void *http_thread(void *param)
{
	int32_t client_sock;
	struct sockaddr_in client_addr;
	socklen_t socklen = sizeof(client_addr);

	while(1) {
		if (cfg.http.handle>0) {
			pthread_mutex_lock(&prg.lockhttp);

			struct pollfd pfd;
			pfd.fd = cfg.http.handle;
			pfd.events = POLLIN | POLLPRI;
			int32_t retval = poll(&pfd, 1, 3000);
			if ( retval>0 ) {
				if ( pfd.revents & (POLLIN|POLLPRI) ) {
					client_sock = accept(cfg.http.handle, (struct sockaddr*)&client_addr, /*(socklen_t*)*/&socklen);
					if ( client_sock<0 ) {
						debugf(" HTTP Server: Accept Error\n");
						break;
					}
					else {
						pthread_t cli_tid;
						create_prio_thread(&cli_tid, (threadfn)gererClient, &client_sock, 50);
					}
				}
			}
			else if (retval<0) {
				debugf(" THREAD HTTP: poll error %d(errno=%d)\n", retval, errno);
				usleep(50000);
			}
			pthread_mutex_unlock(&prg.lockhttp);
		}
		usleep(30000);
	}// While

	debugf("Exiting HTTP Thread\n");
	return NULL;
}

pthread_t http_tid;
int32_t start_thread_http()
{
	create_prio_thread(&http_tid, http_thread,NULL, 50);
	return 0;
}

