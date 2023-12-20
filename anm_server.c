/** 
  Animation Relay Server 1.1.0     

                 anm_server.c v1.1  by Fumi.Iseki (C)2011
*/

#include "xtools.h"
#include "ipaddr_tool.h"

#include "anm_server.h"
#include "anm_data.h"

pid_t  ChildPID = 0;    


void   anm_server(int usock, char* gname, char* idkey, int data_logmode)
{
    struct sockaddr_in rcv_addr;
    struct sockaddr_in snd_addr;
    struct sockaddr_in log_addr;

    char*  ipaddr     = NULL;
    char*  ipaddrnum = NULL;
    udp_header* dat  = NULL;

    unsigned short cport   = 0;
    unsigned short fps_req = 0;
    IntervalTimer  interval;

    unsigned short log_port = 0;

    struct sigaction  sa;
    struct timeval  ntime, stime;

    //
    DEBUG_MODE print_message("[%5d] ANM_SERVER: Animation Relay Server start.\n", CrntPID);

    // シグナルハンドリング
    sa.sa_handler = anm_server_term;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    set_sigterm_child(anm_server_term_child);   // Childプロセス終了時の処理設定
    DEBUG_MODE set_sigseg_handler(NULL);        // Segmentation Falt check

    //
    init_rand();
    gettimeofday(&stime, NULL);

    Buffer buf = make_Buffer(LFRAME);
    int    ttl = MaxIdleTime*30;
    tList* Relay_List = add_tList_node_bystr(NULL, 0, 0, NULL, NULL, NULL, 0);

    //
    if (data_logmode) {
        int lsock = get_valid_udp_socket(MinUdpLogPort, MaxUdpLogPort, &log_port);
        if (log_port!=0) {
            log_addr = get_sockaddr_in(LOCAL_IPADDR, log_port);
            //
            ChildPID = fork();
            if (ChildPID==0) {
                CrntPID = getpid();
                char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
                ANM_LOG_FILE print_log(LogFile, "%s [%5d] Data Log Server starts.\n", tm, CrntPID);
                free(tm);
                anm_log_server(lsock, gname);
                socket_close(lsock);
                anm_server_term(0);
            }
        }
        close(lsock);
    }

    //
    Loop {
        gettimeofday(&ntime, NULL);

        if (ntime.tv_sec-stime.tv_sec>MaxIdleTime) {
            DEBUG_MODE print_message("[%5d] ANM_SERVER: long idle time. go to die.\n", CrntPID);
            break;
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Data受信

        if (recv_wait(usock, ANM_UDP_TIMEOUT)) {
            int cc = udp_recv_Buffer_sockaddr_in(usock, &buf, &rcv_addr);
            if (cc<(int)sizeof(udp_header)) continue;

            dat = (udp_header*)buf.buf;
            ipaddr = get_ipaddr_ipv4(rcv_addr.sin_addr);
            cport  = ntohs(rcv_addr.sin_port);
            ipaddrnum = (char*)to_address_num4_ipv4(ipaddr, 0);
            //DEBUG_MODE print_message("[%5d] ANM_SERVER: Received Data from [%s:%d] key = %s\n", CrntPID, ipaddr, cport, dat->key);

            if (Allow_IPaddr!=NULL && !is_host_in_list(Allow_IPaddr, (unsigned char*)ipaddrnum, NULL)) {
                DEBUG_MODE print_message("[%5d] ANM_SERVER: WARNING: udp data from unknown node!! [%s]\n", CrntPID, ipaddr);
                freeNull(ipaddr);
                freeNull(ipaddrnum);
                continue;
            }

            if (strncmp(idkey, dat->key, ANM_COM_LEN_IDKEY)) {
                DEBUG_MODE print_message("[%5d] ANM_SERVER: WARNING: not match IDKEY of received data!! [%s]\n", CrntPID, ipaddr);
                freeNull(ipaddr);
                freeNull(ipaddrnum);
                continue;
            }
        }
        else continue;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // コマンド処理
        unsigned short rport = ntohs(dat->port);
        tList* lt = search_regist(Relay_List->next, (int)rport, ipaddr);

        // lt->ldat.id : TTL counter
        // lt->ldat.lv : Port Num of Client
        // lt->ldat.key: IP Address
        // lt->ldat.val: Animation UUID
        // lt->ldat.ptr: (IntervalTimer*) Time Interval of data sending
        // lt->ldat.sz : sizeof(IntervalTimer Transfer)

        if (dat->com[0]==ANM_COM_REQ_TRANSFER || dat->com[0]==ANM_COM_REQ_TRANSFER_LOG)
        {
            int vldsz = buf.vldsz;    // Backup

            if (lt!=NULL && !strncasecmp(dat->uuid, lt->ldat.val.buf, ANM_COM_LEN_UUID)) {
                lt->ldat.id = ttl;
                tList* pp = Relay_List->next;
                int interval = (int)(ntime.tv_usec-stime.tv_usec)/1000;
                if (interval<0) interval += 1000;
                //
                if (dat->com[0]==ANM_COM_REQ_TRANSFER_LOG) {
                    buf.vldsz = ntohs(dat->tsz);
                }
                //
                while (pp!=NULL) {
                    IntervalTimer* timer = (IntervalTimer*)(pp->ldat.ptr);
                    timer->lap = timer->lap - interval;
                    if (timer->lap<=0) {
                        snd_addr = get_sockaddr_in((char*)pp->ldat.key.buf, (unsigned short)pp->ldat.lv);
                        udp_send_Buffer_sockaddr_in(usock, &buf, &snd_addr);
                        timer->lap = timer->time;
                    }
                    pp = pp->next;
                }
            }
            
            // Log
            if (dat->com[0]==ANM_COM_REQ_TRANSFER_LOG) {
                if (log_port!=0) {
                    buf.vldsz = vldsz;
                    int sz = strlen(ipaddr) + 1;
                    char* ptr = buf.buf + buf.vldsz;
                    unsigned short* mode = (unsigned short*)ptr;

                    // add IP address info
                    mode[0] = ANM_COM_DATA_IPADDR;
                    mode[1] = sz + 4;
                    memcpy(ptr+4, ipaddr, sz);
                    buf.vldsz += sz + 4;
                        
                    unsigned short num = ntohs(dat->num);
                    num++;
                    dat->num = htons(num);
                    //
                    udp_send_Buffer_sockaddr_in(usock, &buf, &log_addr);
                }
            }
        }

        // Regist
        else if (dat->com[0]==ANM_COM_REQ_REGIST)
        {
            DEBUG_MODE print_message("[%5d] ANM_SERVER: Regist Request from %s:%d\n", CrntPID, ipaddr, cport);

            if (lt==NULL) {
                if (search_animation(Relay_List->next, dat->uuid)!=NULL) {
                    dat->com[1] = ANM_COM_REPLY_REGIST_DUPLI;
                    DEBUG_MODE print_message("[%5d] ANM_SERVER: Anim UUID is already Registered [%s]\n", CrntPID, dat->uuid);
                    char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
                    ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] Regist Request from [%s:%d] Anim UUID is already Registered [%s]\n", tm, CrntPID, idkey, ipaddr, cport, dat->uuid);
                    free(tm);
                }
                else {
                    IntervalTimer* ptr = (IntervalTimer*)malloc(sizeof(IntervalTimer));
                    memset(ptr, 0, sizeof(IntervalTimer));
                    lt = add_tList_node_bystr(Relay_List, ttl, (int)cport, ipaddr, dat->uuid, ptr, sizeof(IntervalTimer));
                    Relay_List->ldat.id++;
                    dat->com[1] = ANM_COM_REPLY_OK;

                    DEBUG_MODE {
                        print_message("==============================================\n");
                        print_tList(stderr, Relay_List);
                        print_message("==============================================\n");
                    }
                    char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
                    ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] Regist Request from [%s:%d] Registered.\n", tm, CrntPID, idkey, ipaddr, cport);
                    free(tm);
                }
            }
            //
            else {
                dat->com[1] = ANM_COM_REPLY_REGIST_ALRDY;
                DEBUG_MODE  print_message("[%5d] ANM_SERVER: Already Registered  %s:%d\n", CrntPID, ipaddr, cport);
                char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
                ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] Regist Request from [%s:%d] Already Registered.\n", tm, CrntPID, idkey, ipaddr, cport);
                free(tm);
            }

            dat->port = htons(cport);    // NAT Port
            int cc = udp_send_sockaddr_in(usock, (char*)dat, sizeof(udp_header), &rcv_addr);
            if (cc<=0) {
                if (cc<0) perror("Regist Response");
                if (lt!=NULL) {
                    del_tList_node(&lt);
                    Relay_List->ldat.id--;
                }
            }
        }

        // Delete
        else if (dat->com[0]==ANM_COM_REQ_DELETE)
        {
            if (lt!=NULL && !strncasecmp(dat->uuid, lt->ldat.val.buf, ANM_COM_LEN_UUID)) {
                DEBUG_MODE print_message("[%5d] ANM_SERVER: Delete Request from %s:%d\n", CrntPID, ipaddr, cport);
                tList* pp = Relay_List->next;
                while (pp!=NULL) {
                    snd_addr = get_sockaddr_in((char*)pp->ldat.key.buf, (unsigned short)pp->ldat.lv);
                    DEBUG_MODE print_message("[%5d] ANM_SERVER: Send Delete data to %s:%d\n", CrntPID, pp->ldat.key.buf, pp->ldat.lv);
                    udp_send_Buffer_sockaddr_in(usock, &buf, &snd_addr);
                    pp = pp->next;
                }
                char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
                ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] Delete Request from [%s:%d] Animation UUID is [%s]\n", tm, CrntPID, idkey, ipaddr, cport, dat->uuid);
                free(tm);
            }
        }

        // Logout
        else if (dat->com[0]==ANM_COM_REQ_LOGOUT)
        {
            if (lt!=NULL && !strncasecmp(dat->uuid, lt->ldat.val.buf, ANM_COM_LEN_UUID)) {
                del_tList_node(&lt);
                Relay_List->ldat.id--;
            }

            DEBUG_MODE {
                print_message("[%5d] ANM_SERVER: Logout Request from %s:%d\n", CrntPID, ipaddr, cport);
                print_message("==============================================\n");
                print_tList(stderr, Relay_List);
                print_message("==============================================\n");
            }
            char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
            ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] Logout Request from [%s:%d]\n", tm, CrntPID, idkey, ipaddr, cport);
            free(tm);
        }

        // Alive
        else if (dat->com[0]==ANM_COM_REQ_ALIVE)
        {
            if (lt!=NULL && !strncasecmp(dat->uuid, lt->ldat.val.buf, ANM_COM_LEN_UUID)) {
                DEBUG_MODE print_message("[%5d] ANM_SERVER: Keep Alive Request from [%s:%d]\n", CrntPID, ipaddr, cport);
                char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
                ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] Alive  Request from [%s:%d]\n", tm, CrntPID, idkey, ipaddr, cport);
                free(tm);

                // Reset TTL
                lt->ldat.id = ttl;
                
                // Interval Timer
                fps_req = dat->tsz;
                if (fps_req!=0) {
                    DEBUG_MODE print_message("[%5d] ANM_SERVER: Band Width Request = %dFPS from [%s:%d]\n", CrntPID, fps_req, ipaddr, cport);
                    ((IntervalTimer*)(lt->ldat.ptr))->time = 1000/fps_req;    // m sec/frame
                }
            }
        }

        // Log Data
        else if (dat->com[0]==ANM_COM_REQ_LOG || dat->com[0]==ANM_COM_REQ_LOG_START || dat->com[0]==ANM_COM_REQ_LOG_STOP)
        {
            if (log_port!=0) {
                //DEBUG_MODE print_message("[%5d] ANM_SERVER: Send Log Data to Log Server\n", CrntPID);

                int sz = strlen(ipaddr) + 1;
                char* ptr = buf.buf + buf.vldsz;
                unsigned short* mode = (unsigned short*)ptr;

                // add IP address info
                mode[0] = ANM_COM_DATA_IPADDR;
                mode[1] = sz + 4;
                memcpy(ptr+4, ipaddr, sz);
                buf.vldsz += sz + 4;
                    
                unsigned short num = ntohs(dat->num);
                num++;
                dat->num = htons(num);
                //
                udp_send_Buffer_sockaddr_in(usock, &buf, &log_addr);
            }
            //
            freeNull(ipaddr);
            freeNull(ipaddrnum);
            continue;
        }

        //
        else {
            freeNull(ipaddr);
            freeNull(ipaddrnum);
            continue;
        }

        // Check TTL
        tList* pp = Relay_List->next;
        while (pp!=NULL) {
            pp->ldat.id--;
            //
            if (pp->ldat.id<=0) {
                udp_header udphd;
                memset(&udphd, 0, sizeof(udp_header));
                udphd.com[0] = ANM_COM_REQ_DELETE;
                memcpy(udphd.uuid, pp->ldat.val.buf, ANM_COM_LEN_UUID);
                memcpy(udphd.key, idkey, ANM_COM_LEN_IDKEY);
                DEBUG_MODE print_message("[%5d] ANM_SERVER: TTL is 0 [%s:%d]\n", CrntPID, pp->ldat.key.buf, pp->ldat.lv);

                tList* pl = Relay_List->next;
                while (pl!=NULL) {
                    snd_addr = get_sockaddr_in((char*)pl->ldat.key.buf, (unsigned short)pl->ldat.lv);
                    DEBUG_MODE print_message("[%5d] ANM_SERVER: Send Delete data to %s:%d\n", CrntPID, pl->ldat.key.buf, pl->ldat.lv);
                    udp_send_sockaddr_in(usock, (char*)&udphd, sizeof(udp_header), &snd_addr);
                    pl = pl->next;
                }
                pp = del_tList_node(&pp);
                Relay_List->ldat.id--;

                char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
                ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] TTL is 0 [%s:%d] Animation UUID is [%s]\n", tm, CrntPID, idkey, pp->ldat.key.buf, pp->ldat.lv, udphd.uuid);
                free(tm);
            }
            else {
                pp = pp->next;
            }
        }

        freeNull(ipaddr);
        freeNull(ipaddrnum);

        stime = ntime;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    del_all_tList(&Relay_List);                

    if (ChildPID!=0) anm_server_kill_child(0);

    DEBUG_MODE print_message("[%5d] ANM_SERVER: down!!\n", CrntPID);
    char* tm = get_local_timestamp(time(0), "%Y/%m/%dT%H:%M:%SZ");
    ANM_LOG_FILE print_log(LogFile, "%s [%5d][%s] Relay Process is going down.\n", tm, CrntPID, idkey);
    free(tm);

    return;
}



