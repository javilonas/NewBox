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

extern struct timeval startime;

unsigned int GetTickCount();
unsigned int GetuTickCount();
unsigned int GetTicks(struct timeval *tv);
unsigned int getseconds();

struct table_average {
	uint32 tab[100];
	int itab;
};

void tabavg_init(struct table_average *t);
void tabavg_add(struct table_average *t, uint32 value);
uint32 tabavg_get(struct table_average *t);
