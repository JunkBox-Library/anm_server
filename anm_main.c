/*	
	Animation Relay Server Main 1.1.0

									by Fumi.Iseki (C)2011
*/


#include "password.h"
#include "ipaddr_tool.h"

#include "anm_data.h"
#include "anm_server.h"



int		UDPSock 			= 0;
pid_t	RootPID 			= 0;
tList* 	Process_List 		= NULL;

char*	LogFileName			= "anm_server.log";


// External
pid_t   CrntPID 			= 0;

int		MinUdpDataPort		= 8201;
int		MaxUdpDataPort		= 8299;
int		MaxIdleTime			= 300;		// sec

tList* 	Allow_IPaddr		= NULL;
FILE* 	LogFile				= NULL;
char*	GrpKeyPrefix		= NULL;



#ifdef CYGWIN
	char*  Hosts_Allow_File = "hosts_access.allow";
	char*  Temp_File_Dir	= "./";
	char*  Data_Log_Dir		= "./datalog/";
#else
	char*  Hosts_Allow_File = "/usr/local/etc/anm_server/hosts_access.allow";
	char*  Temp_File_Dir	= "/var/anm_server/";
	char*  Data_Log_Dir		= "/var/anm_server/datalog/";
#endif




int main(int argc, char** argv)
{
	int  i, vport = 0;
	int  logmode = OFF;
	int  data_logmode = OFF;

	char*  tempfile;
	struct passwd* pw;
	struct sigaction sa;
	struct sockaddr_in cl_addr;

	Buffer username, conffile, pidfile, logfile, datalog;
	pid_t  pid;


#ifdef CYGWIN
	DebugMode = ON;
	data_logmode = ON;
#else
	DebugMode = OFF;
#endif


	// 引数処理
	username  = make_Buffer(LNAME);
	conffile  = make_Buffer(LNAME);
	pidfile	  = make_Buffer(LNAME);
	logfile   = make_Buffer(LNAME);
	datalog   = make_Buffer(LNAME);

	for (i=1; i<argc; i++) {
		if		(!strcmp(argv[i],"-p"))   {if (i!=argc-1) vport = (unsigned short)atoi(argv[i+1]);}
		else if (!strcmp(argv[i],"-f"))	  {if (i!=argc-1) copy_s2Buffer(argv[i+1], &conffile);}
		else if (!strcmp(argv[i],"-u"))   {if (i!=argc-1) copy_s2Buffer(argv[i+1], &username);}

		else if (!strcmp(argv[i],"-pid")) {if (i!=argc-1) copy_s2Buffer(argv[i+1], &pidfile);}
		else if (!strcmp(argv[i],"-l"))   {if (i!=argc-1 && *(argv[i+1])!='-') copy_s2Buffer(argv[i+1], &logfile); logmode=ON;}
		else if (!strcmp(argv[i],"-ld"))  {if (i!=argc-1 && *(argv[i+1])!='-') copy_s2Buffer(argv[i+1], &datalog); data_logmode=ON;}
		else if (!strcmp(argv[i],"-d"))   DebugMode = ON;
		else if (!strcmp(argv[i],"-nd"))  DebugMode = OFF;

		else if (!strcmp(argv[i],"--ver"))  { fprintf(stdout, "\n%s\n\n", ANM_SERVER_VERSION); exit(0);}
		else if (!strcmp(argv[i],"--help")) { anm_help(stdout); exit(0);}
		else if (!strcmp(argv[i],"-h"))     { anm_help(stdout); exit(0);}
	}


	CrntPID = RootPID = getpid();
	if (vport==0) vport = ANM_SERVER_PORT;

	//
	init_rand();
	Process_List = add_tList_node_bystr(NULL, 0, 0, TLIST_ANCHOR, NULL, NULL, 0);



///////////////////////////////////////////////////////////////////////////

	// 設定ファイルの読み込み
	if (conffile.buf[0]=='\0') copy_s2Buffer(ANM_CONFIG_FILE, &conffile);
	read_config_file((char*)conffile.buf);
	free_Buffer(&conffile);


	// PIDファイルの作成
	if (pidfile.buf[0]!='\0') {
		FILE* fp = fopen((char*)pidfile.buf, "w");
		if (fp!=NULL) {
			fprintf(fp, "%d", (int)RootPID);
			fclose(fp);
		}
	}
	free_Buffer(&pidfile);

	DEBUG_MODE print_message("[%5d] ANM_MAIN: root PID is [%d]\n", CrntPID, CrntPID);


	// ACLファイルの読み込み
	Allow_IPaddr = read_ipaddr_file(Hosts_Allow_File);
	if (Allow_IPaddr!=NULL) {
		DEBUG_MODE {
			print_message("[%5d] ANM_MAIN: readed access control list.\n", RootPID);
			print_message("============================================\n");
  			print_address_in_list(stderr, Allow_IPaddr);
			print_message("============================================\n");
		}
	}
	else {
		DEBUG_MODE print_message("[%5d] ANM_MAIN: cannot read access contorol list. no access control.\n", CrntPID);
	}


	// 実効ユーザの変更
	if (username.buf[0]!='\0') {
		int uerr = -1;
		int gerr = -1;
   
		DEBUG_MODE print_message("[%5d] ANM_MAIN: change effective user to [%s]\n", CrntPID, username.buf);
		if (isdigit(username.buf[0])) {
			gerr = 0;
			uerr = seteuid(atoi((char*)username.buf));
		}
		else {
			pw = getpwnam((char*)username.buf);
			if (pw!=NULL) {
				gerr = setegid(pw->pw_gid);
				uerr = seteuid(pw->pw_uid);
			}
		}
		if (gerr==-1) {
			DEBUG_MODE print_message("[%5d] ANM_MAIN: WARNING: cannot change effectinve group.\n", CrntPID);
		}
		if (uerr==-1) {
			DEBUG_MODE print_message("[%5d] ANM_MAIN: WARNING: cannot change effectinve user [%s]\n", CrntPID, username.buf);
		}
	}
	free_Buffer(&username);


	// シグナルハンドリング
	sa.sa_handler = anm_sigterm_process;
	sa.sa_flags   = 0;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGCHLD);
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGHUP,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	set_sigterm_child(anm_sigterm_child);	// Childプロセス終了時の処理設定
	DEBUG_MODE set_sigseg_handler(NULL);	// Segmentation Falt check


	// ログファイルの書き込みチェック
	if (logmode) {
		if (logfile.vldsz<=0) {
			copy_s2Buffer(Temp_File_Dir, &logfile);
			cat_s2Buffer (LogFileName, &logfile);
		}
		
		LogFile = fopen((char*)logfile.buf, "a");
		if (LogFile==NULL) {
			print_message("ANM_MAIN: WARNING: anm_server cannot write logfile [%s]\n", logfile.buf);
		}
		free_Buffer(&logfile);
	}


	// データログディレクトリの書き込みチェック
	if (data_logmode) {
		if (datalog.vldsz>0) {
			Data_Log_Dir = datalog.buf;
		}
		else {
			copy_s2Buffer(Data_Log_Dir, &datalog);
		}
		mkdir((const char*)datalog.buf, 0750);
		
		Buffer temp = dup_Buffer(datalog);
		cat_s2Buffer("/.write_check", &temp);
		FILE* tempfp = fopen((char*)temp.buf, "a");
		if (tempfp!=NULL) {
			fclose(tempfp);
			unlink(temp.buf);
		}
		else {
			print_message("ANM_MAIN: WARNING: anm_server cannot write data log directory [%s]\n", datalog.buf);
		}
		free_Buffer(&temp);
	}



	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// メインループ
    DEBUG_MODE print_message("[%5d] ANM_MAIN: Animation Realy Server started. Port = %d\n", CrntPID, vport);
	ANM_LOG_FILE print_log(LogFile, "%s [%5d] Animation Realy Server started with port = %d\n", get_localtime('-', 'T', ':', 'Z'), CrntPID, vport);

	UDPSock = udp_server_socket_ipv4((int)vport);
	if (UDPSock<0) Error("udp_server_socket");

	udp_header dat;
	char* ipaddr	= NULL;
	char* ipaddrnum = NULL;

	Loop 
	{
		if (recv_wait(UDPSock, ANM_UDP_TIMEOUT)) 
		{
			int cc = udp_recv_sockaddr_in(UDPSock, (char*)&dat, sizeof(udp_header), &cl_addr);
        	if (cc<(int)sizeof(udp_header)) continue;

			ipaddr    = get_ipaddr_ipv4(cl_addr.sin_addr);
			ipaddrnum = (char*)to_address_num4_ipv4(ipaddr, 0);

			// IP Address check
			if (Allow_IPaddr!=NULL && !is_host_in_list(Allow_IPaddr, (unsigned char*)ipaddrnum, NULL)) {
				print_message("[%5d] ANM_MAIN: ERROR: not allowed access from [%s]\n", CrntPID, ipaddr);
				freeNull(ipaddr);
				freeNull(ipaddrnum);

				dat.com[1] = ANM_COM_REPLY_FORBIDDEN;
				udp_send_sockaddr_in(UDPSock, (char*)&dat, sizeof(udp_header), &cl_addr);
				continue;
			} 

			// Group Prefix Key Check
			if (GrpKeyPrefix!=NULL) {
				int len = strlen(GrpKeyPrefix);
				if (len>0 && strncmp(dat.key, GrpKeyPrefix, len)) {
					dat.com[1] = ANM_COM_REPLY_FORBIDDEN;
					udp_send_sockaddr_in(UDPSock, (char*)&dat, sizeof(udp_header), &cl_addr);
					continue;
				}
			}
		}
		else continue;


		if (dat.com[0]==ANM_COM_REQ_LOGIN) 
		{
    		DEBUG_MODE print_message("[%5d] ANM_MAIN: Login Request from [%s:%d]. Group is %s\n", CrntPID, ipaddr, ntohs(cl_addr.sin_port), dat.key);

			tList* access = strncmp_tList(Process_List, dat.key, ANM_COM_LEN_IDKEY, 1);
			if (access!=NULL) {
				dat.com[1] = ANM_COM_REPLY_OK;
				unsigned short port = (unsigned short)access->ldat.lv;
				dat.port = htons(port);
				memcpy(dat.key, access->ldat.val.buf, ANM_COM_LEN_IDKEY);

				udp_send_sockaddr_in(UDPSock, (char*)&dat, sizeof(udp_header), &cl_addr);
				ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] Login  Request from [%s]\n", get_localtime('-', 'T', ':', 'Z'), CrntPID, dat.key, ipaddr);
			}
			//
			else {
				unsigned short int port;
				int sock = get_valid_udp_socket(MinUdpDataPort, MaxUdpDataPort, &port);

				if (port!=0) {
					char* idkey = random_str(ANM_COM_LEN_IDKEY);
					//
					pid = fork();
					if (pid==0) {
						CrntPID = getpid();
						ANM_LOG_FILE print_log(LogFile, "%s [%5d] Login Request from [%s] Group is cretaed [%s]->[%s]\n", 
																					get_localtime('-', 'T', ':', 'Z'), CrntPID, ipaddr, dat.key, idkey);
						freeNull(ipaddr);
						freeNull(ipaddrnum);

						anm_server(sock, dat.key, idkey, data_logmode);
						free(idkey);
						socket_close(sock);
						anm_sigterm_process(0);
					}

					add_tList_node_bystr(Process_List, (int)pid, (int)port, dat.key, idkey, NULL, 0);
					Process_List->ldat.id++;

    				DEBUG_MODE print_message("[%5d] ANM_MAIN: Relay Server Port = %d, Access Key = %s\n", CrntPID, port, idkey);
					dat.com[1] = ANM_COM_REPLY_OK;
					dat.port = htons(port);
					memcpy(dat.key, idkey, ANM_COM_LEN_IDKEY);
					free(idkey);

					udp_send_sockaddr_in(UDPSock, (char*)&dat, sizeof(udp_header), &cl_addr);

            		DEBUG_MODE {
                		print_message("[%5d] ANM_MAIN: added UDP Relay Process.\n", CrntPID);
                		print_message("==============================================\n");
                		print_tList(stderr, Process_List);
                		print_message("==============================================\n");
            		}
				}
				else {
					dat.com[1] = ANM_COM_REPLY_NG;
					udp_send_sockaddr_in(UDPSock, (char*)&dat, sizeof(udp_header), &cl_addr);
					ANM_LOG_FILE print_log(LogFile, "%s [%5d] Login Request from [%s] But create Group error [%s]\n", 
																				get_localtime('-', 'T', ':', 'Z'), CrntPID, ipaddr, dat.key);
				}

				close(sock);	// Do not use socket_close()
			}
		}

		else 
		{
			dat.com[1] = ANM_COM_REPLY_ERROR;
			udp_send_sockaddr_in(UDPSock, (char*)&dat, sizeof(udp_header), &cl_addr);
		}

		freeNull(ipaddr);
		freeNull(ipaddrnum);
	}


	// not reachable
	socket_close(UDPSock);
	UDPSock = 0;

	anm_sigterm_process(0);
}







