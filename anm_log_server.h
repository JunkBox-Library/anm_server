/**
	Animation Log Server: Server Header File

								anm_log_server.h v1.0 by Fumi.Iseki (C)2012
*/


#ifndef  __ANM_LOG_SERVER_H_
#define  __ANM_LOG_SERVER_H_



#define  MinUdpLogPort	10000
#define  MaxUdpLogPort 	20000




//////////////////////////////////////////////////////////////////////////
// Functions

// anm_log_server.c
void    anm_log_server(int usock, char* gname);
void    anm_log_server_term(int signal);


void  init_joint_name(void);
void  clear_joint_name(void);

#endif


