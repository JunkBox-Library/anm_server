/*    
    Animation Relay Server: Display Help 

                                    anm_help.c v1.0  by Fumi.Iseki (C)2011
*/


#include "anm_help.h"

#include "anm_data.h"
#include "anm_server.h"



void  anm_help(FILE* fp)
{
    fprintf(fp, "\n");
    fprintf(fp, " anm_server  ");
    fprintf(fp, "[-p port] [-f config_file] [-u user_name] \n");
    fprintf(fp, "             [-pid pid_file] [-l [log_file]] [-d]\n");
    fprintf(fp, "             [-ver] [--help]\n");
    fprintf(fp, "\n");

    fprintf(fp, "        -p   : port number that client connects. default is %d\n", ANM_SERVER_PORT);
    fprintf(fp, "        -f   : configuration file. default is %s\n", ANM_CONFIG_FILE);
    fprintf(fp, "        -u   : effective user. \n");
    fprintf(fp, "\n");

    fprintf(fp, "        -pid : specify pid file. \n");
    fprintf(fp, "        -l   : specify log file for debug. \n");
    fprintf(fp, "        -ld  : specify log directory for data. \n");
    fprintf(fp, "        -d   : debug mode. display debug information. \n");
    fprintf(fp, "        -nd  : no debug mode. nothing is displayed.\n");
    fprintf(fp, "\n");
 
    fprintf(fp, "        -ver : display Version information. \n");
    fprintf(fp, "        --help, -h : display this Help messages. \n");
    fprintf(fp, "\n");

	return;
}


