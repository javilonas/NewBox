////
// File: th-srv.c
/////


void *cs_connect_srv_th(struct cs_server_data *srv)
{
	int fd;
	int keep = 9000;
	// --->> FAST THREAD
	srv->keepalivetime = GetTickCount();
	struct host_data *host = srv->host;
	uint ip = host->ip;
	if (!ip) ip = host->clip;
	fd =  CreateClientSockTcp_nonb(ip, srv->port);
	if (fd<0) {
		// Setup Host Checking ip time
		if ( host->checkiptime > (getseconds()+60) ) host->checkiptime = getseconds()+60;
		//
		srv->error = errno;
		debugf(" server: socket connection failed to server (%s:%d)\n", srv->host->name,srv->port);
		if (errno==ECONNREFUSED) {
			keep = 60000;
			static char msg[]= "No-one listening on the remote address";
			srv->statmsg = msg;
		}
		else if (errno==ENETUNREACH) {
			static char msg[]= "Network is unreachable";
			srv->statmsg = msg;
		}
		else if (errno==ETIMEDOUT) {
			keep = 5000;
			static char msg[]= "Timeout while attempting connection";
			srv->statmsg = msg;
		}
		else {
			static char msg[]= "socket connection failed";
			srv->statmsg = msg;
		}
		srv->keepalivesent = keep;
		return NULL;
	}
	if (srv->keepalivesent<60000) srv->keepalivesent = srv->keepalivesent+keep;
	srv->error = 0; // No error

	if (srv->type==TYPE_NEWCAMD) {
		if ( cs_connect_srv(srv,fd)!=0 ) {
			debugf(" server: connection failed to newcamd server (%s:%d)\n", srv->host->name, srv->port);
			close(fd);
		}
	}
#ifdef CCCAM_CLI
	else if (srv->type==TYPE_CCCAM) {
		if ( cc_connect_srv(srv,fd)!=0 ) {
			debugf(" server: connection failed to CCcam server (%s:%d)\n", srv->host->name, srv->port);
			close(fd);
		}
	}
#endif
#ifdef RADEGAST_CLI
	else if (srv->type==TYPE_RADEGAST) {
		if ( rdgd_connect_srv(srv,fd)!=0 ) {
			debugf(" server: connection failed to Radegast server (%s:%d)\n", srv->host->name, srv->port);
			close(fd);
		}
	}
#endif
	else close(fd);
	return NULL;
}


void *cs_connect_servers(void *param)
{
	struct cs_server_data *srv;

	//prg.pid_srv = syscall(SYS_gettid);

	while (1) {
		pthread_mutex_lock(&prg.locksrvth);
			uint ticks = GetTickCount();
			srv = cfg.server;
			while (srv) {
				if (!srv->disabled)
				if ( (srv->handle==INVALID_SOCKET) ) {
					if ( (srv->host->ip)||(srv->host->clip) ){
						if ( (srv->keepalivetime+srv->keepalivesent) < ticks ) {
							pthread_t srv_tid;
							create_prio_thread(&srv_tid, (threadfn)cs_connect_srv_th, srv, 50); // Lock server
							//usleep(50000);
						}
					}
					else {
						static char msg[]= "Invalid Address";
						srv->statmsg = msg;
					}
				}
				srv = srv->next;
			}

		pthread_mutex_unlock(&prg.locksrvth);
		sleep(5);
	}
	END_PROCESS = 1;
}


int start_thread_srv()
{
	create_prio_thread(&prg.tid_srv, (threadfn)cs_connect_servers, NULL, 50);
	return 0;
}