///////////////////////////////////////////////////////////////////////////////////
//
// tList
//

/**
void del_process_list(tList* pl, int pid)

    機能：
        プロセスIDをキーにして，リストから削除
*/
void del_process_list(tList* pl, int pid)
{
    if (pl==NULL) return;
    tList* pp = pl;

    DEBUG_MODE print_message("[%5d] DEL_PROCESS_LIST: called with %d\n", CrntPID, pid);
    while (pp!=NULL) {
        if (pp->ldat.id==(int)pid) {
            del_tList_node(&pp);
            pl->ldat.id--;                // 項目の数
            break;
        }
        pp = pp->next;
    }

    return; 
}


tList*  search_regist(tList* lp, int port, char* ip)
{
    while(lp!=NULL) {
        //
        if (!strcmp(ip, lp->ldat.key.buf)) {
            if (port==lp->ldat.lv) return lp;
        }
        lp = lp->next;    
    }

    return NULL;
}


tList*  search_animation(tList* lp, char* uuid)
{
    while(lp!=NULL) {
        //
        if (!strncasecmp(uuid, lp->ldat.val.buf, ANM_COM_LEN_UUID)) {
            return lp;
        }
        lp = lp->next;    
    }

    return NULL;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Termination of Program
//

void  anm_server_term(int signal)
{
    DEBUG_MODE print_message("[%5d] ANM_SERVER_TERM: going down!!\n", CrntPID);

    if (ChildPID!=0) anm_server_kill_child(signal);
    exit(signal);
}


void  anm_server_term_child(int signal)
{
    pid_t pid = 0;
    int ret;

    DEBUG_MODE print_message("[%5d] ANM_SERVER_TERM_CHILD: called. signal = %d\n", (int)getpid(), signal);

    do {    // チャイルドプロセスの終了を待つ   
        pid = waitpid(-1, &ret, WNOHANG);
    } while(pid>0);
}


void  anm_server_kill_child(int signal)
{
    if (ChildPID!=0) {
        ignore_sigterm_child();

        kill(ChildPID, SIGINT);
        ChildPID = 0;
        anm_server_term_child(signal);
    }
}

