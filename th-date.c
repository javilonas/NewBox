////
// File: th-date.c
/////


void* thread_enddate(void *param)
{
	prg.pid_date = syscall(SYS_gettid);
	while(1) {
		pthread_mutex_lock(&prg.lockthreaddate);

		time_t nowtime = time(NULL);
		struct tm *nowtm = localtime(&nowtime);
		//strftime(buf, sizeof(buf), "%d %b %Y %H:%M", nowtm); printf(" Local Time = %s %d\n", buf, nowtm->tm_yday);

		// CCcam Clients
		struct cc_client_data *cli = cfg.cccam.client;
		while (cli) {
			if (cli->enddate.tm_year) {
				//strftime(buf, sizeof(buf), "%d %b %Y %H:%M", &cli->enddate); printf(" Client End date = %s\n", buf);
				if (!cli->disabled) {
					if (cli->enddate.tm_year < nowtm->tm_year) {
						cli->disabled = 1;
						cc_disconnect_cli(cli);
						debugf(" CCcam: Client Disabled '%s'\n", cli->user);
					}
					else if (cli->enddate.tm_year==nowtm->tm_year) {
						if (cli->enddate.tm_yday < nowtm->tm_yday) {
							cli->disabled = 1; // printf(" Client Disabled %s\n", cli->user);
							cc_disconnect_cli(cli);
							debugf(" CCcam: Client Disabled '%s'\n", cli->user);
						}
					}
				}
				else {
					if (cli->enddate.tm_year > nowtm->tm_year) {
						cli->disabled = 0; //printf(" Client Enabled %s\n", cli->user);
						debugf(" CCcam: Client Enabled '%s'\n", cli->user);
					}
					else if (cli->enddate.tm_year==nowtm->tm_year) {
						if (cli->enddate.tm_yday >= nowtm->tm_yday) {
							cli->disabled = 0; //printf(" Client Enabled %s\n", cli->user);
							debugf(" CCcam: Client Enabled '%s'\n", cli->user);
						}
					}
				}
			}
			cli = cli->next;
		}


		// MGcamd Clients
		struct mg_client_data *mgcli = cfg.mgcamd.client;
		while (mgcli) {
			if (mgcli->enddate.tm_year) {
				//strftime(buf, sizeof(buf), "%d %b %Y %H:%M", &mgcli->enddate); printf(" Client End date = %s\n", buf);
				if (!mgcli->disabled) {
					if (mgcli->enddate.tm_year < nowtm->tm_year) {
						mgcli->disabled = 1;
						mg_disconnect_cli(mgcli);
						debugf(" mgcamd: Client Disabled '%s'\n", mgcli->user);
					}
					else if (mgcli->enddate.tm_year==nowtm->tm_year) {
						if (mgcli->enddate.tm_yday < nowtm->tm_yday) {
							mgcli->disabled = 1; // printf(" Client Disabled %s\n", mgcli->user);
							mg_disconnect_cli(mgcli);
							debugf(" mgcamd: Client Disabled '%s'\n", mgcli->user);
						}
					}
				}
				else {
					if (mgcli->enddate.tm_year > nowtm->tm_year) {
						mgcli->disabled = 0; //printf(" Client Enabled %s\n", mgcli->user);
						debugf(" mgcamd: Client Enabled '%s'\n", mgcli->user);
					}
					else if (mgcli->enddate.tm_year==nowtm->tm_year) {
						if (mgcli->enddate.tm_yday >= nowtm->tm_yday) {
							mgcli->disabled = 0; //printf(" Client Enabled %s\n", mgcli->user);
							debugf(" mgcamd: Client Enabled '%s'\n", mgcli->user);
						}
					}
				}
			}
			mgcli = mgcli->next;
		}

		pthread_mutex_unlock(&prg.lockthreaddate);
		sleep(10);
	}
}

void start_thread_date()
{
	create_prio_thread(&prg.tid_date, (threadfn)thread_enddate, NULL, 50);
}

