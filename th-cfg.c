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


#ifndef STAT

#ifdef INOTIFY
#include "inotify/inotify.h"
#include "inotify/inotify-syscalls.h"
#else
#include <sys/inotify.h>
#endif

void *reread_config_thread(void *param)
{
	int fd = inotify_init(); //1(IN_NONBLOCK);
	int wd = inotify_add_watch(fd,config_file, IN_CLOSE_WRITE|IN_IGNORED);
	struct inotify_event *event;
	char buf[1024];

	//prg.pid_cfg = syscall(SYS_gettid);

	while(1) {
		int len = read(fd,buf,1024);
		int changed = 0;
		int i = 0;
		while(i<len){
			event=(struct inotify_event *) &buf[i];
			//displayInotifyEvent(event);
			if (event->mask & IN_CLOSE_WRITE) changed = 1;
			if (event->mask & IN_IGNORED) wd = inotify_add_watch(fd,config_file, IN_CLOSE_WRITE|IN_IGNORED);
			i += sizeof(struct inotify_event) + event->len;
		}
		if (changed) {
			debugf(" Config file Changed...\n");
			// UPDATE, LOCK ONLY MAIN LOOPS
			// First Freeze Clients/Servers Connections

			pthread_mutex_lock(&prg.lockdnsth);
			pthread_mutex_lock(&prg.locksrvth);
			pthread_mutex_lock(&prg.locksrvcs);
#ifdef RADEGAST_SRV
			pthread_mutex_lock(&prg.lockrdgdsrv);
#endif
#ifdef MGCAMD_SRV
			pthread_mutex_lock(&prg.locksrvmg);
#endif
#ifdef CCCAM_SRV
			pthread_mutex_lock(&prg.locksrvcc);
#endif
#ifdef FREECCCAM_SRV
			pthread_mutex_lock(&prg.locksrvfreecc);
#endif

			pthread_mutex_lock(&prg.lockmain);
			pthread_mutex_lock(&prg.lockhttp);
			debugf(" Waiting connection threads termination\n");
			sleep(3); // Wait for connection threads
			debugf(" Locking main thread\n");
			pipe_lock( srvsocks[0] );
			pipe_lock( srvsocks[1] );
			usleep(100000); // wait for messages & lock

			reread_config(&cfg);
			check_config(&cfg);

			pthread_mutex_unlock(&prg.lockhttp);
			pthread_mutex_unlock(&prg.lockmain);

#ifdef FREECCCAM_SRV
			pthread_mutex_unlock(&prg.locksrvfreecc);
#endif
#ifdef CCCAM_SRV
			pthread_mutex_unlock(&prg.locksrvcc);
#endif
#ifdef MGCAMD_SRV
			pthread_mutex_unlock(&prg.locksrvmg);
#endif
#ifdef RADEGAST_SRV
			pthread_mutex_unlock(&prg.lockrdgdsrv);
#endif
			pthread_mutex_unlock(&prg.locksrvcs);
			pthread_mutex_unlock(&prg.locksrvth);
			pthread_mutex_unlock(&prg.lockdnsth);
		}
		usleep(30000);
	}
}


#else


#include <sys/stat.h>

void *reread_config_thread(void *param)
{
	struct stat config_stat;
	time_t config_mtime;
	char name[255];
	// Set UP Modification Time of Config file
	sprintf( name, "%s", config_file );
	stat( name, &config_stat);
	config_mtime = config_stat.st_mtime;

	//prg.pid_cfg = syscall(SYS_gettid);

	// Connect Clients 
	while (1) {
		sleep(10);
		stat( name, &config_stat);
		if (config_mtime != config_stat.st_mtime) {
			// wait for storing file
			do {
				config_mtime = config_stat.st_mtime;
				sleep(3);
				stat( name, &config_stat);
			} while (config_mtime != config_stat.st_mtime);
			debugf(" Config file Changed...\n");

			// UPDATE, LOCK ONLY MAIN LOOPS
			pthread_mutex_lock(&prg.lockdnsth);
			pthread_mutex_lock(&prg.locksrvth);
			pthread_mutex_lock(&prg.locksrvcs);
#ifdef RADEGAST_SRV
			pthread_mutex_lock(&prg.lockrdgdsrv);
#endif
#ifdef MGCAMD_SRV
			pthread_mutex_lock(&prg.locksrvmg);
#endif
#ifdef CCCAM_SRV
			pthread_mutex_lock(&prg.locksrvcc);
#endif
#ifdef FREECCCAM_SRV
			pthread_mutex_lock(&prg.locksrvfreecc);
#endif
			pthread_mutex_lock(&prg.lockmain);
			sleep(3); // Wait for connection threads
			pthread_mutex_lock(&prg.lockhttp);
			pipe_lock( srvsocks[0] );
			sleep(1);

			pipe_lock( cecm_pipe[1] );
			usleep(30000); // wait for messages & lock

			reread_config(&cfg);
			check_config(&cfg);
			sleep(1);
			pthread_mutex_unlock(&prg.lockhttp);
			pthread_mutex_unlock(&prg.lockmain);

#ifdef FREECCCAM_SRV
			pthread_mutex_unlock(&prg.locksrvfreecc);
#endif
#ifdef CCCAM_SRV
			pthread_mutex_unlock(&prg.locksrvcc);
#endif
#ifdef MGCAMD_SRV
			pthread_mutex_unlock(&prg.locksrvmg);
#endif
#ifdef RADEGAST_SRV
			pthread_mutex_unlock(&prg.lockrdgdsrv);
#endif
			pthread_mutex_unlock(&prg.locksrvcs);
			pthread_mutex_unlock(&prg.locksrvth);
			pthread_mutex_unlock(&prg.lockdnsth);

			// Test Locks


		}
	}
}

#endif

int start_thread_config()
{
	create_prio_thread(&prg.tid_cfg, (threadfn)reread_config_thread, NULL, 50);
	return 0;
}

