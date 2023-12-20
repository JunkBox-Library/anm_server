/** 
  Animation Log Server 1.0     

              anm_log_server.c v1.0  by Fumi.Iseki (C)2012
*/

#include "network.h"
#include "xtools.h"
#include "matrix.h"

#include "anm_server.h"
#include "anm_log_server.h"
#include "anm_data.h"


#define   NI_SHM_JOINT_NUM     34
#define   NI_JTXT_FILE_ID    "NI_JOINTS_TEXT_FILE"


Buffer   NiJointName[NI_SHM_JOINT_NUM];

tList* File_List = NULL;


void   anm_log_server(int usock, char* gname)
{
    int datasz;    

    udp_header* dat = NULL;
    struct sockaddr_in rcv_addr;
    struct sigaction  sa;

    vector        posVect[NI_SHM_JOINT_NUM];
    quaternion    rotQuat[NI_SHM_JOINT_NUM];
    double        jntAngl[NI_SHM_JOINT_NUM];
    int           rcvData[NI_SHM_JOINT_NUM];

    //
    DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Log Server start.\n", CrntPID);

    // シグナルハンドリング
    sa.sa_handler = anm_log_server_term;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    DEBUG_MODE set_sigseg_handler(NULL);            // Segmentation Falt check

    File_List = add_tList_node_bystr(NULL, 0, 0, NULL, NULL, NULL, 0);

    Buffer path = make_Buffer(LNAME);               // full path of log file
    Buffer srch = make_Buffer(LNAME);               // search name
    Buffer name = make_Buffer(LNAME);               // user name
    Buffer clip = make_Buffer(LEN_IPADDR);          // IP address of Client (Viewer)
    Buffer buf  = make_Buffer(LFRAME);

    Buffer ldir = make_Buffer_bystr(Data_Log_Dir);  // directory of log    file
    //
    cat_s2Buffer("/", &ldir);
    cat_s2Buffer(gname, &ldir);
    cat_s2Buffer("/", &ldir);
    mkdir((const char*)ldir.buf, 0750);

    init_joint_name();

    //
    Loop 
    {
        FILE* fp = NULL;
        int errflag = FALSE;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Data受信

        if (recv_wait(usock, ANM_UDP_TIMEOUT)) {
            int cc = udp_recv_Buffer_sockaddr_in(usock, &buf, &rcv_addr);
            if (cc<(int)sizeof(udp_header)) {
                DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Received Data size = [%d]\n", CrntPID, cc);
                continue;
            }

            dat = (udp_header*)buf.buf;
            char* ipaddr = get_ipaddr_ipv4(rcv_addr.sin_addr);
            datasz = 4;        // sizeof(float)
            //DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Received Data from [%s] key = %s\n", CrntPID, ipaddr, dat->key);
            //
            if (strcmp(ipaddr, LOCAL_IPADDR)) {
                DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: WARNING: udp data from unknown node!! [%s]\n", CrntPID, ipaddr);
                freeNull(ipaddr);
                continue;
            }
            freeNull(ipaddr);
        }
        else continue;

        //

        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        // コマンド処理

        if (dat->com[0]==ANM_COM_REQ_LOG || dat->com[0]==ANM_COM_REQ_TRANSFER_LOG)
        {
            //DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Recieved LOG Data\n", CrntPID);
            //DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Recieved LOG Data %s %s\n", CrntPID, Data_Log_Dir, dat->key);

            int len = sizeof(udp_header);
            char* ptr = (char*)dat + len;
            unsigned short* mode = NULL;

            // get name and IP address
            while (len<buf.vldsz) {
                mode = (unsigned short*)ptr;
                mode[0] &= 0x00ff;
                //
                if (mode[0]==ANM_COM_DATA_POSITION) {
                    ptr += 16;                        // datasz*4
                    len += 16;                        // datasz*4
                }
                else if (mode[0]==ANM_COM_DATA_ROTATION) {
                    ptr += 20;                        // datasz*5
                    len += 20;                        // datasz*5
                }
                else if (mode[0]==(ANM_COM_DATA_ROTATION | ANM_COM_DATA_POSITION)) {
                    ptr += 32;                        // datasz*8
                    len += 32;                        // datasz*8
                }
                else if (mode[0]==ANM_COM_DATA_NAME) {
                    copy_s2Buffer(ptr+4, &name);    // + datasz
                    ptr += mode[1];
                    len += mode[1];
                }
                else if (mode[0]==ANM_COM_DATA_LAPTIME) {
                    ptr += mode[1];
                    len += mode[1];
                }
                else if (mode[0]==ANM_COM_DATA_ANGLE) {
                    ptr += 8;                        // datasz*2;
                    len += 8;                        // datasz*2;
                }
                else if (mode[0]==ANM_COM_DATA_IPADDR) {
                    copy_s2Buffer(ptr+4, &clip);
                    ptr += mode[1];
                    len += mode[1];
                }
                else {
                    DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: ERROR: Unknown Data Type = 0x%04x\n", CrntPID, mode[0]);
                    errflag = TRUE;
                    break;
                }
            }
            if (errflag) continue;

            //
            copy_Buffer(&name, &srch);
            cat_s2Buffer("_",  &srch);
            cat_Buffer(&clip,  &srch);

            //
            tList* user = strncmp_tList(File_List, srch.buf, 0, 1);
            if (user!=NULL) {
                fp = *(FILE**)user->ldat.ptr;
            }
            if (fp==NULL) continue;

            //
            len = sizeof(udp_header);
            ptr = (char*)dat + len;
            unsigned long int laptime;
            //
            memset(posVect, 0, NI_SHM_JOINT_NUM*sizeof(vector));
            memset(rotQuat, 0, NI_SHM_JOINT_NUM*sizeof(quaternion));
            memset(jntAngl, 0, NI_SHM_JOINT_NUM*sizeof(double));
            memset(rcvData, 0, NI_SHM_JOINT_NUM*sizeof(int));

            //
            while (len<buf.vldsz) {
                mode = (unsigned short*)ptr;
                mode[0] &= 0x00ff;
                
                //
                if (mode[0]==ANM_COM_DATA_POSITION) {
                    unsigned short j = mode[1];
                    if (j<NI_SHM_JOINT_NUM) {
                        posVect[j].x = *(float*)(ptr+4);
                        posVect[j].y = *(float*)(ptr+8);
                        posVect[j].z = *(float*)(ptr+12);
                        rcvData[j] = TRUE;
                    }
                    ptr += 16;                        // datasz*4
                    len += 16;                        // datasz*4
                }
                else if (mode[0]==ANM_COM_DATA_ROTATION) {
                    unsigned short j = mode[1];
                    if (j<NI_SHM_JOINT_NUM) {
                        rotQuat[j].x = *(float*)(ptr+4);
                        rotQuat[j].y = *(float*)(ptr+8);
                        rotQuat[j].z = *(float*)(ptr+12);
                        rotQuat[j].s = *(float*)(ptr+16);
                        rcvData[j] = TRUE;
                    }
                    ptr += 20;                        // datasz*5
                    len += 20;                        // datasz*5
                }
                else if (mode[0]==(ANM_COM_DATA_ROTATION | ANM_COM_DATA_POSITION)) {
                    unsigned short j = mode[1];
                    if (j<NI_SHM_JOINT_NUM) {
                        posVect[j].x = *(float*)(ptr+4);
                        posVect[j].y = *(float*)(ptr+8);
                        posVect[j].z = *(float*)(ptr+12);
                        rotQuat[j].x = *(float*)(ptr+16);
                        rotQuat[j].y = *(float*)(ptr+20);
                        rotQuat[j].z = *(float*)(ptr+24);
                        rotQuat[j].s = *(float*)(ptr+28);
                        rcvData[j] = TRUE;
                    }
                    ptr += 32;                        // datasz*8
                    len += 32;                        // datasz*8
                }
                else if (mode[0]==ANM_COM_DATA_NAME) {
                    ptr += mode[1];
                    len += mode[1];
                }
                else if (mode[0]==ANM_COM_DATA_LAPTIME) {
                    laptime = *(unsigned long int*)(ptr+4);    // + datasz
                    ptr += mode[1];
                    len += mode[1];
                }
                else if (mode[0]==ANM_COM_DATA_ANGLE) {
                    unsigned short j = mode[1];
                    if (j<NI_SHM_JOINT_NUM) {
                        jntAngl[j] = *(float*)(ptr+4);
                        //rcvData[j] = TRUE;
                    }
                    ptr += 8;                        // datasz*2;
                    len += 8;                        // datasz*2;
                }
                else if (mode[0]==ANM_COM_DATA_IPADDR) {
                    ptr += mode[1];
                    len += mode[1];
                }
                else {
                    DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: ERROR: Unknown Data Type = 0x%04x\n", CrntPID, mode[0]);
                    errflag = TRUE;
                    break;
                }
            }
            if (errflag) continue;

            //
            int j;
            fprintf(fp, "%d\n", laptime);
            for (j=0; j<NI_SHM_JOINT_NUM; j++) {
                if (rcvData[j]) {
                    fprintf(fp, " %-10s %11.6f %11.6f %11.6f  ", NiJointName[j].buf, posVect[j].x, posVect[j].y, posVect[j].z);
                    fprintf(fp, "    %11.8f %11.8f %11.8f %11.8f", rotQuat[j].x, rotQuat[j].y, rotQuat[j].z, rotQuat[j].s);
                    fprintf(fp, "    %11.6f", jntAngl[j]*RAD2DEGREE);
                    fprintf(fp, "\n");
                }
            }
        }

        //
        else if (dat->com[0]==ANM_COM_REQ_LOG_START)
        {
            DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Recieved LOG START\n", CrntPID);

            int len = sizeof(udp_header);
            char* ptr = (char*)dat + len;
            unsigned short* mode = NULL;

            //
            while (len<buf.vldsz) {
                mode = (unsigned short*)ptr;
                //
                if (mode[0]==ANM_COM_DATA_NAME) {
                    copy_s2Buffer(ptr+4, &name);        // + datasz
                    copy_Buffer(&ldir, &path);
                    cat_Buffer(&name,  &path);
                    cat_s2Buffer("/",  &path);
                    mkdir((const char*)path.buf, 0750);

                    //cat_s2Buffer(get_localtime('-', 'T', ':', 'Z'), &path);    
                    cat_s2Buffer(get_local_timestamp(time(0), "%Y/"), &path);    
                    cat_s2Buffer(".txt", &path);    
                    DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Open File = %s\n", CrntPID, path.buf);
                    fp = fopen((char*)path.buf, "w");
                    if (fp==NULL) DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Fail to Open File\n", CrntPID);

                    ptr += mode[1];
                    len += mode[1];
                }
                else if (mode[0]==ANM_COM_DATA_IPADDR) {
                    copy_s2Buffer(ptr+4, &clip);
                    ptr += mode[1];
                    len += mode[1];
                }
                else {
                    DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: ERROR: Unknown Data Type = 0x%04x\n", CrntPID, mode[0]);
                    errflag = TRUE;
                    break;
                }
            }
            if (fp==NULL) continue;
            if (errflag) {
                fclose(fp);
                continue;
            }

            //
            copy_Buffer(&name, &srch);
            cat_s2Buffer("_",  &srch);
            cat_Buffer(&clip,  &srch);

            add_tList_node_bystr(File_List, 0, 0, (char*)srch.buf, NULL, (void*)&fp, sizeof(FILE*));
            File_List->ldat.id++;
            //
            DEBUG_MODE {
                fprintf(stderr, "++++++++++++++++++++++++++++++++++++++++++++++\n");
                print_tList(stderr, File_List);
                fprintf(stderr, "++++++++++++++++++++++++++++++++++++++++++++++\n");
            }
            //
            fprintf(fp, "%s\n", NI_JTXT_FILE_ID);
            fprintf(fp, "%s, %s\n", name.buf, clip.buf);
        }

        //
        else if (dat->com[0]==ANM_COM_REQ_LOG_STOP) 
        {
            DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Recieved LOG STOP\n", CrntPID);

            int len = sizeof(udp_header);
            char* ptr = (char*)dat + len;
            unsigned short* mode = (unsigned short*)ptr;
            //
            while (len<buf.vldsz) {
                mode = (unsigned short*)ptr;
                //
                if (mode[0]==ANM_COM_DATA_NAME) {
                    copy_s2Buffer(ptr+4, &name);
                    ptr += mode[1];
                    len += mode[1];
                }
                else if (mode[0]==ANM_COM_DATA_IPADDR) {
                    copy_s2Buffer(ptr+4, &clip);
                    ptr += mode[1];
                    len += mode[1];
                }
            }

            //
            copy_Buffer(&name, &srch);
            cat_s2Buffer("_",  &srch);
            cat_Buffer(&clip,  &srch);

            tList* user = strncmp_tList(File_List, srch.buf, 0, 1);
            if (user!=NULL) {
                fp = *(FILE**)user->ldat.ptr;
                if (fp!=NULL) fclose(fp);
                del_tList_node(&user);
                File_List->ldat.id--;
                //
                DEBUG_MODE {
                    fprintf(stderr, "++++++++++++++++++++++++++++++++++++++++++++++\n");
                    print_tList(stderr, File_List);
                    fprintf(stderr, "++++++++++++++++++++++++++++++++++++++++++++++\n");
                }
            }
            else {

            }
        }

        //
        else {
            DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: Recieved Unknown Data (%02x)\n", CrntPID, dat->com[0]);
        }
    }

    /////////////////////////////////////////////////////////////////////////
    //
    if (File_List!=NULL) del_all_tList(&File_List);

    DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER: down!!\n", CrntPID);
    return;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Termination of Program
//

void  anm_log_server_term(int signal)
{
    DEBUG_MODE print_message("[%5d] ANM_LOG_SERVER_TERM: going down!!\n", CrntPID);

    if (File_List!=NULL) del_all_tList(&File_List);
    clear_joint_name();

    exit(signal);
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Joint Name
//

/*
共有メモリ名
    mPelvis(0), mTorso(1), mChest(2), mNeck(3), mHead(4), mSkull(5), 
    mEyeLeft(6), mEyeRight(7), mBustLeft(8), mBustRight(9),
    mCollarLeft (10), mShoulderLeft (11), mElbowLeft (12), mWristLeft (13), mFingertipLeft (14),
    mCollarRight(15), mShoulderRight(16), mElbowRight(17), mWristRight(18), mFingertipRight(19),
    mHipLeft (20), mKneeLeft (21), mAnkleLeft (22), mFootLeft (23), mToeLeft (24),
    mHipRight(25), mKneeRight(26), mAnkleRight(27), mFootRight(28), mToeRight(29), 
    L_Hand(30), R_Hand(31), Expression(32), Avatar(33)
*/
void  init_joint_name(void)
{
     NiJointName[ 0] = make_Buffer_bystr("PELVIS");
     NiJointName[ 1] = make_Buffer_bystr("TORSO");
     NiJointName[ 2] = make_Buffer_bystr("CHEST");
     NiJointName[ 3] = make_Buffer_bystr("NECK");
     NiJointName[ 4] = make_Buffer_bystr("HEAD");
     NiJointName[ 5] = make_Buffer_bystr("SKULL");
     NiJointName[ 6] = make_Buffer_bystr("L_EYE");
     NiJointName[ 7] = make_Buffer_bystr("R_EYE");
     NiJointName[ 8] = make_Buffer_bystr("L_BUST");
     NiJointName[ 9] = make_Buffer_bystr("R_BUST");
     NiJointName[10] = make_Buffer_bystr("L_COLLAR");
     NiJointName[11] = make_Buffer_bystr("L_SHLDR");
     NiJointName[12] = make_Buffer_bystr("L_ELBOW");
     NiJointName[13] = make_Buffer_bystr("L_WRIST");
     NiJointName[14] = make_Buffer_bystr("L_FNGRTIP");
     NiJointName[15] = make_Buffer_bystr("R_COLLAR");
     NiJointName[16] = make_Buffer_bystr("R_SHLDR");
     NiJointName[17] = make_Buffer_bystr("R_ELBOW");
     NiJointName[18] = make_Buffer_bystr("R_WRIST");
     NiJointName[19] = make_Buffer_bystr("R_FNGRTIP");
     NiJointName[20] = make_Buffer_bystr("L_HIP");
     NiJointName[21] = make_Buffer_bystr("L_KNEE");
     NiJointName[22] = make_Buffer_bystr("L_ANKLE");
     NiJointName[23] = make_Buffer_bystr("L_FOOT");
     NiJointName[24] = make_Buffer_bystr("L_TOE");
     NiJointName[25] = make_Buffer_bystr("R_HIP");
     NiJointName[26] = make_Buffer_bystr("R_KNEE");
     NiJointName[27] = make_Buffer_bystr("R_ANKLE");
     NiJointName[28] = make_Buffer_bystr("R_FOOT");
     NiJointName[29] = make_Buffer_bystr("R_TOE");
     NiJointName[30] = make_Buffer_bystr("L_HAND");
     NiJointName[31] = make_Buffer_bystr("R_HAND");
     NiJointName[32] = make_Buffer_bystr("FACE");
     NiJointName[33] = make_Buffer_bystr("AVATAR");
}


void  clear_joint_name(void)
{
    int j;
    for (j=0; j<NI_SHM_JOINT_NUM; j++) {
        free_Buffer(&NiJointName[j]);
    }
}
