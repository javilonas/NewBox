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

///////////////////////////////////////////////////////////////////////////////
// THREAD RESOLVE DNS
///////////////////////////////////////////////////////////////////////////////

void *dns_child_thread(struct host_data *host)
{
	unsigned int newip;

	pthread_mutex_lock(&prg.lockdns);
	newip = hostname2ip(host->name);
	pthread_mutex_unlock(&prg.lockdns);
	usleep(10000);
	if (!newip) {
		//debugf(" dns: failed to get address for %s\n",host->name);
		host->checkiptime = getseconds() + 60;
		host->ip = newip;
	}
	else if (newip!=host->ip) {
		host->checkiptime = getseconds() + 300;
		host->ip = newip;
		//debugf(" dns: %s --> %s\n", host->name, ip2string(host->ip));
	}
	else {
		host->checkiptime = getseconds() + 600;
		//debugf(" dns: %s == %s\n", host->name, ip2string(host->ip));
	}
	return NULL;
}

void *dns_thread(void *param)
{
	do {
		pthread_mutex_lock(&prg.lockdnsth);

		struct host_data *host = cfg.host;
		while (host) {
			if (host->checkiptime<=getseconds()) {
				pthread_t new_tid;
				create_prio_thread(&new_tid, (threadfn)dns_child_thread,host, 50);
				dns_child_thread( host );
			}
			host = host->next;
		}

		pthread_mutex_unlock(&prg.lockdnsth);
		sleep(10);
	} while (1);
}


int start_thread_dns()
{
	create_prio_thread(&prg.tid_dns, (threadfn)dns_thread, NULL, 50);
	return 0;
}

