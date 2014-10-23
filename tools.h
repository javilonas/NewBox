
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
