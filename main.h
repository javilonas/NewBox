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

void cs_disconnect_cli(struct cs_client_data *cli);
int cs_connect_cli(int sock, uint32 ip);
void cs_senddcw_cli(struct cs_client_data *cli); // msgid is stored into ecm
void cs_getclimsg();

void cs_disconnect_srv(struct cs_server_data *srv);
int cs_connect_srv(struct cs_server_data *cs);
void cs_sendecm_srv(struct cs_server_data *srv, unsigned int msgid, ECM_DATA *ecm);
void cs_getsrvmsg();

int cmp_cards( struct cs_card_data* card1, struct cs_card_data* card2);
struct cs_card_data *srv_findcard( struct cs_server_data *srv, struct cardserver_data *cs, uint16 ecmcaid, uint32 ecmprov);

void srv_cstatadd( struct cs_server_data *srv, int csid, int ok); //, int ecmoktime)

int sidata_getval(struct cs_server_data *srv, struct cardserver_data *cs, uint16 caid, uint32 prov, uint16 sid, struct cs_card_data **selcard );
void sidata_add(struct cs_server_data *srv, uint8 *nodeid, uint16 caid, uint32 prov, uint16 sid,int val);
int sidata_update(struct cs_server_data *srv, struct cardserver_data *cs, uint16 caid, uint32 prov, uint16 sid,int val);

struct cardserver_data *getcsbycaidprov( uint16 caid, uint32 prov);
struct cardserver_data *getcsbyid(uint32 id);
struct cardserver_data *getcsbyport(int port);

struct cs_card_data *srv_check_card( struct cs_server_data *srv, uint16 caid, uint32 prov );

