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
#include <unistd.h>

#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <errno.h>

#include "common.h"
#include "debug.h"
#include "threads.h"

int create_prio_thread(pthread_t *tid, threadfn func, void *arg, int newprio) //0 to 99
{
  pthread_attr_t tattr;
  int ret;
  struct sched_param param;

  /* initialized with default attributes */
  ret = pthread_attr_init (&tattr);

  /* safe to get existing scheduling param */
  ret = pthread_attr_getschedparam (&tattr, &param);

  /* set the priority; others are unchanged */
  param.sched_priority = newprio;

  /* setting the new scheduling param */
  ret = pthread_attr_setschedparam (&tattr, &param);

	pthread_attr_setstacksize(&tattr, 1024*100);

  /* with new priority specified */
	while (1) {
		ret = pthread_create (tid, &tattr, func, arg);
		if (ret==EAGAIN) {
			usleep(10000);
			continue;
		}
		if (ret!=0) return 0;
		break;
	}
	pthread_detach(*tid);
	usleep(1000); // wait 1ms
	return ret;
}

