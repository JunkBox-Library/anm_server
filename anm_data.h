/**
    Second Life/OpenSim Animation Relay Server: Data define Header File

                            anm_data.h v1.1  by Fumi.Iseki (C)2011
*/

#ifndef  __ANM_SERVER_DATA_H_
#define  __ANM_SERVER_DATA_H_



#define  ANM_SERVER_PORT            8200



//
//
#define  ANM_COM_NUM_DATA            8
#define  ANM_COM_LEN_DATA           64		// sizeof(double)*ANM_COM_NUM_DATA
#define  ANM_COM_LEN_POS            24
#define  ANM_COM_LEN_ROT            32

#define  ANM_COM_LEN_COMMAND         2
#define  ANM_COM_LEN_IDKEY          16
#define  ANM_COM_LEN_UUID           36
#define  ANM_COM_LEN_PASS           32

#define  ANM_COM_LEN_NAME			15		// 



typedef struct _udp_header {
    unsigned char  com [ANM_COM_LEN_COMMAND];	//  2	コマンド
	unsigned short msec;						//  2 	タイマー
    char		   key [ANM_COM_LEN_IDKEY+1];	// 17	グループキー
    char		   uuid[ANM_COM_LEN_UUID+1];	// 37	アニメーションUUID
    unsigned short port;						//  2	ポート番号/識別用番号
    unsigned short num;							//  2	データの数
    unsigned short tsz;							//  2	汎用
} udp_header;									// ---> 64



#define  ANM_COM_DATA_POSITION		0x01
#define  ANM_COM_DATA_ROTATION		0x02
#define  ANM_COM_DATA_ANGLE			0x04
#define  ANM_COM_DATA_NAME			0x10
//#define  ANM_COM_DATA_TITLE		0x20
#define  ANM_COM_DATA_IPADDR		0x40
#define  ANM_COM_DATA_LAPTIME		0x80



// com[0] に設定 REQUEST 
#define  ANM_COM_REQ_LOGIN          0x01
#define  ANM_COM_REQ_LOGOUT         0x02
#define  ANM_COM_REQ_REGIST         0x03
#define  ANM_COM_REQ_TRANSFER       0x04
#define  ANM_COM_REQ_DELETE         0x05
#define  ANM_COM_REQ_ALIVE          0x06
#define  ANM_COM_REQ_TRANSFER_LOG	0x07

#define  ANM_COM_REQ_LOG			0x11
#define  ANM_COM_REQ_LOG_START		0x12
#define  ANM_COM_REQ_LOG_STOP		0x13


// com[1] に設定  REPLY
#define  ANM_COM_REPLY_ERROR        0x00
#define  ANM_COM_REPLY_OK           0x01
#define  ANM_COM_REPLY_NG           0x02
#define  ANM_COM_REPLY_FORBIDDEN    0x03
#define  ANM_COM_REPLY_REGIST_ALRDY 0x10
#define  ANM_COM_REPLY_REGIST_DUPLI 0x11
#define  ANM_COM_REPLY_TIMEOUT      0xff


// ERROR
#define  ANM_COM_ERROR_SOCKET       0xf0
#define  ANM_COM_ERROR_SERVER_DOWN  0xf1
#define  ANM_COM_ERROR_TIMEOUT      0xff



#endif

