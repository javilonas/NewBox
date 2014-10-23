////
// File: srv-common.c
/////

void *connect_cli_thread(void *param)
{
	int clientsock = -1;
	struct sockaddr_in clientaddr;
	socklen_t socklen = sizeof(clientaddr);

	pthread_t srv_tid;

	prg.pid_concli = syscall(SYS_gettid);

	while(1) {

		pthread_mutex_lock(&prg.lockclientsthread);

		struct pollfd pfd[16];
		int pfdcount = 0;

#ifdef CCCAM_SRV
		if (cfg.cccam.handle>0) {
			cfg.cccam.ipoll = pfdcount;
			pfd[pfdcount].fd = cfg.cccam.handle;
			pfd[pfdcount].events = POLLIN | POLLPRI;
			pfdcount++;
		} else cfg.cccam.ipoll = -1;
#endif

#ifdef MGCAMD_SRV
		if (cfg.mgcamd.handle>0) {
			cfg.mgcamd.ipoll = pfdcount;
			pfd[pfdcount].fd = cfg.mgcamd.handle;
			pfd[pfdcount].events = POLLIN | POLLPRI;
			pfdcount++;
		} else cfg.mgcamd.ipoll = -1;
#endif

		struct cardserver_data *cs = cfg.cardserver;
		while (cs) {
			if (cs->handle>0) {
				cs->ipoll = pfdcount;
				pfd[pfdcount].fd = cs->handle;
				pfd[pfdcount].events = POLLIN | POLLPRI;
				pfdcount++;
			} else cs->ipoll = -1;
			cs = cs->next;
		}

		int retval = poll(pfd, pfdcount, 3000);

		if ( retval>0 ) {

			// Newcamd Server
			cs = cfg.cardserver;
			while (cs) {
				if ( (cs->handle>0)&&(cs->ipoll>=0)&&(cs->handle==pfd[cs->ipoll].fd) ) {
					if ( pfd[cs->ipoll].revents & (POLLIN|POLLPRI) ) {
						socklen = sizeof(clientaddr);
						clientsock = accept(cs->handle, (struct sockaddr*)&clientaddr, &socklen );
						if (clientsock<=0) {
							//if (errno == EAGAIN || errno == EINTR) continue;
							//else {
								debugf(" newcamd: Accept failed (errno=%d)\n",errno);
							//}
						}
						else {
							//debugf(" [%s] New Client Connection...%s\n", cs->name, ip2string(clientaddr.sin_addr.s_addr) );
							SetSocketKeepalive(clientsock); 
							struct cs_clicon *clicondata = malloc( sizeof(struct cs_clicon) );
							clicondata->cs = cs; 
							clicondata->sock = clientsock; 
							clicondata->ip = clientaddr.sin_addr.s_addr;
							//while(EAGAIN==pthread_create(&srv_tid, NULL, th_cs_connect_cli,clicondata)) sleepms(1); pthread_detach(&srv_tid);
							create_prio_thread(&srv_tid, (threadfn)th_cs_connect_cli, clicondata, 50);
						}
					}
				}
				cs = cs->next;
			}

#ifdef CCCAM_SRV
			if ( (cfg.cccam.handle>0)&&(cfg.cccam.ipoll>=0)&&(cfg.cccam.handle==pfd[cfg.cccam.ipoll].fd) ) {
				if ( pfd[cfg.cccam.ipoll].revents & (POLLIN|POLLPRI) ) {
					socklen = sizeof(clientaddr);
					clientsock = accept( cfg.cccam.handle, (struct sockaddr*)&clientaddr, &socklen );
					if ( clientsock<0 ) {
						debugf(" CCcam: Accept failed (errno=%d)\n",errno);
						sleepms(30);
					}
					else {
						SetSocketKeepalive(clientsock); 
						//debugf(" CCcam: new connection...\n");
						struct struct_clicon *clicondata = malloc( sizeof(struct struct_clicon) );
						clicondata->sock = clientsock; 
						clicondata->ip = clientaddr.sin_addr.s_addr;
						create_prio_thread(&srv_tid, (threadfn)cc_connect_cli, clicondata, 50);
					}
				}
			}
#endif


#ifdef MGCAMD_SRV
			if ( (cfg.mgcamd.handle>0)&&(cfg.mgcamd.ipoll>=0)&&(cfg.mgcamd.handle==pfd[cfg.mgcamd.ipoll].fd) ) {
				if ( pfd[cfg.mgcamd.ipoll].revents & (POLLIN|POLLPRI) ) {
					socklen = sizeof(clientaddr);
					clientsock = accept(cfg.mgcamd.handle, (struct sockaddr*)&clientaddr, &socklen );
					if (clientsock<0) {
						if (errno == EAGAIN || errno == EINTR) continue;
						else {
							debugf(" mgcamd: Accept failed (%d)\n",errno);
						}
					}
					else {			
						//debugf(" mgcamd: new client Connection(%d)...%s\n", clientsock, ip2string(clientaddr.sin_addr.s_addr) );
						SetSocketKeepalive(clientsock); 
						struct mg_clicon *clicondata = malloc( sizeof(struct mg_clicon) );
						clicondata->sock = clientsock; 
						clicondata->ip = clientaddr.sin_addr.s_addr;
						//while(EAGAIN==pthread_create(&srv_tid, NULL, th_cs_connect_cli,clicondata)) sleepms(1); pthread_detach(&srv_tid);
						create_prio_thread(&srv_tid, (threadfn)th_mg_connect_cli, clicondata, 50);
					}
				}
			}
#endif
		}

		pthread_mutex_unlock(&prg.lockclientsthread);
		sleepms(1);
	}
}