//////////////////////////////////////////////////////////////////////////
// プログラムの終了
//
void  anm_sigterm_process(int signal)
{  
	int ret;
	tList* pl = NULL;
	pid_t pid, cpid;

	pid = getpid();
	DEBUG_MODE print_message("[%5d] ANM_SIGTERM_PROCESS: going down!!\n", CrntPID);

	ignore_sigterm_child();

	pl = Process_List->next;
	while(pl!=NULL) {
		if (pl->ldat.id>0) {
			DEBUG_MODE print_message("[%5d] ANM_SIGTERM_PROCESS: send SIGINT to [%d]\n", CrntPID, pl->ldat.id);
			kill((pid_t)pl->ldat.id, SIGINT);
		}
		pl = pl->next;
	}
	del_all_tList(&Process_List);
	
	do {	 		// チャイルドプロセスの終了を待つ   
		cpid = waitpid(-1, &ret, WNOHANG);
	} while(cpid>0);


	// Root Process
	if (pid==RootPID) {
		DEBUG_MODE print_message("[%5d] ANM_SIGTERM_PROCESS: root process is going down.\n", CrntPID);

		ANM_LOG_FILE {
			print_log(LogFile, "%s [%5d] Animation Realy Server is going down\n", get_localtime('-', 'T', ':', 'Z'), CrntPID);
			fflush(LogFile);
			fclose(LogFile);
			LogFile = NULL;
		}

		if (UDPSock>0) socket_close(UDPSock);
		UDPSock = 0;
	}

	exit(signal);
}





/**			   
void  anm_sigterm_child(int signal)
				  
	機能：child プロセス終了時の処理
 
*/				
void  anm_sigterm_child(int signal)
{				
	pid_t pid = 0;
	int ret;
				  
	DEBUG_MODE print_message("[%5d] ANM_SIGTERM_CHILD: called. signal = %d\n", (int)getpid(), signal);

	do {	// チャイルドプロセスの終了を待つ   
		pid = waitpid(-1, &ret, WNOHANG);
		if (pid>0 && Process_List!=NULL) {
			del_process_list(Process_List, (int)pid);
		}
	} while(pid>0);

	DEBUG_MODE {
   		print_message("[%5d] ANM_SIGTERM_CHILD: Delete Relay Process.\n", CrntPID);
   		print_message("==============================================\n");
   		print_tList(stderr, Process_List);
   		print_message("==============================================\n");
   	}
}



