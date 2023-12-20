/**
	Animation Relay Server: Server Header File

								anm_server.h v1.1 by Fumi.Iseki (C)2011
*/


#ifndef  __ANM_SERVER_H_
#define  __ANM_SERVER_H_




#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <sys/time.h>

#include "txml.h"

#include "anm_help.h"
#include "anm_log_server.h" 



#define  ANM_SERVER_VERSION 	"anm_server v1.2.0 (c) Fumi.Iseki and NSL"
#define  ANM_UDP_TIMEOUT    	10

#define  ANM_LOG_FILE   		if(LogFile!=NULL)

#define  print_log				fprint_message



#ifdef CYGWIN
	#define  ANM_CONFIG_FILE    "anm_server.conf"
#else
	#define  ANM_CONFIG_FILE    "/usr/local/etc/anm_server/anm_server.conf"
#endif





typedef struct __interval_timer
{
	int  time;
	int  lap;
} IntervalTimer;





//////////////////////////////////////////////////////////////////////////

extern int 		DebugMode;
extern  pid_t   CrntPID;

extern int 		MaxUdpDataPort;
extern int 		MinUdpDataPort;
extern int 		MaxIdleTime;

extern tList*   Allow_IPaddr;
extern char* 	Hosts_Allow_File;
extern char* 	Temp_File_Dir;
extern char* 	Data_Log_Dir;

extern FILE*	LogFile;

extern char*    GrpKeyPrefix;







//////////////////////////////////////////////////////////////////////////
// Functions

// anm_server.c
void    anm_server(int usock, char* gname, char* idkey, int logmode);
tList*  search_regist(tList* lp, int port, char* ip);
tList*  search_animation(tList* lp, char* uuid);

void    del_process_list(tList* pl, int pid);
void    anm_server_term(int signal);
void    anm_server_term_child(int signal);
void    anm_server_kill_child(int signal);


// anm_main.c
void   	anm_sigterm_process(int signal);
void  	anm_sigterm_child(int signal);


// anm_config.c
void   	read_config_file(char* fn);



#endif


