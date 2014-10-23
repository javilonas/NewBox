
struct trace_data {
	char host[32];
	int port;
	unsigned int ip;
	int sock;
	struct sockaddr_in addr;
};

extern int flag_debugscr;
extern int flag_debugnet;
extern int flag_debugfile;
extern char debug_file[256];

extern int flag_debugtrace;
extern struct trace_data trace;

#define MAX_DBGLINES 27 // 50 default
extern char dbgline[MAX_DBGLINES][512];
extern int idbgline;

char* debugtime(char *str);
void debug(char *str);
void debugf(char *format, ...);
void debughex(uint8 *buffer, int len);

void fdebug(char *str);
void fdebugf(char *format, ...);

